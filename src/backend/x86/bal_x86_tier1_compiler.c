#include "backend/x86/bal_x86_tier1_compiler.h"
#include "backend/bal_cpu.h"
#include "bal_decoder.h"
#include "bal_engine.h"
#include "bal_engine_flags.h"

#include <string.h>

// Set which registers holds the first argument passed to the JIT block based on the OS calling
// convention.
#if BAL_PLATFORM_WINDOWS

#define BAL_X86_ABI_ARG1 BAL_X86_RCX

#else

#define BAL_X86_ABI_ARG1 BAL_X86_RDI

#endif

// Scratch registers for the Greedy Allocator. We avoid RDI and RSI since they are often used for
// the argument passing.
//
static const bal_x86_register_t SCRATCH_REGISTERS[]
    = { BAL_X86_RAX, BAL_X86_RCX, BAL_X86_RDX, BAL_X86_R8, BAL_X86_R9, BAL_X86_R10 };

#define SCRATCH_REGISTERS_SIZE (sizeof(SCRATCH_REGISTERS) / sizeof(SCRATCH_REGISTERS[0]))

static void               reset_register_allocator(bal_tier1_compiler_t *compiler);
static bal_x86_register_t allocate_x86_register(bal_tier1_compiler_t *compiler,
                                                uint8_t               arm_register,
                                                bool                  skip_load_instruction);
static void               flush_dirty_registers(bal_tier1_compiler_t *compiler);
static void               terminate_block(bal_tier1_compiler_t *compiler,
                                          bal_guest_address_t   next_pc,
                                          bool                  is_pc_dynamic,
                                          size_t                arm_instruction_count,
                                          uint32_t              engine_flags);
static uint32_t extract_operand_value(uint32_t instruction, const bal_decoder_operand_t *operand);
static void     translate_jump(bal_tier1_compiler_t                     *compiler,
                               const bal_decoder_instruction_metadata_t *metadata,
                               const uint32_t                            instruction,
                               const bal_guest_address_t                 guest_address,
                               bal_guest_address_t                      *target_pc);
static void     translate_mov(bal_tier1_compiler_t                     *compiler,
                              const bal_decoder_instruction_metadata_t *metadata,
                              const uint32_t                            instruction);
static void     translate_mov_orr(bal_tier1_compiler_t                     *compiler,
                                  const bal_decoder_instruction_metadata_t *metadata,
                                  const uint32_t                            instruction);
static void     handle_orr_immediate(bal_tier1_compiler_t                     *compiler,
                                     const bal_decoder_instruction_metadata_t *metadata,
                                     const uint32_t                            instruction);
static void     handle_orr_shifted_register(bal_tier1_compiler_t                     *compiler,
                                             const bal_decoder_instruction_metadata_t *metadata,
                                             const uint32_t                            instruction);
BAL_HOT static void emit_orr_shifted_raw(bal_tier1_compiler_t             *compiler,
                                      const bal_x86_register_t         x86_rd,
                                      const bal_x86_register_t         x86_rn,
                                      const bal_x86_register_t         x86_rm,
                                      const uint32_t                   imm6,
                                      const uint32_t                   shift_type,
                                      const int32_t                    datasize,
                                      const bool                       has_x86_rn);

static uint64_t decode_bitmask_immediate(const uint32_t N,
                                         const uint32_t immr,
                                         const uint32_t imms,
                                         const int32_t  datasize);
bal_error_t
bal_tier1_compiler_init(bal_tier1_compiler_t         *compiler,
                        const bal_executable_buffer_t executable_buffer,
                        const size_t                  buffer_size,
                        const bal_logger_t            logger)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: compiler is NULL");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(NULL == executable_buffer.rw_pointer)
        || BAL_UNLIKELY(NULL == executable_buffer.rx_pointer))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: executable_buffer is NULL");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return compiler->status;
    }

    if (BAL_UNLIKELY(0 == buffer_size))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: buffer_size is 0");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return compiler->status;
    }

    compiler->logger = logger;
    bal_error_t error
        = bal_x86_assembler_init(&compiler->assembler, executable_buffer, buffer_size, logger);

    if (BAL_UNLIKELY(error != BAL_SUCCESS))
    {
        return error;
    }

    error = bal_sliding_window_init(&compiler->window, &compiler->assembler);

    if (BAL_UNLIKELY(error != BAL_SUCCESS))
    {
        return error;
    }

    return BAL_SUCCESS;
}

void
bal_tier1_compiler_reset(bal_tier1_compiler_t *compiler)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return;
    }

    compiler->status = BAL_SUCCESS;
    reset_register_allocator(compiler);
    bal_sliding_window_reset(&compiler->window);
    bal_x86_assembler_reset(&compiler->assembler);
}

void *
bal_tier1_compiler_translate(bal_tier1_compiler_t         *compiler,
                             const bal_memory_interface_t *memory_interface,
                             bal_guest_address_t           guest_address,
                             const size_t                  max_instructions,
                             const uint32_t                engine_flags)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return NULL;
    }

    const bal_logger_t *logger = &compiler->logger;

    if (BAL_UNLIKELY(NULL == memory_interface))
    {
        BAL_LOG_ERROR(logger, "Aborting function: memory_interface is NULL");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return NULL;
    }

    if (BAL_UNLIKELY(NULL == memory_interface->translate))
    {
        BAL_LOG_ERROR(logger, "Aborting function: memory_interface->translate is NULL");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return NULL;
    }

    if (BAL_UNLIKELY(0 == max_instructions))
    {
        BAL_LOG_INFO(logger, "Aborting function: max Instructions is 0");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return NULL;
    }

    BAL_LOG_INFO(logger, "Starting Tier 1 JIT Compilation");
    BAL_LOG_INFO(logger,
                 "GVA: 0x%016llX | Max Instructions: %zu",
                 (unsigned long long)guest_address,
                 max_instructions);
    reset_register_allocator(compiler);
    void *host_address = compiler->assembler.rx_buffer + compiler->assembler.offset;

    // Setup block.
    //
    bal_x86_emit_push_r64(&compiler->assembler, BAL_X86_RBP);
    bal_x86_emit_mov_r64_r64(&compiler->assembler, BAL_X86_RBP, BAL_X86_ABI_ARG1);

    bool   is_block_terminated   = false;
    bool   is_pc_dynamic         = false;
    size_t arm_instruction_count = 0;

    while (false == is_block_terminated)
    {
        size_t          max_readable_instructions_bytes = 0;
        const uint32_t *host_address_base = (const uint32_t *)memory_interface->translate(
            (void *)memory_interface, guest_address, &max_readable_instructions_bytes);

        if (BAL_UNLIKELY(NULL == host_address_base))
        {
            BAL_LOG_ERROR(logger,
                          "Aborting function: memory translation fault at GVA 0x%016llX",
                          (unsigned long long)guest_address);
            compiler->status = BAL_ERROR_MEMORY_FAULT;
            break;
        }

        const uint32_t *BAL_RESTRICT host_address_cursor = host_address_base;
        size_t max_readable_instructions = max_readable_instructions_bytes / sizeof(uint32_t);

        // Has the remaining instructions crossed a memory page boundary? This should not happen on
        // ARM64 since instructions are strictly 4 byte aligned. So throw an error if this
        // happens.
        //
        if (BAL_UNLIKELY(0 == max_readable_instructions))
        {
            BAL_LOG_ERROR(logger,
                          "Aborting function: insufficient memory at GVA 0x%016llX. Need 4 bytes "
                          "for an instruction, but only %zu bytes are readable",
                          (unsigned long long)guest_address,
                          max_readable_instructions_bytes);
            compiler->status = BAL_ERROR_PC_ALIGNMENT;
            break;
        }

        if (max_readable_instructions > max_instructions)
        {
            max_readable_instructions = max_instructions;
        }

        if (engine_flags & BAL_ENGINE_FLAG_STRICT_ALIGNMENT)
        {
            if (guest_address % 4 != 0)
            {
                BAL_LOG_ERROR(logger,
                              "Aborting function: strict alignment fault at GVA 0x%016llX",
                              (unsigned long)guest_address);
                compiler->status = BAL_ERROR_PC_ALIGNMENT;
                break;
            }
        }

        for (size_t i = 0; i < max_readable_instructions; ++i)
        {
            const uint32_t                            instruction = *host_address_cursor;
            const bal_decoder_instruction_metadata_t *metadata    = bal_decode_arm64(instruction);

            if (BAL_UNLIKELY(NULL == metadata))
            {
                BAL_LOG_ERROR(logger,
                              "Aborting function: failed to decode instruction %08X at GVA 0x%llX",
                              instruction,
                              (unsigned long long)guest_address);
                is_block_terminated = false;
                break;
            }

            BAL_LOG_TRACE(logger,
                          "[0X%016llX] %08x : %s",
                          (unsigned long long)guest_address,
                          instruction,
                          metadata->name);

            if (engine_flags & BAL_ENGINE_FLAG_TRAP_SVC)
            {
                if (BAL_UNLIKELY(0 == strncmp(metadata->name, "SVC", 3)))
                {
                    BAL_LOG_INFO(logger,
                                 "Trapping on SVC instruction at 0x%016llX",
                                 (unsigned long long)guest_address);
                    is_block_terminated = true;
                    break;
                }
            }

            if (engine_flags & BAL_ENGINE_FLAG_TRAP_SMC_HVC)
            {
                if (BAL_UNLIKELY((0 == strncmp(metadata->name, "SMC", 3)
                                  || (strncmp(metadata->name, "HVC", 3) == 0))))
                {
                    BAL_LOG_INFO(logger,
                                 "Trapping on SMC/HVC instruction at 0x%016llX",
                                 (unsigned long long)guest_address);
                    is_block_terminated = true;
                    break;
                }
            }

            if (engine_flags & BAL_ENGINE_FLAG_WFI_YIELDS_HOST)
            {
                if (BAL_UNLIKELY(0 == strncmp(metadata->name, "WFI", 3)))
                {
                    BAL_LOG_INFO(logger,
                                 "WFI instruction encountered at 0x%016llX, yielding to host",
                                 (unsigned long long)guest_address);
                    is_block_terminated = true;
                    break;
                }
            }

            switch (metadata->ir_opcode)
            {
                case OPCODE_MOV:
                    translate_mov_orr(compiler, metadata, instruction);
                    break;
                case OPCODE_CONST:;
                    const char variant = metadata->name[3];

                    if (BAL_LIKELY(variant == 'N') || BAL_LIKELY(variant == 'K')
                        || BAL_LIKELY(variant == 'Z'))
                    {
                        translate_mov(compiler, metadata, instruction);
                        break;
                    }

                    BAL_LOG_ERROR(logger,
                                  "Aborting function: Invalid CONST instruction detected: %s",
                                  metadata->name);
                    is_block_terminated = true;
                    break;
                case OPCODE_JUMP:;
                    bal_guest_address_t target_pc = 0;
                    translate_jump(compiler, metadata, instruction, guest_address, &target_pc);
                    guest_address       = target_pc;
                    is_block_terminated = true;
                    break;
                case OPCODE_RETURN:
                    BAL_LOG_DEBUG(logger, "Block terminated by RET");
                    const uint8_t rn
                        = (uint8_t)extract_operand_value(instruction, &metadata->operands[0]);
                    const bool               skip_load_instruction = false;
                    const bal_x86_register_t x86_rn
                        = allocate_x86_register(compiler, rn, skip_load_instruction);

                    const bal_x86_macro_t store_pc_macro
                        = { .opcode              = BAL_X86_MACRO_STORE,
                            .source              = x86_rn,
                            .immediate_or_offset = offsetof(bal_cpu_t, pc) };
                    bal_sliding_window_push(&compiler->window, store_pc_macro);

                    is_pc_dynamic       = true;
                    is_block_terminated = true;
                    break;
                case OPCODE_TRAP:
                    BAL_LOG_INFO(logger,
                                 "Block terminated by TRAP/Unknown instruction: %s: ",
                                 metadata->name);
                    is_block_terminated = true;
                    break;
                default:
                    BAL_LOG_ERROR(logger,
                                  " Aborting function: Tier 1 Unsupported Opcode: %s",
                                  metadata->name);
                    is_block_terminated = true;
                    break;
            }

            if (BAL_UNLIKELY(compiler->status != BAL_SUCCESS
                             || compiler->assembler.status != BAL_SUCCESS))
            {
                if (BAL_SUCCESS == compiler->status)
                {
                    compiler->status = compiler->assembler.status;
                }

                BAL_LOG_ERROR(logger, "Status failure during translation: %d", compiler->status);
                is_block_terminated = true;
                break;
            }

            ++arm_instruction_count;

            if (i + 1 == max_instructions)
            {
                BAL_LOG_WARN(logger, "Max instructions limit reached, terminating block");
                is_block_terminated = true;
                break;
            }

            if (true == is_block_terminated)
            {
                break;
            }

            guest_address += 4;
            ++host_address_cursor;
        }
    }

    terminate_block(compiler, guest_address, is_pc_dynamic, arm_instruction_count, engine_flags);

    if (BAL_UNLIKELY(compiler->status != BAL_SUCCESS || compiler->assembler.status != BAL_SUCCESS))
    {
        const bal_error_t status
            = compiler->status == BAL_SUCCESS ? compiler->assembler.status : compiler->status;
        BAL_LOG_ERROR(
            logger, "Aborting function: block assembly was truncated due to error: %d", status);
        return NULL;
    }

    BAL_LOG_INFO(
        logger, "Tier 1 compiled block ends at GVA 0x%016llX", (unsigned long long)guest_address);
    return host_address;
}

static void
reset_register_allocator(bal_tier1_compiler_t *compiler)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return;
    }

    BAL_LOG_TRACE(&compiler->logger, "Resetting Register Allocator");
    memset(compiler->arm_to_x86, -1, sizeof(compiler->arm_to_x86));
    memset(compiler->x86_to_arm, -1, sizeof(compiler->x86_to_arm));
    memset(compiler->is_dirty, false, sizeof(compiler->is_dirty));
}

bal_x86_register_t
allocate_x86_register(bal_tier1_compiler_t *compiler,
                      uint8_t               arm_register,
                      const bool            skip_load_instruction)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        BAL_LOG_ERROR(&compiler->logger, "Aborting function: compiler is NULL");
        return BAL_X86_INVALID;
    }

    if (compiler->arm_to_x86[arm_register] != -1)
    {
        BAL_LOG_DEBUG(&compiler->logger,
                      "RegAlloc: Hit - ARM X%u is already in x86 r%d",
                      arm_register,
                      compiler->arm_to_x86[arm_register]);
        const int8_t x86_register = compiler->arm_to_x86[arm_register];
        return x86_register;
    }

    bal_x86_register_t free_register       = BAL_X86_RAX;
    bool               free_register_found = false;

    for (size_t i = 0; i < SCRATCH_REGISTERS_SIZE; ++i)
    {
        if (-1 == compiler->x86_to_arm[SCRATCH_REGISTERS[i]])
        {
            free_register       = SCRATCH_REGISTERS[i];
            free_register_found = true;
            break;
        }
    }

    if (false == free_register_found)
    {
        // TODO: add spilling
        BAL_LOG_ERROR(&compiler->logger, "Greedy Allocator ran out of scratch registers!");
    }

    compiler->arm_to_x86[arm_register]  = (int8_t)free_register;
    compiler->x86_to_arm[free_register] = (int8_t)arm_register;

    if (false == skip_load_instruction)
    {
        BAL_LOG_DEBUG(&compiler->logger,
                      "RegAlloc: Miss - mapped ARM X%u to x86 r%d",
                      arm_register,
                      free_register);
        const uint64_t        offset              = offsetof(bal_cpu_t, x[arm_register]);
        const bal_x86_macro_t load_register_macro = {
            .opcode              = BAL_X86_MACRO_LOAD,
            .destination         = free_register,
            .immediate_or_offset = offset,
        };

        bal_sliding_window_push(&compiler->window, load_register_macro);
        BAL_LOG_DEBUG(&compiler->logger, "RegAlloc: emitted LOAD macro for ARM X%u", arm_register);
    }
    else
    {
        BAL_LOG_DEBUG(&compiler->logger,
                      "RegAlloc: skipped LOAD macro for ARM X%u (write only)",
                      arm_register);
    }
    return free_register;
}

void
flush_dirty_registers(bal_tier1_compiler_t *compiler)
{
    if (NULL == compiler)
    {
        return;
    }

    const bal_logger_t *BAL_RESTRICT logger            = &compiler->logger;
    const bool *BAL_RESTRICT         dirty_cursor      = compiler->is_dirty;
    const int8_t *BAL_RESTRICT       arm_to_x86_cursor = compiler->arm_to_x86;

    for (uint8_t arm_register = 0; arm_register < 32; ++arm_register)
    {
        if (true == *dirty_cursor)
        {
            const bal_x86_register_t x86_register = (bal_x86_register_t)*arm_to_x86_cursor;
            const uint64_t           offset       = offsetof(bal_cpu_t, x[arm_register]);
            const bal_x86_macro_t    store_macro  = {
                .opcode              = BAL_X86_MACRO_STORE,
                .source              = x86_register,
                .immediate_or_offset = offset,
            };
            BAL_LOG_DEBUG(logger, "Flush: dirty ARM X%u to x86 r%d", arm_register, x86_register);
            (void)logger;
            bal_sliding_window_push(&compiler->window, store_macro);
        }

        ++dirty_cursor;
        ++arm_to_x86_cursor;
    }
}

void
terminate_block(bal_tier1_compiler_t *compiler,
                bal_guest_address_t   next_pc,
                bool                  is_pc_dynamic,
                size_t                arm_instruction_count,
                uint32_t              engine_flags)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return;
    }

    BAL_LOG_DEBUG(&compiler->logger, "Terminating basic block");

    if (engine_flags & BAL_ENGINE_FLAG_INSTRUCTION_COUNTING)
    {
        const bal_x86_macro_t count_macro = {
            .opcode              = BAL_X86_MACRO_ADD_CPU_ICOUNT,
            .destination         = BAL_X86_RBP,
            .immediate_or_offset = arm_instruction_count,
        };
        bal_sliding_window_push(&compiler->window, count_macro);
    }

    flush_dirty_registers(compiler);
    BAL_LOG_TRACE(&compiler->logger, "Flushing sliding window");
    bal_sliding_window_flush_all(&compiler->window);

    if (false == is_pc_dynamic)
    {
        BAL_LOG_TRACE(&compiler->logger, "Updating Guest PC");
        bal_x86_emit_mov_r64_imm64(&compiler->assembler, BAL_X86_RAX, next_pc);
        bal_x86_emit_store_r64_rbp_offset(
            &compiler->assembler, BAL_X86_RAX, offsetof(bal_cpu_t, pc));
    }

    BAL_LOG_TRACE(&compiler->logger, "Restoring host frame pointer and emitting RET");
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_RBP);
    bal_x86_emit_ret(&compiler->assembler);
}

BAL_HOT static uint32_t
extract_operand_value(const uint32_t instruction, const bal_decoder_operand_t *operand)
{
    if (BAL_OPERAND_TYPE_NONE == operand->type)
    {
        return 0;
    }

    const uint32_t mask = (1U << operand->bit_width) - 1;
    const uint32_t bits = instruction >> operand->bit_position & mask;
    return bits;
}

void
translate_jump(bal_tier1_compiler_t *BAL_RESTRICT                     compiler,
               const bal_decoder_instruction_metadata_t *BAL_RESTRICT metadata,
               const uint32_t                                         instruction,
               const bal_guest_address_t                              guest_address,
               bal_guest_address_t *BAL_RESTRICT                      target_pc)
{
    const uint32_t imm26     = extract_operand_value(instruction, &metadata->operands[0]);
    const uint32_t bit_width = metadata->operands[0].bit_width;
    const uint32_t shift     = 32U - bit_width;

    // WARNING: Shift guarantees sign extension within 32-bits.
    const int32_t signed_immediate = (int32_t)(imm26 << shift) >> shift;

    // WARNING: ARM64 branch offsets are encoded as 4 bytes.
    const int64_t offset = (int64_t)signed_immediate * 4;

    // WARNING: Cast offset to uint64_t handles negative offsets safely via two's complement
    // wrapping.
    *target_pc = guest_address + (uint64_t)offset;

    BAL_LOG_DEBUG(
        &compiler->logger, "Translated JUMP to 0x%016llX", (unsigned long long)*target_pc);

    // WARNING: Prevents unsued local variable compiler warning.
    (void)compiler;
}

BAL_HOT static void
translate_mov(bal_tier1_compiler_t                     *compiler,
              const bal_decoder_instruction_metadata_t *metadata,
              const uint32_t                            instruction)
{
    // WARNING: Cast to uint8_t is safe because ARM64 register indices
    // are in the range [0, 31].
    const uint8_t  rd      = (uint8_t)extract_operand_value(instruction, &metadata->operands[0]);
    const uint64_t imm16   = extract_operand_value(instruction, &metadata->operands[1]);
    const uint64_t hw      = extract_operand_value(instruction, &metadata->operands[2]);
    const uint64_t shift   = hw * 16ULL;
    const uint64_t mask    = BAL_OPERAND_TYPE_REGISTER_32 == metadata->operands[0].type
                                 ? 0xFFFFFFFFULL
                                 : 0xFFFFFFFFFFFFFFFFULL;
    const char     variant = metadata->name[3];

    if ('Z' == variant)
    {
        const uint64_t           value                 = imm16 << shift & mask;
        const bool               skip_load_instruction = true;
        const bal_x86_register_t x86_rd
            = allocate_x86_register(compiler, rd, skip_load_instruction);

        const bal_x86_macro_t mov_macro = {
            .opcode              = BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = value,
        };
        bal_sliding_window_push(&compiler->window, mov_macro);
        compiler->is_dirty[rd] = true;
    }
    else if ('N' == variant)
    {
        const uint64_t           value                 = ~(imm16 << shift) & mask;
        const bool               skip_load_instruction = true;
        const bal_x86_register_t x86_rd
            = allocate_x86_register(compiler, rd, skip_load_instruction);
        const bal_x86_macro_t mov_macro = {
            .opcode              = BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = value,
        };
        bal_sliding_window_push(&compiler->window, mov_macro);
        compiler->is_dirty[rd] = true;
    }
    else
    {
        // MOVK: Keep existing bits, overwrite 16 bits.
        //
        const bool               skip_load_instruction = false;
        const bal_x86_register_t x86_rd
            = allocate_x86_register(compiler, rd, skip_load_instruction);
        const uint64_t        clear_mask   = ~(0xFFFFULL << shift) & mask;
        const uint64_t        insert_value = imm16 << shift & mask;
        const bal_x86_macro_t and_macro    = {
            .opcode              = BAL_X86_MACRO_AND_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = clear_mask,
        };
        const bal_x86_macro_t or_macro = {
            .opcode              = BAL_X86_MACRO_OR_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = insert_value,
        };
        bal_sliding_window_push(&compiler->window, and_macro);
        bal_sliding_window_push(&compiler->window, or_macro);
    }
    compiler->is_dirty[rd] = true;
}

BAL_HOT static uint64_t
decode_bitmask_immediate(const uint32_t N,
                         const uint32_t immr,
                         const uint32_t imms,
                         const int32_t  datasize)
{
    // WARNING: N, immr, imms are extracted from the raw ARM64 instruction's
    // bitmask immediate fields. datasize must be 32 or 64.

    // Compute the highest set bit in N:imms to find the element size.
    // For 64-bit: 7 bit value (N + imms[5:0]).
    // For 32-bit: 5 bit value (imms[4:0]), N is ignored.
    //
    uint32_t test = (N << 6U) | imms;

    if (32 == datasize)
    {
        // WARNING: For 32-bit, only the lower 5 bits of imms are used.
        test &= 0x1FU;
    }

    // WARNING: All-ones in the relevant bits means all-ones result.
    if (((64 == datasize) && (0x7FU == test)) || ((32 == datasize) && (0x1FU == test)))
    {
        return (64 == datasize) ? ~0ULL : 0xFFFFFFFFULL;
    }

    // Find the highest set bit in 'test' to determine the element size.
    //
    const uint32_t max_bit = (64 == datasize) ? 6U : 4U;
    int32_t       len      = 0;

    for (uint32_t i = max_bit; i != UINT32_MAX; --i)
    {
        if (test & (1U << i))
        {
            // WARNING: Cast uint32_t i (max 6) to int32_t is safe.
            len = (int32_t)i;
            break;
        }
    }

    // WARNING: esize must be at least 2. Enforce len >= 1.
    if (0 == len)
    {
        len = 1;
    }

    const int32_t  esize  = 1 << len;

    // WARNING: Cast len (0-6) to uint64_t for safe shift.
    const uint64_t levels = (1ULL << (uint64_t)len) - 1ULL;

    // WARNING: Narrowing cast from uint64_t to int32_t is safe because
    // levels is at most 63, and AND with levels guarantees the result fits.
    const int32_t S = (int32_t)((uint64_t)imms & levels);
    const int32_t R = (int32_t)((uint64_t)immr & levels);

    // Create (S + 1) consecutive 1 bits, rotate right by R.
    //
    uint64_t mask = 0ULL;

    // WARNING: (S + 1) can be up to 64. 1ULL << 64 is undefined behavior.
    if ((uint64_t)(S + 1) >= 64ULL)
    {
        mask = ~0ULL;
    }
    else
    {
        // WARNING: Cast to uint64_t prevents undefined shift behavior.
        mask = (1ULL << (uint64_t)(S + 1)) - 1ULL;
    }

    if (0U != (uint32_t)R)
    {
        // WARNING: Casts to uint64_t ensure safe 64-bit rotation.
        mask = (mask >> (uint64_t)R) | (mask << (uint64_t)(esize - R));
    }

    // WARNING: Mask to esize bits when esize is smaller than 64.
    if (esize < 64)
    {
        // WARNING: Cast esize to uint64_t for safe shift.
        mask &= (1ULL << (uint64_t)esize) - 1ULL;
    }

    // Replicate the pattern to fill the full datasize.
    //
    int32_t current_esize = esize;

    while (current_esize < datasize)
    {
        mask = (mask << current_esize) | mask;
        current_esize *= 2;
    }

    // WARNING: For 32-bit operations, clear the upper 32 bits.
    if (32 == datasize)
    {
        mask &= 0xFFFFFFFFULL;
    }

    return mask;
}

BAL_HOT static void
translate_mov_orr(bal_tier1_compiler_t                     *compiler,
                  const bal_decoder_instruction_metadata_t *metadata,
                  const uint32_t                            instruction)
{
    // WARNING: The decoder maps 11 ORR encodings to OPCODE_MOV.
    // 7 are NEON/SIMD (rejected here), 4 are scalar (handled below).

    const bal_logger_t *BAL_RESTRICT logger = &compiler->logger;

    // Step 1: SIMD detection. Reject REGISTER_128 or non-standard width.
    //
    for (uint32_t i = 0; i < BAL_OPERANDS_SIZE; ++i)
    {
        const uint16_t type = metadata->operands[i].type;
        const uint16_t bw   = metadata->operands[i].bit_width;

        if (BAL_UNLIKELY(BAL_OPERAND_TYPE_REGISTER_128 == type))
        {
            BAL_LOG_ERROR(logger,
                          "Unsupported SIMD ORR variant: %s", metadata->name);
            compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
            return;
        }

        if (BAL_UNLIKELY((BAL_OPERAND_TYPE_NONE != type)
                         && (BAL_OPERAND_TYPE_IMMEDIATE != type)
            && (BAL_OPERAND_TYPE_CONDITION != type)
            // WARNING: 5 is the bit width of ARM register indices (32 regs).
            && (5 != bw)))
        {
            BAL_LOG_ERROR(logger,
                          "Unsupported SIMD ORR variant: %s", metadata->name);
            compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
            return;
        }
    }

    // Step 2: Count defined operands.
    //
    uint32_t operand_count = 0;

    for (uint32_t i = 0; i < BAL_OPERANDS_SIZE; ++i)
    {
        if (BAL_OPERAND_TYPE_NONE != metadata->operands[i].type)
        {
            ++operand_count;
        }
    }

    // Step 3: Dispatch based on operand count.
    //
    if (2 == operand_count)
    {
        handle_orr_immediate(compiler, metadata, instruction);
        return;
    }

    if (5 == operand_count)
    {
        handle_orr_shifted_register(compiler, metadata, instruction);
        return;
    }

    // Step 4: Fallback for unexpected operand counts.
    //
    BAL_LOG_ERROR(logger,
                  "Unsupported OPCODE_MOV variant"
                  " with %u operands: %s",
                  operand_count,
                  metadata->name);
    compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
}

BAL_HOT static void
handle_orr_immediate(bal_tier1_compiler_t                     *compiler,
                     const bal_decoder_instruction_metadata_t *metadata,
                     const uint32_t                            instruction)
{
    const uint32_t rd
        = extract_operand_value(instruction, &metadata->operands[0]);
    const uint32_t rn
        = extract_operand_value(instruction, &metadata->operands[1]);

    // WARNING: N:immr:imms are not exposed by the decoder. Extract
    // them directly from the raw instruction word.
    const uint32_t N    = (instruction >> 22U) & 1U;
    const uint32_t immr = (instruction >> 16U) & 0x3FU;
    const uint32_t imms = (instruction >> 10U) & 0x3FU;
    const int32_t  datasize
        = (BAL_OPERAND_TYPE_REGISTER_64
           == metadata->operands[0].type) ? 64 : 32;

    const uint64_t mask
        = decode_bitmask_immediate(N, immr, imms, datasize);

    if (31 == rn)
    {
        // MOV Wd/Xd, #imm alias (Rn = XZR).
        // WARNING: Cast rd to uint8_t is safe (range [0, 31]).
        const bool               skip_load = true;
        const bal_x86_register_t x86_rd
            = allocate_x86_register(compiler, (uint8_t)rd, skip_load);
        const bal_x86_macro_t mov_macro = {
            .opcode              = BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = mask,
        };
        bal_sliding_window_push(&compiler->window, mov_macro);
    }
    else
    {
        // General ORR Wd/Xd, Wn/Xn, #imm.
        // WARNING: Casts to uint8_t are safe (range [0, 31]).
        const bal_x86_register_t x86_rn
            = allocate_x86_register(compiler, (uint8_t)rn, false);
        const bal_x86_register_t x86_rd
            = allocate_x86_register(compiler, (uint8_t)rd, true);

        if (x86_rd != x86_rn)
        {
            const bal_x86_macro_t mov_macro = {
                .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
                .destination = x86_rd,
                .source      = x86_rn,
            };
            bal_sliding_window_push(&compiler->window, mov_macro);
        }

        const bal_x86_macro_t or_macro = {
            .opcode              = BAL_X86_MACRO_OR_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = mask,
        };
        bal_sliding_window_push(&compiler->window, or_macro);

        // WARNING: For 32-bit operations, zero-extend the result to
        // clear the upper 32 bits of the x86 register.
        if (32 == datasize)
        {
            const bal_x86_macro_t and_macro = {
                .opcode = BAL_X86_MACRO_AND_REGISTER_IMMEDIATE,
                .destination = x86_rd,
                .immediate_or_offset = 0xFFFFFFFFULL,
            };
            bal_sliding_window_push(&compiler->window, and_macro);
        }
    }

    compiler->is_dirty[rd] = true;
}

BAL_HOT static void
handle_orr_shifted_register(bal_tier1_compiler_t                     *compiler,
                             const bal_decoder_instruction_metadata_t *metadata,
                             const uint32_t                            instruction)
{
    // WARNING: Casts to uint8_t are safe (range [0, 31]).
    const uint32_t rd
        = extract_operand_value(instruction, &metadata->operands[0]);
    const uint32_t rn
        = extract_operand_value(instruction, &metadata->operands[1]);
    const uint32_t imm6
        = extract_operand_value(instruction, &metadata->operands[2]);
    const uint32_t rm
        = extract_operand_value(instruction, &metadata->operands[3]);
    const uint32_t shift_type
        = extract_operand_value(instruction, &metadata->operands[4]);

    // MOV Xd/Wd, Xm (register copy: ORR Xd, XZR, Xm).
    //
    if ((31 == rn) && (0 == imm6) && (0 == shift_type))
    {
        // WARNING: Casts to uint8_t are safe (range [0, 31]).
        const bal_x86_register_t x86_rm
            = allocate_x86_register(compiler, (uint8_t)rm, false);
        const bal_x86_register_t x86_rd
            = allocate_x86_register(compiler, (uint8_t)rd, true);

        // WARNING: Skip identity move when x86_rd == x86_rm.
        if (x86_rd != x86_rm)
        {
            const bal_x86_macro_t mov_macro = {
                .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
                .destination = x86_rd,
                .source      = x86_rm,
            };
            bal_sliding_window_push(&compiler->window, mov_macro);
        }

        // WARNING: For 32-bit operations, zero-extend the result.
        const int32_t regcopy_datasize
            = (BAL_OPERAND_TYPE_REGISTER_64
               == metadata->operands[0].type) ? 64 : 32;

        if (32 == regcopy_datasize)
        {
            const bal_x86_macro_t and_macro = {
                .opcode = BAL_X86_MACRO_AND_REGISTER_IMMEDIATE,
                .destination = x86_rd,
                .immediate_or_offset = 0xFFFFFFFFULL,
            };
            bal_sliding_window_push(&compiler->window, and_macro);
        }

        compiler->is_dirty[rd] = true;
        return;
    }

    // General ORR Xd, Xn, Xm {shift_type #imm6}.
    //
    if (BAL_UNLIKELY(shift_type > 2))
    {
        BAL_LOG_ERROR(&compiler->logger,
                      "Unsupported ORR shift type: %u", shift_type);
        compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
        return;
    }

    const int32_t datasize
        = (BAL_OPERAND_TYPE_REGISTER_64
           == metadata->operands[0].type) ? 64 : 32;

    // WARNING: Allocate sources before dest to avoid register aliasing.
    // WARNING: When rn == 31 (XZR), do NOT load from x[31].
    //
    // WARNING: Casts to uint8_t are safe (range [0, 31]).
    const bal_x86_register_t x86_rm
        = allocate_x86_register(compiler, (uint8_t)rm, false);
    bal_x86_register_t x86_rn    = BAL_X86_INVALID;
    bool               has_x86_rn = false;

    if (31 != rn)
    {
        x86_rn    = allocate_x86_register(compiler, (uint8_t)rn, false);
        has_x86_rn = true;
    }

    const bal_x86_register_t x86_rd
        = allocate_x86_register(compiler, (uint8_t)rd, true);

    emit_orr_shifted_raw(compiler, x86_rd, x86_rn, x86_rm,
                         imm6, shift_type, datasize, has_x86_rn);

    compiler->is_dirty[rd] = true;
}

BAL_HOT static void
emit_orr_shifted_raw(bal_tier1_compiler_t             *compiler,
                     const bal_x86_register_t          x86_rd,
                     const bal_x86_register_t          x86_rn,
                     const bal_x86_register_t          x86_rm,
                     const uint32_t                    imm6,
                     const uint32_t                    shift_type,
                     const int32_t                     datasize,
                     const bool                        has_x86_rn)
{
    // Flush window so R11 is free for raw assembler emission.
    //
    bal_sliding_window_flush_all(&compiler->window);

    // Copy Rm to R11 and shift it.
    //
    bal_x86_emit_mov_r64_r64(&compiler->assembler, BAL_X86_R11, x86_rm);

    // WARNING: imm6 is at most 63, safe to cast to uint8_t.
    switch (shift_type)
    {
        case 0:
            bal_x86_emit_shl_r64_imm8(
                &compiler->assembler, BAL_X86_R11, (uint8_t)imm6);
            break;
        case 1:
            bal_x86_emit_shr_r64_imm8(
                &compiler->assembler, BAL_X86_R11, (uint8_t)imm6);
            break;
        case 2:
            bal_x86_emit_sar_r64_imm8(
                &compiler->assembler, BAL_X86_R11, (uint8_t)imm6);
            break;
        default:
            break;
    }

    if (false == has_x86_rn)
    {
        // WARNING: No Rn (XZR case). Xd = Rm << shift.
        bal_x86_emit_mov_r64_r64(
            &compiler->assembler, x86_rd, BAL_X86_R11);
    }
    else
    {
        // WARNING: Skip identity move when x86_rd == x86_rn.
        if (x86_rd != x86_rn)
        {
            bal_x86_emit_mov_r64_r64(
                &compiler->assembler, x86_rd, x86_rn);
        }

        bal_x86_emit_or_r64_r64(
            &compiler->assembler, x86_rd, BAL_X86_R11);
    }

    // WARNING: For 32-bit operations, zero-extend the result.
    if (32 == datasize)
    {
        const bal_x86_macro_t and_macro = {
            .opcode              = BAL_X86_MACRO_AND_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = 0xFFFFFFFFULL,
        };
        bal_sliding_window_push(&compiler->window, and_macro);
        bal_sliding_window_flush_all(&compiler->window);
    }
}

/*** end of file ***/