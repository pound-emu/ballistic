#include "backend/x86/bal_x86_sliding_window.h"
#include "backend/bal_cpu.h"
#include <string.h>

#ifndef NDEBUG

// Strings MUST match the enum value order in bal_x86_macro_t.
static const char *const BAL_X86_MACRO_NAMES[] = {
    "NOP",
    "ADD_CPU_ICOUNT",
    "ADD_REGISTER_IMMEDIATE",
    "ADD_REGISTER_IMMEDIATE_32",
    "ADD_REGISTER_REGISTER",
    "ADD_REGISTER_REGISTER_32",
    "AND_REGISTER_IMMEDIATE",
    "AND_REGISTER_REGISTER",
    "JCC_RELATIVE",
    "JMP_REGISTER",
    "JMP_RELATIVE",
    "LOAD",
    "MOV_REGISTER_IMMEDIATE",
    "MOV_REGISTER_REGISTER",
    "OR_REGISTER_IMMEDIATE",
    "OR_REGISTER_REGISTER",
    "RET",
    "ROR_REGISTER_IMMEDIATE",
    "SAR_REGISTER_IMMEDIATE",
    "SETCC",
    "SHL_REGISTER_IMMEDIATE",
    "SHR_REGISTER_IMMEDIATE",
    "STORE",
    "SUB_REGISTER_IMMEDIATE",
    "SUB_REGISTER_IMMEDIATE_32",
    "SUB_REGISTER_REGISTER",
    "SUB_REGISTER_REGISTER_32",
    "TEST_REGISTER_REGISTER",
    "UD2",
    "XOR_REGISTER_REGISTER",
};

static inline const char *
bal_x86_macro_opcode_to_string(const bal_x86_macro_opcode_t opcode)
{
    const size_t num_names = sizeof(BAL_X86_MACRO_NAMES) / sizeof(BAL_X86_MACRO_NAMES[0]);

    if ((size_t)opcode < num_names && BAL_X86_MACRO_NAMES[opcode] != NULL)
    {
        return BAL_X86_MACRO_NAMES[opcode];
    }

    return "UNKNOWN MACRO";
}

#endif // NDEBUG

#define ASSEMBLER_TEMPORARY_REGISTER BAL_X86_R11

BAL_HOT static void flush_single_macro(bal_x86_assembler_t   *assembler,
                                       const bal_x86_macro_t *macro);
BAL_HOT static void run_peephole_optimizer(bal_sliding_window_t *window);

bal_error_t
bal_sliding_window_init(bal_sliding_window_t *BAL_RESTRICT window,
                        bal_x86_assembler_t *BAL_RESTRICT  assembler)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(NULL == window))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: window is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return assembler->status;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: assembler status != BAL_SUCCESS");
        return assembler->status;
    }

    BAL_LOG_INFO(&bal_thread_logger, "Initializing sliding window");
    memset(window, 0, sizeof(*window));
    window->assembler = assembler;
    return BAL_SUCCESS;
}

void
bal_sliding_window_reset(bal_sliding_window_t *window)
{
    if (BAL_UNLIKELY(NULL == window))
    {
        return;
    }

    window->count = 0;
    (void)memset(window->macros, 0, sizeof(*window->macros));
}

void
bal_sliding_window_push(bal_sliding_window_t *BAL_RESTRICT window, const bal_x86_macro_t macro)
{
    if (BAL_UNLIKELY(NULL == window))
    {
        return;
    }

    bal_x86_assembler_t *BAL_RESTRICT assembler = window->assembler;

    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    size_t window_count = window->count;

    if (BAL_UNLIKELY(window_count == BAL_SLIDING_WINDOW_CAPACITY))
    {
        BAL_LOG_DEBUG(&bal_thread_logger,
                      "Sliding window capacity reached (%d), flushing macros",
                      BAL_SLIDING_WINDOW_CAPACITY);
        const bal_x86_macro_t *BAL_RESTRICT macro_cursor = window->macros;

        for (size_t i = 0; i < window_count; ++i)
        {
            flush_single_macro(assembler, macro_cursor++);
        }

        window_count = 0;
    }

#ifndef NDEBUG

    BAL_LOG_DEBUG(&bal_thread_logger,
                  "Pushing macro opcode %s (dest: r%d, src: r%d, imm/off: 0x%llX)",
                  bal_x86_macro_opcode_to_string(macro.opcode),
                  macro.destination,
                  macro.source,
                  // WARNING: C standard guarantees unsigned long long is at least 64 bits wide.
                  (unsigned long long)macro.immediate_or_offset);

#endif // NDEBUG

    window->macros[window_count++] = macro;
    window->count                  = window_count;
    run_peephole_optimizer(window);
}

void
bal_sliding_window_flush_all(bal_sliding_window_t *BAL_RESTRICT window)
{
    if (BAL_UNLIKELY(NULL == window))
    {
        return;
    }

    bal_x86_assembler_t *BAL_RESTRICT assembler = window->assembler;

    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    const size_t                        window_count = window->count;
    const bal_x86_macro_t *BAL_RESTRICT macro_cursor = window->macros;

    for (size_t i = 0; i < window_count; ++i)
    {
        flush_single_macro(assembler, macro_cursor++);
    }

    window->count = 0;
}

void
flush_single_macro(bal_x86_assembler_t *BAL_RESTRICT   assembler,
                   const bal_x86_macro_t *BAL_RESTRICT macro)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == macro))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: macro is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: assembler status != BAL_SUCCESS");
        return;
    }

    const bal_x86_macro_opcode_t opcode              = macro->opcode;
    const bal_x86_register_t     destination         = macro->destination;
    const bal_x86_register_t     source              = macro->source;
    const uint64_t               immediate_or_offset = macro->immediate_or_offset;

    switch (opcode)
    {
        case BAL_X86_MACRO_NOP:
            BAL_LOG_DEBUG(&bal_thread_logger, "Flush skipped: NOP macro");
            break;
        case BAL_X86_MACRO_ADD_CPU_ICOUNT:
            // WARNING: Cast of offset (size_t) to int32_t is safe because the bal_cpu_t struct
            // size is far below the limits of a 32-bit signed integer.
            bal_x86_emit_add_mem64_rbp_offset_imm(
                assembler, offsetof(bal_cpu_t, instruction_count), (int32_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_ADD_REGISTER_IMMEDIATE:
            // WARNING: The immediate is at most 24-bits.
            bal_x86_emit_add_r64_imm32(assembler, destination, (int32_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_ADD_REGISTER_REGISTER:
            bal_x86_emit_add_r64_r64(assembler, destination, source);
            break;
        case BAL_X86_MACRO_AND_REGISTER_IMMEDIATE:
            bal_x86_emit_mov_r64_imm64(
                assembler, ASSEMBLER_TEMPORARY_REGISTER, immediate_or_offset);
            bal_x86_emit_and_r64_r64(assembler, destination, ASSEMBLER_TEMPORARY_REGISTER);
            break;
        case BAL_X86_MACRO_AND_REGISTER_REGISTER:
            bal_x86_emit_and_r64_r64(assembler, destination, source);
            break;
        case BAL_X86_MACRO_JCC_RELATIVE:
            if (BAL_LIKELY(immediate_or_offset <= INT32_MAX))
            {
                // WARNING: Check confirms offset can fit in int32_t.
                bal_x86_emit_jcc_rel32(assembler, macro->condition, (int32_t)immediate_or_offset);
            }
            else
            {
                BAL_LOG_ERROR(&bal_thread_logger,
                              "[+0x%04zu] Relative jump offset 0x%X out of bounds for `jcc rel32`",
                              assembler->offset,
                              immediate_or_offset);
                assembler->status = BAL_ERROR_BRANCH_OFFSET_OVERFLOW;
            }
            break;
        case BAL_X86_MACRO_JMP_REGISTER:
            bal_x86_emit_jmp_r64(assembler, destination);
            break;
        case BAL_X86_MACRO_JMP_RELATIVE:
            if (BAL_LIKELY(immediate_or_offset <= INT32_MAX))
            {
                // WARNING: Check confirms offset can fit in int32_t.
                bal_x86_emit_jmp_rel32(assembler, (int32_t)immediate_or_offset);
            }
            else
            {
                BAL_LOG_ERROR(&bal_thread_logger,
                              "[+0x%04zu] Relative jump offset 0x%X out of bounds for `jmp rel32`",
                              assembler->offset,
                              immediate_or_offset);
                assembler->status = BAL_ERROR_BRANCH_OFFSET_OVERFLOW;
            }
            break;
        case BAL_X86_MACRO_LOAD:
            // WARNING: The displacement offset refers to the structural members within bal_cpu_t.
            // This struct is statically bounded to 264 bytes, which fits inside a signed 32-bit
            // integer.
            bal_x86_emit_load_r64_rbp_offset(assembler, destination, (int32_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE:
            bal_x86_emit_mov_r64_imm64(assembler, destination, immediate_or_offset);
            break;
        case BAL_X86_MACRO_MOV_REGISTER_REGISTER:
            bal_x86_emit_mov_r64_r64(assembler, destination, source);
            break;
        case BAL_X86_MACRO_OR_REGISTER_IMMEDIATE:
            bal_x86_emit_mov_r64_imm64(
                assembler, ASSEMBLER_TEMPORARY_REGISTER, immediate_or_offset);
            bal_x86_emit_or_r64_r64(assembler, destination, ASSEMBLER_TEMPORARY_REGISTER);
            break;
        case BAL_X86_MACRO_OR_REGISTER_REGISTER:
            bal_x86_emit_or_r64_r64(assembler, destination, source);
            break;
        case BAL_X86_MACRO_ROR_REGISTER_IMMEDIATE:
            bal_x86_emit_ror_r64_imm8(assembler, destination, (uint8_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_SAR_REGISTER_IMMEDIATE:
            bal_x86_emit_sar_r64_imm8(assembler, destination, (uint8_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_SETCC:
            bal_x86_emit_setcc_mem8_rbp_offset(
                assembler, macro->condition, (int32_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_SHL_REGISTER_IMMEDIATE:
            bal_x86_emit_shl_r64_imm8(assembler, destination, (uint8_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_SHR_REGISTER_IMMEDIATE:
            bal_x86_emit_shr_r64_imm8(assembler, destination, (uint8_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_STORE:
            // WARNING: The displacement offset refers to the structural members within bal_cpu_t.
            // This struct is statically bounded to 264 bytes, which fits inside a signed 32-bit
            // integer.
            bal_x86_emit_store_r64_rbp_offset(assembler, source, (int32_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_SUB_REGISTER_IMMEDIATE:
            bal_x86_emit_sub_r64_imm32(assembler, destination, (int32_t)immediate_or_offset);
            break;
        case BAL_X86_MACRO_SUB_REGISTER_REGISTER:
            bal_x86_emit_sub_r64_r64(assembler, destination, source);
            break;
        case BAL_X86_MACRO_UD2:
            bal_x86_emit_ud2(assembler);
            break;
        case BAL_X86_MACRO_XOR_REGISTER_REGISTER:
            bal_x86_emit_xor_r64_r64(assembler, destination, source);
            break;
        default:
            BAL_LOG_ERROR(
                &bal_thread_logger, "Aborting function: Unknown x86 macro opcode: %d", opcode);
            break;
    }
}

void
run_peephole_optimizer(bal_sliding_window_t *BAL_RESTRICT window)
{
    if (BAL_UNLIKELY(NULL == window) || BAL_UNLIKELY(NULL == window->assembler))
    {
        return;
    }

    const size_t count = window->count;

    if (0 == count)
    {
        BAL_LOG_WARN(&bal_thread_logger, "Aborting function: window is empty");
        return;
    }

    if (BAL_UNLIKELY(window->assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: Assembler status != BAL_SUCCESS");
        return;
    }

    bal_x86_macro_t *BAL_RESTRICT macro1             = &window->macros[window->count - 1];
    const bal_x86_macro_opcode_t  macro1_opcode      = macro1->opcode;
    const bal_x86_register_t      macro1_destination = macro1->destination;
    const bal_x86_register_t      macro1_source      = macro1->source;

    // PEEPHOLE 1: Identity Move (mov rax, rax).
    if (BAL_UNLIKELY(BAL_X86_MACRO_MOV_REGISTER_REGISTER == macro1_opcode
                     && macro1_destination == macro1_source))
    {
        // WARNING: Prevents unsued local variable compiler warning.
        BAL_LOG_DEBUG(&bal_thread_logger,
                      "Peephole: Killed identity MOV (r%d -> r%d)",
                      macro1_source,
                      macro1_destination);
        macro1->opcode = BAL_X86_MACRO_NOP;
        return;
    }

    if (count < 2)
    {
        return;
    }

    bal_x86_macro_t *BAL_RESTRICT macro2             = &window->macros[window->count - 2];
    const bal_x86_macro_opcode_t  macro2_opcode      = macro2->opcode;
    const bal_x86_register_t      macro2_destination = macro2->destination;
    const bal_x86_register_t      macro2_source      = macro2->source;

    // PEEPHOLE 2: Redundant MOV (1: mov rax, rbx. 2: mov rax, rbx).
    if (BAL_UNLIKELY(BAL_X86_MACRO_MOV_REGISTER_REGISTER == macro1_opcode
                     && BAL_X86_MACRO_MOV_REGISTER_REGISTER == macro2_opcode))
    {
        if (macro1_destination == macro2_destination && macro1_source == macro2_source)
        {
            BAL_LOG_DEBUG(&bal_thread_logger,
                          "Peephole: Killed redundant MOV (r%d -> r%d)",
                          macro1_source,
                          macro1_destination);
            macro2->opcode = BAL_X86_MACRO_NOP;
        }
    }
}
