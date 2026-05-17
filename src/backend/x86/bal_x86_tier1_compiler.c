#include "backend/x86/bal_x86_tier1_compiler.h"
#include "backend/bal_cpu.h"
#include "bal_decoder.h"
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
    = { BAL_X86_RAX, BAL_X86_RCX, BAL_X86_RDX, BAL_X86_R8, BAL_X86_R9, BAL_X86_R10, BAL_X86_R11 };

#define SCRATCH_REGISTERS_SIZE (sizeof(SCRATCH_REGISTERS) / sizeof(SCRATCH_REGISTERS[0]))

static void               reset_register_allocator(bal_tier1_compiler_t *compiler);
static bal_x86_register_t allocate_x86_register(bal_tier1_compiler_t *compiler,
                                                uint8_t               arm_register);
static void               flush_dirty_registers(bal_tier1_compiler_t *compiler);
static void               terminate_block(bal_tier1_compiler_t *compiler);
static uint32_t extract_operand_value(uint32_t instruction, const bal_decoder_operand_t *operand);
static void     translate_movz(bal_tier1_compiler_t *compiler, const uint32_t *arm_operands);

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

void *
bal_tier1_compiler_translate(bal_tier1_compiler_t         *compiler,
                             const bal_memory_interface_t *memory_interface,
                             bal_guest_address_t           guest_address,
                             const size_t                  max_instructions)
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
    bal_x86_emit_push_r64(&compiler->assembler, BAL_X86_RBX);
    bal_x86_emit_mov_r64_r64(&compiler->assembler, BAL_X86_RBP, BAL_X86_ABI_ARG1);

    bool     is_block_terminated                         = false;
    uint32_t arm_instruction_operands[BAL_OPERANDS_SIZE] = { 0 };

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

            const bal_decoder_operand_t *BAL_RESTRICT operands_cursor = metadata->operands;
            uint32_t *BAL_RESTRICT arm_operands_cursor                = arm_instruction_operands;

            for (size_t ii = 0; ii < BAL_OPERANDS_SIZE; ++ii)
            {
                *arm_operands_cursor++ = extract_operand_value(instruction, operands_cursor);
                ++operands_cursor;
            }

            BAL_LOG_TRACE(logger,
                          "[0X%016llX] %08x : %s",
                          (unsigned long long)guest_address,
                          instruction,
                          metadata->name);

            switch (metadata->ir_opcode)
            {
                case OPCODE_CONST:
                    // TODO: implement support for the rest MOV instructions.
                    translate_movz(compiler, arm_instruction_operands);
                    break;
                case OPCODE_RETURN:
                    BAL_LOG_DEBUG(logger, "Block terminated by RET");
                    is_block_terminated = true;
                    break;
                default:
                    BAL_LOG_ERROR(logger, "Tier 1 Unsupported Opcode: %s", metadata->name);
                    is_block_terminated = true;
                    break;
            }

            if (BAL_UNLIKELY(compiler->status != BAL_SUCCESS))
            {
                BAL_LOG_ERROR(logger, "Status failure: %d", compiler->status);
                is_block_terminated = true;
                break;
            }

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
    terminate_block(compiler);
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
allocate_x86_register(bal_tier1_compiler_t *compiler, uint8_t arm_register)
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

    BAL_LOG_DEBUG(&compiler->logger,
                  "RegAlloc: Miss - mapped ARM X%u to x86 r%d",
                  arm_register,
                  free_register);
    compiler->arm_to_x86[arm_register]        = (int8_t)free_register;
    compiler->x86_to_arm[free_register]       = (int8_t)arm_register;
    const uint64_t        offset              = offsetof(bal_cpu_t, x[arm_register]);
    const bal_x86_macro_t load_register_macro = {
        .opcode              = BAL_X86_MACRO_LOAD,
        .destination         = free_register,
        .immediate_or_offset = offset,
    };

    bal_sliding_window_push(&compiler->window, load_register_macro);
    BAL_LOG_DEBUG(&compiler->logger, "RegAlloc: emitted LOAD macro for ARM X%u", arm_register);
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
terminate_block(bal_tier1_compiler_t *compiler)
{
    if (BAL_UNLIKELY(NULL == compiler))
    {
        return;
    }

    BAL_LOG_DEBUG(&compiler->logger, "Terminating basic block");
    flush_dirty_registers(compiler);
    BAL_LOG_TRACE(&compiler->logger, "Flushing sliding window");
    bal_sliding_window_flush_all(&compiler->window);
    BAL_LOG_TRACE(&compiler->logger, "Restoring host frame pointer and emitting RET");
    bal_x86_emit_pop_r64(&compiler->assembler, BAL_X86_RBX);
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
translate_movz(bal_tier1_compiler_t *compiler, const uint32_t *arm_operands)
{
    const uint8_t  rd    = (uint8_t)arm_operands[0];
    const uint64_t imm16 = arm_operands[1];
    const uint64_t hw    = arm_operands[2];
    const uint64_t shift = hw * 16;

    const uint64_t           value     = imm16 << shift;
    const bal_x86_register_t x86_rd    = allocate_x86_register(compiler, rd);
    const bal_x86_macro_t    mov_macro = {
        .opcode              = BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE,
        .destination         = x86_rd,
        .immediate_or_offset = value,
    };
    bal_sliding_window_push(&compiler->window, mov_macro);
    compiler->is_dirty[rd] = true;
}