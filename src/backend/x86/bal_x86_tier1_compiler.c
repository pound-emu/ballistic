#include "backend/x86/bal_x86_tier1_compiler.h"
#include "backend/bal_cpu.h"
#include "bal_decoder.h"
#include "bal_engine.h"
#include "bal_engine_flags.h"
#include "bal_platform.h"

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
// Note that R11 was intentionally excluded from this. See [`BAL_X86_REGISTER_DISCARD_RESULT`].
static const bal_x86_register_t SCRATCH_REGISTERS[]
    = { BAL_X86_RAX, BAL_X86_RCX, BAL_X86_RDX, BAL_X86_R8, BAL_X86_R9,
        BAL_X86_R10, BAL_X86_R12, BAL_X86_R14, BAL_X86_R15 };

#define SCRATCH_REGISTERS_SIZE (sizeof(SCRATCH_REGISTERS) / sizeof(SCRATCH_REGISTERS[0]))

/// Dedicated throwaway register for instructions that update flags but discards the result.
#define BAL_X86_REGISTER_DISCARD_RESULT BAL_X86_R11

static void reset_register_allocator(bal_tier1_compiler_t *compiler);

static bal_x86_register_t allocate_x86_register(bal_tier1_compiler_t *compiler,
                                                uint8_t               arm_register,
                                                bool                  skip_load_instruction);

static void evict_x86_register(bal_tier1_compiler_t *compiler, bal_x86_register_t x86_register);

static void flush_dirty_registers(bal_tier1_compiler_t *compiler);

static void terminate_block(bal_tier1_compiler_t *compiler,
                            bal_guest_address_t   next_pc,
                            bool                  is_pc_dynamic,
                            size_t                arm_instruction_count,
                            uint32_t              engine_flags);

static void terminate_block_conditional(bal_tier1_compiler_t *compiler,
                                        bal_x86_condition_t   condition,
                                        bal_guest_address_t   fallthrough_pc,
                                        bal_guest_address_t   taken_pc);

static uint32_t extract_operand_value(uint32_t instruction, const bal_decoder_operand_t *operand);

static void translate_add_sub_imm(bal_tier1_compiler_t                     *compiler,
                                  const bal_decoder_instruction_metadata_t *metadata,
                                  uint32_t                                  instruction,
                                  bool                                      is_sub);

static void translate_add_sub_reg(bal_tier1_compiler_t                     *compiler,
                                  const bal_decoder_instruction_metadata_t *metadata,
                                  uint32_t                                  instruction,
                                  bool                                      is_sub);

static void translate_b_cond(bal_tier1_compiler_t *compiler,
                             uint32_t              instruction,
                             bal_guest_address_t   guest_address);

static void translate_cbz_cbnz(bal_tier1_compiler_t                     *compiler,
                               const bal_decoder_instruction_metadata_t *metadata,
                               uint32_t                                  instruction,
                               bal_guest_address_t                       guest_address,
                               bool                                      is_cbnz);

static void translate_jump(const bal_tier1_compiler_t               *compiler,
                           const bal_decoder_instruction_metadata_t *metadata,
                           const uint32_t                            instruction,
                           const bal_guest_address_t                 guest_address,
                           bal_guest_address_t                      *target_pc);

static void translate_and(bal_tier1_compiler_t                     *compiler,
                          const bal_decoder_instruction_metadata_t *metadata,
                          uint32_t                                  instruction);

static void translate_mov_immediate(bal_tier1_compiler_t                     *compiler,
                                    const bal_decoder_instruction_metadata_t *metadata,
                                    uint32_t                                  instruction);

static void translate_mov_register(bal_tier1_compiler_t                     *compiler,
                                   const bal_decoder_instruction_metadata_t *metadata,
                                   uint32_t                                  instruction);

bal_error_t
bal_tier1_compiler_init(bal_tier1_compiler_t         *compiler,
                        const bal_executable_buffer_t executable_buffer,
                        const size_t                  buffer_size,
                        bal_jit_debug_context_t      *debug_context)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: compiler is NULL");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(NULL == executable_buffer.rw_pointer)
        || BAL_UNLIKELY(NULL == executable_buffer.rx_pointer))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: executable_buffer is NULL");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return compiler->status;
    }

    if (BAL_UNLIKELY(0 == buffer_size))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: buffer_size is 0");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return compiler->status;
    }

    compiler->debug_context = debug_context;
    bal_error_t error
        = bal_x86_assembler_init(&compiler->assembler, executable_buffer, buffer_size);

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

    if (BAL_UNLIKELY(NULL == memory_interface))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: memory_interface is NULL");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return NULL;
    }

    if (BAL_UNLIKELY(NULL == memory_interface->translate))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: memory_interface->translate is NULL");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return NULL;
    }

    if (BAL_UNLIKELY(0 == max_instructions))
    {
        BAL_LOG_INFO(&bal_thread_logger, "Aborting function: max Instructions is 0");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return NULL;
    }

    BAL_LOG_INFO(&bal_thread_logger, "Starting Tier 1 JIT Compilation");
    BAL_LOG_INFO(&bal_thread_logger,
                 "GVA: 0x%016llX | Max Instructions: %zu",
                 (unsigned long long)guest_address,
                 max_instructions);
    reset_register_allocator(compiler);
    void *host_address = compiler->assembler.rx_buffer + compiler->assembler.offset;

    // Debug tracking.
    const bal_guest_address_t guest_address_base           = guest_address;
    const size_t              start_x86_block_offset       = compiler->assembler.offset;
    size_t                    start_x86_instruction_offset = 0;
    bal_jit_instruction_map_t mappings[2048];
    uint32_t                  mapping_count = 0;
    const bool                debug_enabled = engine_flags & BAL_ENGINE_FLAG_JIT_DEBUG
                               && compiler->debug_context != NULL
                               && compiler->debug_context->entries != NULL;

    // Setup block.
    //
    bal_x86_emit_push_r64(&compiler->assembler, BAL_X86_RBP);
    bal_x86_emit_push_r64(&compiler->assembler, BAL_X86_R12);
    bal_x86_emit_push_r64(&compiler->assembler, BAL_X86_R13);
    bal_x86_emit_push_r64(&compiler->assembler, BAL_X86_R14);
    bal_x86_emit_push_r64(&compiler->assembler, BAL_X86_R15);
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
            BAL_LOG_ERROR(&bal_thread_logger,
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
            BAL_LOG_ERROR(&bal_thread_logger,
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
                BAL_LOG_ERROR(&bal_thread_logger,
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
                BAL_LOG_ERROR(&bal_thread_logger,
                              "Aborting function: failed to decode instruction %08X at GVA 0x%llX",
                              instruction,
                              (unsigned long long)guest_address);
                is_block_terminated = false;
                break;
            }

            BAL_LOG_TRACE(&bal_thread_logger,
                          "[0X%016llX] %08x : %s",
                          (unsigned long long)guest_address,
                          instruction,
                          metadata->name);

            if (engine_flags & BAL_ENGINE_FLAG_TRAP_SVC)
            {
                if (BAL_UNLIKELY(0 == strncmp(metadata->name, "SVC", 3)))
                {
                    BAL_LOG_INFO(&bal_thread_logger,
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
                    BAL_LOG_INFO(&bal_thread_logger,
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
                    BAL_LOG_INFO(&bal_thread_logger,
                                 "WFI instruction encountered at 0x%016llX, yielding to host",
                                 (unsigned long long)guest_address);
                    is_block_terminated = true;
                    break;
                }
            }

            start_x86_instruction_offset = compiler->assembler.offset;

            switch (metadata->ir_opcode)
            {
                case OPCODE_MOV:;
                    const char variant = metadata->name[3];

                    if (BAL_LIKELY(variant == 'N') || BAL_LIKELY(variant == 'K')
                        || BAL_LIKELY(variant == 'Z'))
                    {
                        translate_mov_immediate(compiler, metadata, instruction);
                        break;
                    }

                    if (BAL_LIKELY(BAL_OPERAND_TYPE_REGISTER_64 == metadata->operands[1].type)
                        || BAL_LIKELY(BAL_OPERAND_TYPE_REGISTER_32 == metadata->operands[1].type))
                    {
                        translate_mov_register(compiler, metadata, instruction);
                    }

                    BAL_LOG_ERROR(&bal_thread_logger,
                                  "Aborting function: Invalid MOV instruction detected: %s",
                                  metadata->name);
                    is_block_terminated = true;
                    break;
                case OPCODE_ADD:
                case OPCODE_SUB:
                case OPCODE_CMP:;
                    if (BAL_OPERAND_TYPE_REGISTER_64 == metadata->operands[3].type
                        || BAL_OPERAND_TYPE_REGISTER_32 == metadata->operands[3].type)
                    {
                        const bool is_sub = (OPCODE_SUB == metadata->ir_opcode)
                                            || (OPCODE_CMP == metadata->ir_opcode);
                        translate_add_sub_reg(compiler, metadata, instruction, is_sub);
                        break;
                    }

                    if (BAL_LIKELY(BAL_OPERAND_TYPE_IMMEDIATE == metadata->operands[2].type))
                    {
                        const bool is_sub = (OPCODE_SUB == metadata->ir_opcode)
                                            || (OPCODE_CMP == metadata->ir_opcode);
                        translate_add_sub_imm(compiler, metadata, instruction, is_sub);
                        break;
                    }

                    BAL_LOG_ERROR(&bal_thread_logger,
                                  "Aborting function: Tier 1 unsupported opcode variant: "
                                  "%s",
                                  metadata->name);
                    is_block_terminated = true;
                    break;
                case OPCODE_JUMP:;
                    bal_guest_address_t target_pc = 0;
                    translate_jump(compiler, metadata, instruction, guest_address, &target_pc);
                    guest_address       = target_pc;
                    is_block_terminated = true;
                    break;
                case OPCODE_BRANCH_ZERO:
                case OPCODE_BRANCH_NOT_ZERO:;
                    translate_cbz_cbnz(compiler,
                                       metadata,
                                       instruction,
                                       guest_address,
                                       OPCODE_BRANCH_NOT_ZERO == metadata->ir_opcode);
                    is_block_terminated = true;
                    break;
                case OPCODE_BRANCH_CONDITIONAL:;
                    translate_b_cond(compiler, instruction, guest_address);
                    is_block_terminated = true;
                    break;
                case OPCODE_RETURN:
                    BAL_LOG_DEBUG(&bal_thread_logger, "Block terminated by RET");
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
                    BAL_LOG_INFO(&bal_thread_logger,
                                 "Block terminated by TRAP/Unknown instruction: %s: ",
                                 metadata->name);
                    const bal_x86_macro_t ud2_macro = {
                        .opcode = BAL_X86_MACRO_UD2,
                    };

                    // Sync guest state before the CPU faults.
                    bal_sliding_window_flush_all(&compiler->window);

                    bal_sliding_window_push(&compiler->window, ud2_macro);
                    compiler->status    = BAL_ERROR_UNKNOWN_INSTRUCTION;
                    is_block_terminated = true;
                    break;
                case OPCODE_AND:
                    translate_and(compiler, metadata, instruction);
                    break;
                default:
                    BAL_LOG_ERROR(&bal_thread_logger,
                                  " Aborting function: Tier 1 Unsupported Opcode: %s",
                                  metadata->name);
                    compiler->status    = BAL_ERROR_UNKNOWN_INSTRUCTION;
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

                BAL_LOG_ERROR(
                    &bal_thread_logger, "Status failure during translation: %d", compiler->status);
                is_block_terminated = true;
                break;
            }

            if (true == debug_enabled && mapping_count < 2048)
            {
                mappings[mapping_count].x86_offset
                    = (uint32_t)(start_x86_instruction_offset - start_x86_block_offset);
                mappings[mapping_count].guest_pc_offset
                    = (uint32_t)(guest_address - guest_address_base);
                ++mapping_count;
            }

            ++arm_instruction_count;

            if (i + 1 == max_instructions)
            {
                BAL_LOG_WARN(&bal_thread_logger,
                             "Max instructions limit reached, terminating block");
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
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Aborting function: block assembly was truncated due to error: %s",
                      bal_error_to_string(status));
        return NULL;
    }

    if (true == debug_enabled && mapping_count > 0)
    {
        void *BAL_RESTRICT rx_start = compiler->assembler.rx_buffer + start_x86_block_offset;
        const uint32_t rx_size = (uint32_t)(compiler->assembler.offset - start_x86_block_offset);
        bal_jit_debug_add_block(compiler->debug_context,
                                rx_start,
                                rx_size,
                                guest_address_base,
                                mappings,
                                mapping_count);
    }

    BAL_LOG_INFO(&bal_thread_logger,
                 "Tier 1 compiled block ends at GVA 0x%016llX",
                 (unsigned long long)guest_address);
    return host_address;
}

static void
reset_register_allocator(bal_tier1_compiler_t *compiler)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return;
    }

    BAL_LOG_TRACE(&bal_thread_logger, "Resetting Register Allocator");
    compiler->is_dirty        = 0U;
    compiler->eviction_cursor = 0U;
    (void)memset(compiler->arm_to_x86, -1, sizeof(compiler->arm_to_x86));
    (void)memset(compiler->x86_to_arm, -1, sizeof(compiler->x86_to_arm));
}

static bal_x86_register_t
allocate_x86_register(bal_tier1_compiler_t *BAL_RESTRICT compiler,
                      uint8_t                            arm_register,
                      const bool                         skip_load_instruction)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: compiler is NULL");
        return BAL_X86_INVALID;
    }

    if (BAL_UNLIKELY(compiler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function, compiler->status != BAL_SUCCESS.");
        return BAL_X86_INVALID;
    }

    if (BAL_UNLIKELY(compiler->assembler.status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: assembler status != BAL_SUCCESS.");
        return BAL_X86_INVALID;
    }

    if (BAL_UNLIKELY(arm_register >= 32U))
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Aborting function: ARM register index %u is out of "
                      "range (0-31).",
                      arm_register);
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return BAL_X86_INVALID;
    }

    if (compiler->arm_to_x86[arm_register] != -1)
    {
        const int8_t cached_x86_register = compiler->arm_to_x86[arm_register];

        if (BAL_UNLIKELY(cached_x86_register < 0 || cached_x86_register >= (int8_t)BAL_X86_R15))
        {
            BAL_LOG_ERROR(&bal_thread_logger,
                          "RegAlloc: corrupted mapping ARM X%u -> x86 r%d. "
                          "Valid range is 0-r%d. Resetting X%u.",
                          arm_register,
                          cached_x86_register,
                          (int)BAL_X86_R15,
                          arm_register);
            compiler->arm_to_x86[arm_register] = -1;
            compiler->status                   = BAL_ERROR_REGISTER_ALLOCATOR_CORRUPTED;
            return BAL_X86_INVALID;
        }

        if (BAL_UNLIKELY(compiler->x86_to_arm[cached_x86_register] != (int8_t)arm_register))
        {
            BAL_LOG_ERROR(&bal_thread_logger,
                          "RegAlloc: reverse x86 to ARM mapping inconsistency ARM X%u -> x86 r%d,"
                          " resetting both X%u and r%d",
                          arm_register,
                          cached_x86_register);
            compiler->arm_to_x86[arm_register]        = -1;
            compiler->x86_to_arm[cached_x86_register] = -1;
            compiler->status                          = BAL_ERROR_REGISTER_ALLOCATOR_CORRUPTED;
            return BAL_X86_INVALID;
        }

        BAL_LOG_DEBUG(&bal_thread_logger,
                      "RegAlloc: Hit - ARM X%u is already in x86 r%d",
                      arm_register,
                      cached_x86_register);
        return (bal_x86_register_t)cached_x86_register;
    }

    bal_x86_register_t                     free_register           = BAL_X86_INVALID;
    int8_t *BAL_RESTRICT                   x86_to_arm_cursor       = compiler->x86_to_arm;
    const bal_x86_register_t *BAL_RESTRICT scratch_register_cursor = SCRATCH_REGISTERS;

    for (size_t i = 0U; i < SCRATCH_REGISTERS_SIZE; ++i)
    {
        if (BAL_UNLIKELY(*scratch_register_cursor < 0
                         || *scratch_register_cursor > (int8_t)BAL_X86_R15))
        {
            BAL_LOG_ERROR(&bal_thread_logger,
                          "RegAlloc: scratch pool contains invalid entry %d "
                          "at index %zu",
                          *scratch_register_cursor,
                          i);
            compiler->status = BAL_ERROR_REGISTER_ALLOCATOR_CORRUPTED;
            return BAL_X86_INVALID;
        }

        if (-1 == x86_to_arm_cursor[*scratch_register_cursor])
        {
            free_register = *scratch_register_cursor;
            break;
        }

        ++scratch_register_cursor;
    }

    // Evict a clean register.
    if (BAL_X86_INVALID == free_register)
    {
        for (size_t i = 0U; i < SCRATCH_REGISTERS_SIZE; ++i)
        {
            const int8_t arm_victim_register = x86_to_arm_cursor[i];

            if (arm_victim_register != -1 && !(compiler->is_dirty & (1U << arm_victim_register)))
            {
                free_register = (bal_x86_register_t)i;
            }
        }

        // All registers are dirty, force an eviction.
        if (BAL_X86_INVALID == free_register)
        {
            const uint8_t slot_index = compiler->eviction_cursor % SCRATCH_REGISTERS_SIZE;
            free_register            = SCRATCH_REGISTERS[slot_index];
            ++compiler->eviction_cursor;
        }

        evict_x86_register(compiler, free_register);
    }

    compiler->arm_to_x86[arm_register]  = (int8_t)free_register;
    compiler->x86_to_arm[free_register] = (int8_t)arm_register;

    if (false == skip_load_instruction)
    {
        BAL_LOG_DEBUG(&bal_thread_logger,
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
        BAL_LOG_DEBUG(&bal_thread_logger, "RegAlloc: emitted LOAD macro for ARM X%u", arm_register);
    }
    else
    {
        BAL_LOG_DEBUG(&bal_thread_logger,
                      "RegAlloc: skipped LOAD macro for ARM X%u (write only)",
                      arm_register);
    }
    return free_register;
}

static void
evict_x86_register(bal_tier1_compiler_t *compiler, const bal_x86_register_t x86_register)
{
    const int8_t arm_victim = compiler->x86_to_arm[x86_register];

    if (BAL_UNLIKELY(-1 == arm_victim))
    {
        return;
    }

    const uint32_t dirty_bit = 1U << arm_victim;

    if (compiler->is_dirty & dirty_bit)
    {
        const uint64_t        offset      = offsetof(bal_cpu_t, x[arm_victim]);
        const bal_x86_macro_t store_macro = {
            .opcode              = BAL_X86_MACRO_STORE,
            .source              = x86_register,
            .immediate_or_offset = offset,
        };
        bal_sliding_window_push(&compiler->window, store_macro);
        compiler->is_dirty &= ~dirty_bit;
    }

    compiler->arm_to_x86[arm_victim]   = -1;
    compiler->x86_to_arm[x86_register] = -1;
    BAL_LOG_DEBUG(
        &bal_thread_logger, "RegAlloc: evicted ARM X%d from x86 r%d", arm_victim, x86_register);
}

void
flush_dirty_registers(bal_tier1_compiler_t *compiler)
{
    if (NULL == compiler)
    {
        return;
    }

    const bal_logger_t *BAL_RESTRICT logger            = &bal_thread_logger;
    const uint32_t                   dirty_mask        = compiler->is_dirty;
    const int8_t *BAL_RESTRICT       arm_to_x86_cursor = compiler->arm_to_x86;

    for (uint8_t arm_register = 0; arm_register < 32; ++arm_register)
    {
        if (dirty_mask & (1U << arm_register))
        {
            const bal_x86_register_t x86_register = (bal_x86_register_t)*arm_to_x86_cursor;
            const uint64_t           offset       = offsetof(bal_cpu_t, x[arm_register]);
            const bal_x86_macro_t    store_macro  = {
                    .opcode              = BAL_X86_MACRO_STORE,
                    .source              = x86_register,
                    .immediate_or_offset = offset,
            };
            BAL_LOG_DEBUG(
                &bal_thread_logger, "Flush: dirty ARM X%u to x86 r%d", arm_register, x86_register);
            (void)logger;
            bal_sliding_window_push(&compiler->window, store_macro);
        }

        ++arm_to_x86_cursor;
    }
}

void
terminate_block(bal_tier1_compiler_t     *compiler,
                const bal_guest_address_t next_pc,
                const bool                is_pc_dynamic,
                const size_t              arm_instruction_count,
                const uint32_t            engine_flags)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return;
    }

    BAL_LOG_DEBUG(&bal_thread_logger, "Terminating basic block");

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
    BAL_LOG_TRACE(&bal_thread_logger, "Flushing sliding window");
    bal_sliding_window_flush_all(&compiler->window);

    if (false == is_pc_dynamic)
    {
        BAL_LOG_TRACE(&bal_thread_logger, "Updating Guest PC");
        bal_x86_emit_mov_r64_imm64(&compiler->assembler, BAL_X86_RAX, next_pc);
        bal_x86_emit_store_r64_rbp_offset(
            &compiler->assembler, BAL_X86_RAX, offsetof(bal_cpu_t, pc));
    }

    BAL_LOG_TRACE(&bal_thread_logger, "Restoring callee-saved registers and emitting RET");
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R15);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R14);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R13);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R12);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_RBP);
    bal_x86_emit_ret(&compiler->assembler);
}

void
terminate_block_conditional(bal_tier1_compiler_t     *compiler,
                            const bal_x86_condition_t condition,
                            const bal_guest_address_t fallthrough_pc,
                            const bal_guest_address_t taken_pc)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return;
    }

    bal_sliding_window_flush_all(&compiler->window);
    size_t         dirty_count = 0;
    const uint32_t dirty_mask  = compiler->is_dirty;

    for (uint8_t arm_register = 0; arm_register < 32; ++arm_register)
    {
        if (dirty_mask & (1U << arm_register))
        {
            ++dirty_count;
        }
    }

    // STORE macro: 7 bytes (REX + Opcode + ModRM + Disp32)
    // MOV RAX, IMM64: 5 bytes if <= 32-bit, 10 bytes if > 32-bit
    // MOV [RBP+PC], RAX: 7 bytes (REX + Opcode + ModRM + Disp32)
    // POP R15-R12: 8 bytes (4 registers x 2 bytes [REX.B + Opcode]
    // POP RBP: 1 byte
    // RET: 1 byte
    const size_t mov_fallthrough_size = (fallthrough_pc <= 0xFFFFFFFFULL) ? 5 : 10;
    const size_t epilogue_size        = (dirty_count * 7) + mov_fallthrough_size + 7 + 8 + 1 + 1;

    bal_x86_assembler_t *BAL_RESTRICT assembler   = &compiler->assembler;
    const int32_t                     jump_offset = (int32_t)epilogue_size;
    bal_x86_emit_jcc_rel32(assembler, condition, jump_offset);

    flush_dirty_registers(compiler);
    bal_sliding_window_flush_all(&compiler->window);

    // Emit fallthrough epilogue.
    bal_x86_emit_mov_r64_imm64(&compiler->assembler, BAL_X86_RAX, fallthrough_pc);
    bal_x86_emit_store_r64_rbp_offset(&compiler->assembler, BAL_X86_RAX, offsetof(bal_cpu_t, pc));
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R15);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R14);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R13);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_R12);
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_RBP);
    bal_x86_emit_ret(&compiler->assembler);
    flush_dirty_registers(compiler);
    bal_sliding_window_flush_all(&compiler->window);

    // Emit taken epilogue.
    bal_x86_emit_mov_r64_imm64(&compiler->assembler, BAL_X86_RAX, taken_pc);
    bal_x86_emit_store_r64_rbp_offset(&compiler->assembler, BAL_X86_RAX, offsetof(bal_cpu_t, pc));
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
translate_add_sub_imm(bal_tier1_compiler_t *BAL_RESTRICT                     compiler,
                      const bal_decoder_instruction_metadata_t *BAL_RESTRICT metadata,
                      const uint32_t                                         instruction,
                      const bool                                             is_sub)
{
    const bal_decoder_operand_t *BAL_RESTRICT operand_cursor = metadata->operands;
    const uint8_t  rd           = (uint8_t)extract_operand_value(instruction, &operand_cursor[0]);
    const uint8_t  rn           = (uint8_t)extract_operand_value(instruction, &operand_cursor[1]);
    const uint32_t imm12        = extract_operand_value(instruction, &operand_cursor[2]);
    const uint32_t shift        = extract_operand_value(instruction, &operand_cursor[3]);
    const uint64_t shift_amount = 1 == shift ? 12 : 0;
    uint64_t       value        = (uint64_t)imm12 << shift_amount;

    // Does this instruction set flags?
    const bool is_setting_flags = (OPCODE_CMP == metadata->ir_opcode) || ('S' == metadata->name[3])
                                  || ('N' == metadata->name[2]);
    const bool               is_discarded_result = is_setting_flags && (31 == rd);
    const bool               skip_load_rn        = false;
    const bal_x86_register_t x86_rn = allocate_x86_register(compiler, rn, skip_load_rn);
    bal_x86_register_t       x86_rd;

    if (true == is_discarded_result)
    {
        x86_rd                          = BAL_X86_REGISTER_DISCARD_RESULT;
        const bal_x86_macro_t mov_macro = {
            .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
            .destination = x86_rd,
            .source      = x86_rn,
        };
        bal_sliding_window_push(&compiler->window, mov_macro);
    }
    else
    {
        const bool skip_load_rd = true;
        x86_rd                  = allocate_x86_register(compiler, rd, skip_load_rd);

        if (x86_rd != x86_rn)
        {
            const bal_x86_macro_t mov_macro = {
                .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
                .destination = x86_rd,
                .source      = x86_rn,
            };
            bal_sliding_window_push(&compiler->window, mov_macro);
        }
    }

    if (true == is_sub)
    {
        const bal_x86_macro_t sub_macro = {
            .opcode              = BAL_X86_MACRO_SUB_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = value,
        };
        bal_sliding_window_push(&compiler->window, sub_macro);
    }
    else
    {
        const bal_x86_macro_t add_macro = {
            .opcode              = BAL_X86_MACRO_ADD_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = value,
        };
        bal_sliding_window_push(&compiler->window, add_macro);
    }

    if (true == is_setting_flags)
    {
        const bal_x86_macro_t set_n_macro = {
            .opcode              = BAL_X86_MACRO_SETCC,
            .condition           = BAL_X86_COND_S,
            .immediate_or_offset = offsetof(bal_cpu_t, flag_n),
        };
        bal_sliding_window_push(&compiler->window, set_n_macro);

        const bal_x86_macro_t set_z_macro = {
            .opcode              = BAL_X86_MACRO_SETCC,
            .condition           = BAL_X86_COND_E,
            .immediate_or_offset = offsetof(bal_cpu_t, flag_z),
        };
        bal_sliding_window_push(&compiler->window, set_z_macro);

        const bal_x86_macro_t set_v_macro = {
            .opcode              = BAL_X86_MACRO_SETCC,
            .condition           = BAL_X86_COND_O,
            .immediate_or_offset = offsetof(bal_cpu_t, flag_v),
        };
        bal_sliding_window_push(&compiler->window, set_v_macro);

        // ARM carry flag is inverted compared to x86's carry during subtraction.
        const bal_x86_condition_t c_condition = true == is_sub ? BAL_X86_COND_AE : BAL_X86_COND_B;
        const bal_x86_macro_t     set_c_macro = {
                .opcode              = BAL_X86_MACRO_SETCC,
                .condition           = c_condition,
                .immediate_or_offset = offsetof(bal_cpu_t, flag_c),
        };
        bal_sliding_window_push(&compiler->window, set_c_macro);
    }

    if (false == is_discarded_result)
    {
        compiler->is_dirty |= 1U << rd;
    }
}

void
translate_add_sub_reg(bal_tier1_compiler_t *BAL_RESTRICT                     compiler,
                      const bal_decoder_instruction_metadata_t *BAL_RESTRICT metadata,
                      const uint32_t                                         instruction,
                      const bool                                             is_sub)
{
    const bal_decoder_operand_t *BAL_RESTRICT operand_cursor = metadata->operands;
    const uint8_t  rd           = (uint8_t)extract_operand_value(instruction, &operand_cursor[0]);
    const uint8_t  rn           = (uint8_t)extract_operand_value(instruction, &operand_cursor[1]);
    const uint32_t shift_amount = extract_operand_value(instruction, &operand_cursor[2]);
    const uint8_t  rm           = (uint8_t)extract_operand_value(instruction, &operand_cursor[3]);

    if (BAL_UNLIKELY(shift_amount != 0))
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Aborting function: Tier 1 does not support shift amounts != 0 yet: %s",
                      metadata->name);
        compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
        return;
    }

    // Does this instruction set flags?
    const bool is_setting_flags = (OPCODE_CMP == metadata->ir_opcode) || ('S' == metadata->name[3])
                                  || ('N' == metadata->name[2]);
    const bool               is_discarded_result = is_setting_flags && (31 == rd);
    const bool               skip_load_rn        = false;
    const bal_x86_register_t x86_rn       = allocate_x86_register(compiler, rn, skip_load_rn);
    const bool               skip_load_rm = false;
    const bal_x86_register_t x86_rm       = allocate_x86_register(compiler, rm, skip_load_rm);
    bal_x86_register_t       x86_rd;

    if (true == is_discarded_result)
    {
        x86_rd                          = BAL_X86_REGISTER_DISCARD_RESULT;
        const bal_x86_macro_t mov_macro = {
            .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
            .destination = x86_rd,
            .source      = x86_rn,
        };
        bal_sliding_window_push(&compiler->window, mov_macro);
    }
    else
    {
        const bool skip_load_rd = true;
        x86_rd                  = allocate_x86_register(compiler, rd, skip_load_rd);

        if (x86_rd != x86_rn)
        {
            const bal_x86_macro_t mov_macro = {
                .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
                .destination = x86_rd,
                .source      = x86_rn,
            };
            bal_sliding_window_push(&compiler->window, mov_macro);
        }
    }

    if (true == is_sub)
    {
        const bal_x86_macro_t sub_macro = {
            .opcode      = BAL_X86_MACRO_SUB_REGISTER_REGISTER,
            .destination = x86_rd,
            .source      = x86_rm,
        };
        bal_sliding_window_push(&compiler->window, sub_macro);
    }
    else
    {
        const bal_x86_macro_t add_macro = {
            .opcode      = BAL_X86_MACRO_ADD_REGISTER_REGISTER,
            .destination = x86_rd,
            .source      = x86_rm,
        };
        bal_sliding_window_push(&compiler->window, add_macro);
    }

    if (true == is_setting_flags)
    {
        const bal_x86_macro_t set_n_macro = {
            .opcode              = BAL_X86_MACRO_SETCC,
            .condition           = BAL_X86_COND_S,
            .immediate_or_offset = offsetof(bal_cpu_t, flag_n),
        };
        bal_sliding_window_push(&compiler->window, set_n_macro);

        const bal_x86_macro_t set_z_macro = {
            .opcode              = BAL_X86_MACRO_SETCC,
            .condition           = BAL_X86_COND_E,
            .immediate_or_offset = offsetof(bal_cpu_t, flag_z),
        };
        bal_sliding_window_push(&compiler->window, set_z_macro);

        const bal_x86_macro_t set_v_macro = {
            .opcode              = BAL_X86_MACRO_SETCC,
            .condition           = BAL_X86_COND_O,
            .immediate_or_offset = offsetof(bal_cpu_t, flag_v),
        };
        bal_sliding_window_push(&compiler->window, set_v_macro);

        // ARM carry flag is inverted compared to x86's carry during subtraction.
        const bal_x86_condition_t c_condition = true == is_sub ? BAL_X86_COND_AE : BAL_X86_COND_B;
        const bal_x86_macro_t     set_c_macro = {
                .opcode              = BAL_X86_MACRO_SETCC,
                .condition           = c_condition,
                .immediate_or_offset = offsetof(bal_cpu_t, flag_c),
        };
        bal_sliding_window_push(&compiler->window, set_c_macro);
    }

    if (false == is_discarded_result)
    {
        compiler->is_dirty |= 1U << rd;
    }
}

void
translate_b_cond(bal_tier1_compiler_t *BAL_RESTRICT compiler,
                 const uint32_t                     instruction,
                 const bal_guest_address_t          guest_address)
{
    const uint32_t            cond           = instruction & 0xF;
    const uint32_t            imm19          = (instruction >> 5) & 0x7FFFF;
    const int32_t             signed_imm     = (int32_t)(imm19 ^ 0x40000) - 0x40000;
    const int64_t             offset         = (int64_t)signed_imm * 4;
    const bal_guest_address_t taken_pc       = guest_address + (uint64_t)offset;
    const bal_guest_address_t fallthrough_pc = guest_address + 4;

    bal_x86_assembler_t *BAL_RESTRICT assembler   = &compiler->assembler;
    int32_t                           flag_offset = 0;

    // If true, branch if flag == 0. If false, branch if flag != 0.
    bool test_for_zero = false;

    switch (cond)
    {
        case 0x0:
            flag_offset   = offsetof(bal_cpu_t, flag_z);
            test_for_zero = false;
            break; // EQ (Z=1)
        case 0x1:
            flag_offset   = offsetof(bal_cpu_t, flag_z);
            test_for_zero = true;
            break; // NE (Z=0)
        case 0x2:
            flag_offset   = offsetof(bal_cpu_t, flag_c);
            test_for_zero = false;
            break; // CS/HS (C=1)
        case 0x3:
            flag_offset   = offsetof(bal_cpu_t, flag_c);
            test_for_zero = true;
            break; // CC/LO (C=0)
        case 0x4:
            flag_offset   = offsetof(bal_cpu_t, flag_n);
            test_for_zero = false;
            break; // MI (N=1)
        case 0x5:
            flag_offset   = offsetof(bal_cpu_t, flag_n);
            test_for_zero = true;
            break; // PL (N=0)
        case 0x6:
            flag_offset   = offsetof(bal_cpu_t, flag_v);
            test_for_zero = false;
            break; // VS (V=1)
        case 0x7:
            flag_offset   = offsetof(bal_cpu_t, flag_v);
            test_for_zero = true;
            break; // VC (V=0)
        case 0xE:  // AL (always)
            flush_dirty_registers(compiler);
            bal_sliding_window_flush_all(&compiler->window);
            bal_x86_emit_mov_r64_imm64(assembler, BAL_X86_RAX, taken_pc);
            bal_x86_emit_store_r64_rbp_offset(assembler, BAL_X86_RAX, offsetof(bal_cpu_t, pc));
            bal_x86_emit_pop_r64(assembler, BAL_X86_RBP);
            bal_x86_emit_ret(assembler);
            return;
        default:
            BAL_LOG_ERROR(&bal_thread_logger, "Unsupported B.cond condition: %u", cond);
            compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
            return;
    }

    bal_sliding_window_flush_all(&compiler->window);
    bal_x86_emit_cmp_mem8_rbp_offset_imm(assembler, flag_offset, 0);
    const bal_x86_condition_t x86_cond = test_for_zero ? BAL_X86_COND_E : BAL_X86_COND_NE;
    terminate_block_conditional(compiler, x86_cond, fallthrough_pc, taken_pc);
}

void
translate_cbz_cbnz(bal_tier1_compiler_t *BAL_RESTRICT                     compiler,
                   const bal_decoder_instruction_metadata_t *BAL_RESTRICT metadata,
                   const uint32_t                                         instruction,
                   const bal_guest_address_t                              guest_address,
                   const bool                                             is_cbnz)
{
    const uint8_t  rn        = (uint8_t)extract_operand_value(instruction, &metadata->operands[0]);
    const uint32_t imm19     = extract_operand_value(instruction, &metadata->operands[1]);
    const uint32_t bit_width = metadata->operands[1].bit_width;
    const uint32_t shift     = 32U - bit_width;

    const int32_t             signed_immediate = (int32_t)(imm19 << shift) >> shift;
    const int64_t             offset           = (int64_t)signed_immediate * 4;
    const bal_guest_address_t taken_pc         = guest_address + (uint64_t)offset;
    const bal_guest_address_t fallthrough_pc   = guest_address + 4;
    const bool                skip_load_rn     = false;
    const bal_x86_register_t  x86_rn           = allocate_x86_register(compiler, rn, skip_load_rn);
    const bal_x86_macro_t     test_macro       = {
                  .opcode      = BAL_X86_MACRO_TEST_REGISTER_REGISTER,
                  .destination = x86_rn,
                  .source      = x86_rn,
    };
    bal_sliding_window_push(&compiler->window, test_macro);
    terminate_block_conditional(
        compiler, true == is_cbnz ? BAL_X86_COND_NE : BAL_X86_COND_E, fallthrough_pc, taken_pc);
}

void
translate_jump(const bal_tier1_compiler_t *BAL_RESTRICT               compiler,
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
        &bal_thread_logger, "Translated JUMP to 0x%016llX", (unsigned long long)*target_pc);

    // WARNING: Prevents unsued local variable compiler warning.
    (void)compiler;
}

void
translate_and(bal_tier1_compiler_t *BAL_RESTRICT                     compiler,
              const bal_decoder_instruction_metadata_t *BAL_RESTRICT metadata,
              const uint32_t                                         instruction)
{
    const bal_decoder_operand_t *BAL_RESTRICT operands = metadata->operands;
    const uint8_t  rd           = (uint8_t)extract_operand_value(instruction, &operands[0]);
    const uint8_t  rn           = (uint8_t)extract_operand_value(instruction, &operands[1]);
    const uint32_t shift_amount = extract_operand_value(instruction, &operands[2]);
    const uint8_t  rm           = (uint8_t)extract_operand_value(instruction, &operands[3]);

    if (BAL_UNLIKELY('S' == metadata->name[3]))
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Aborting function: Tier 1 does not support ANDS yet: %s",
                      metadata->name);
        compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
        return;
    }

    if (BAL_UNLIKELY(shift_amount != 0))
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Aborting function: Tier 1 does not support shift amounts != 0 yet: %s",
                      metadata->name);
        compiler->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
        return;
    }

    const bool               skip_load_rn = false;
    const bal_x86_register_t x86_rn       = allocate_x86_register(compiler, rn, skip_load_rn);
    const bool               skip_load_rm = false;
    const bal_x86_register_t x86_rm       = allocate_x86_register(compiler, rm, skip_load_rm);
    const bool               skip_load_rd = true;
    const bal_x86_register_t x86_rd       = allocate_x86_register(compiler, rd, skip_load_rd);

    if (BAL_UNLIKELY(BAL_X86_INVALID == x86_rd) || BAL_UNLIKELY(BAL_X86_INVALID == x86_rn)
        || BAL_UNLIKELY(BAL_X86_INVALID == x86_rm))
    {
        compiler->status = BAL_ERROR_INCORRECT_REGISTER_TYPE;
        return;
    }

    bal_x86_register_t x86_and_source = x86_rm;

    if (31 == rd)
    {
        const bal_x86_macro_t mov_macro = {
            .opcode              = BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE,
            .destination         = x86_rd,
            .immediate_or_offset = 0,
        };
        bal_sliding_window_push(&compiler->window, mov_macro);
    }
    else if (x86_rd == x86_rm)
    {
        x86_and_source = x86_rn;
    }
    else if (x86_rd != x86_rn)
    {
        const bal_x86_macro_t mov_macro = {
            .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
            .destination = x86_rd,
            .source      = x86_rn,
        };
        bal_sliding_window_push(&compiler->window, mov_macro);
    }

    const bal_x86_macro_t and_macro = {
        .opcode      = BAL_X86_MACRO_AND_REGISTER_REGISTER,
        .destination = x86_rd,
        .source      = x86_and_source,
    };
    bal_sliding_window_push(&compiler->window, and_macro);
    compiler->is_dirty |= 1U << rd;
}

void
translate_mov_immediate(bal_tier1_compiler_t                     *compiler,
                        const bal_decoder_instruction_metadata_t *metadata,
                        const uint32_t                            instruction)
{
    const uint8_t  rd      = (uint8_t)extract_operand_value(instruction, &metadata->operands[0]);
    const uint64_t imm16   = extract_operand_value(instruction, &metadata->operands[1]);
    const uint64_t hw      = extract_operand_value(instruction, &metadata->operands[2]);
    const uint64_t shift   = hw * 16ULL;
    const uint64_t mask    = metadata->operands[0].type == BAL_OPERAND_TYPE_REGISTER_32
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
        compiler->is_dirty |= 1U << rd;
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
        compiler->is_dirty |= 1U << rd;
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

    compiler->is_dirty |= 1U << rd;
}

void
translate_mov_register(bal_tier1_compiler_t *BAL_RESTRICT                     compiler,
                       const bal_decoder_instruction_metadata_t *BAL_RESTRICT metadata,
                       uint32_t                                               instruction)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: compiler is NULL.");
        return;
    }

    if (BAL_UNLIKELY(NULL == metadata))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: metadata is NULL.");
        compiler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(compiler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: compiler->status != BAL_SUCCESS.");
        return;
    }

    const bal_decoder_operand_t *BAL_RESTRICT operand_cursor = metadata->operands;
    const uint8_t rd = (uint8_t)extract_operand_value(instruction, &operand_cursor[0]);
    const uint8_t rm = (uint8_t)extract_operand_value(instruction, &operand_cursor[1]);

    const bool               skip_load_rm = false;
    const bal_x86_register_t x86_rm       = allocate_x86_register(compiler, rm, skip_load_rm);
    const bool               skip_load_rd = true;
    const bal_x86_register_t x86_rd       = allocate_x86_register(compiler, rd, skip_load_rd);

    const bal_x86_macro_t mov_macro = {
        .opcode      = BAL_X86_MACRO_MOV_REGISTER_REGISTER,
        .destination = x86_rd,
        .source      = x86_rm,
    };
    bal_sliding_window_push(&compiler->window, mov_macro);
    compiler->is_dirty |= 1U << rd;
}
