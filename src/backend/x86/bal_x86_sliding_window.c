#include "backend/x86/bal_x86_sliding_window.h"
#include <string.h>

#define ASSEMBLER_TEMPORARY_REGISTER BAL_X86_R11

BAL_HOT static void flush_single_macro(bal_x86_assembler_t   *assembler,
                                       const bal_x86_macro_t *macro);
BAL_HOT static void run_peephole_optimizer(bal_sliding_window_t *window);

bal_error_t
bal_sliding_window_init(bal_sliding_window_t *window, bal_x86_assembler_t *assembler)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(NULL == window))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: window is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return assembler->status;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler status != BAL_SUCCESS");
        return assembler->status;
    }

    BAL_LOG_INFO(&assembler->logger, "Initializing sliding window");
    memset(window, 0, sizeof(*window));
    window->assembler = assembler;
    return BAL_SUCCESS;
}
void
bal_sliding_window_push(bal_sliding_window_t *window, const bal_x86_macro_t macro)
{
    if (BAL_UNLIKELY(NULL == window))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == window->assembler))
    {
        return;
    }

    size_t window_count = window->count;

    if (BAL_UNLIKELY(window_count == BAL_SLIDING_WINDOW_CAPACITY))
    {
        BAL_LOG_DEBUG(&window->assembler->logger,
                      "Sliding window capacity reached (%d), flushing macros",
                      BAL_SLIDING_WINDOW_CAPACITY);
        const bal_x86_macro_t *macro_cursor = window->macros;

        for (size_t i = 0; i < window_count; ++i)
        {
            flush_single_macro(window->assembler, macro_cursor++);
        }

        window_count = 0;
    }

    BAL_LOG_DEBUG(&window->assembler->logger,
                  "Pushing macro opcode id %d (dest: r%d, src: r%d, imm/off: 0x%llX)",
                  macro.opcode,
                  macro.destination,
                  macro.source,
                  (unsigned long long)macro.immediate_or_offset);
    window->macros[window_count++] = macro;
    window->count                  = window_count;
    run_peephole_optimizer(window);
}

void
bal_sliding_window_flush_all(bal_sliding_window_t *window)
{
    if (NULL == window)
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == window->assembler))
    {
        return;
    }

    const size_t           window_count = window->count;
    const bal_x86_macro_t *macro_cursor = window->macros;

    for (size_t i = 0; i < window_count; ++i)
    {
        flush_single_macro(window->assembler, macro_cursor++);
    }

    window->count = 0;
}

void
flush_single_macro(bal_x86_assembler_t *assembler, const bal_x86_macro_t *macro)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == macro))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: macro is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler status != BAL_SUCCESS");
        return;
    }

    switch (macro->opcode)
    {
        case BAL_X86_MACRO_NOP:
            BAL_LOG_DEBUG(&assembler->logger, "Flush skipped: NOP macro");
            break;
        case BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE:
            bal_x86_emit_mov_r64_imm64(assembler, macro->destination, macro->immediate_or_offset);
            break;
        case BAL_X86_MACRO_LOAD:
            bal_x86_emit_load_r64_rbp_offset(
                assembler, macro->destination, (int32_t)macro->immediate_or_offset);
            break;
        case BAL_X86_MACRO_STORE:
            bal_x86_emit_store_r64_rbp_offset(
                assembler, macro->source, (int32_t)macro->immediate_or_offset);
            break;
        case BAL_X86_MACRO_AND_REGISTER_IMMEDIATE:
            bal_x86_emit_mov_r64_imm64(
                assembler, ASSEMBLER_TEMPORARY_REGISTER, macro->immediate_or_offset);
            bal_x86_emit_and_r64_r64(assembler, macro->destination, ASSEMBLER_TEMPORARY_REGISTER);
            break;
        case BAL_X86_MACRO_OR_REGISTER_IMMEDIATE:
            bal_x86_emit_mov_r64_imm64(
                assembler, ASSEMBLER_TEMPORARY_REGISTER, macro->immediate_or_offset);
            bal_x86_emit_or_r64_r64(assembler, macro->destination, ASSEMBLER_TEMPORARY_REGISTER);
            break;
        default:
            BAL_LOG_ERROR(&assembler->logger,
                          "Aborting function: Unknown x86 macro opcode: %d",
                          macro->opcode);
            break;
    }
}

void
run_peephole_optimizer(bal_sliding_window_t *window)
{
    if (BAL_UNLIKELY(NULL == window) || BAL_UNLIKELY(NULL == window->assembler))
    {
        return;
    }

    if (0 == window->count)
    {
        BAL_LOG_WARN(&window->assembler->logger, "Aborting function: window is empty");
        return;
    }

    if (BAL_UNLIKELY(window->assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&window->assembler->logger,
                      "Aborting function: Assembler status != BAL_SUCCESS");
        return;
    }

    bal_x86_macro_t *macro1 = &window->macros[window->count - 1];
    // PEEPHOLE 1: Identity Move (mov rax, rax).
    //
    if (BAL_X86_MACRO_MOV_REGISTER_REGISTER == macro1->opcode
        && macro1->destination == macro1->source)
    {
        BAL_LOG_DEBUG(&window->assembler->logger,
                      "Peephole: Killed identity MOV (r%d -> r%d)",
                      macro1->source,
                      macro1->destination);
        macro1->opcode = BAL_X86_MACRO_NOP;
        return;
    }

    if (window->count < 2)
    {
        return;
    }

    bal_x86_macro_t *macro2 = &window->macros[window->count - 1];

    // PEEPHOLE 2: Redundant MOV (1: mov rax, rbx. 2: mov rax, rbx).
    //
    if (BAL_X86_MACRO_MOV_REGISTER_REGISTER == macro1->opcode
        && BAL_X86_MACRO_MOV_REGISTER_REGISTER == macro2->opcode)
    {
        if (macro1->destination == macro2->destination && macro1->source == macro2->source)
        {
            BAL_LOG_DEBUG(&window->assembler->logger,
                          "Peephole: Killed redundant MOV (r%d -> r%d)",
                          macro1->source,
                          macro1->destination);
            macro2->opcode = BAL_X86_MACRO_NOP;
        }
    }
}