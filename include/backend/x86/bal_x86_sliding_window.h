#ifndef BALLISTIC_BAL_X86_SLIDING_WINDOW_H
#define BALLISTIC_BAL_X86_SLIDING_WINDOW_H

#include "backend/x86/bal_x86_assembler.h"
#include "bal_assembler.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/// The maximum number of macros held in the sliding window.
#define BAL_SLIDING_WINDOW_CAPACITY 4

    typedef enum
    {
        BAL_X86_MACRO_NOP = 0,
        BAL_X86_MACRO_AND_REGISTER_IMMEDIATE,
        BAL_X86_MACRO_OR_REGISTER_IMMEDIATE,
        BAL_X86_MACRO_MOV_REGISTER_IMMEDIATE,
        BAL_X86_MACRO_MOV_REGISTER_REGISTER,
        BAL_X86_MACRO_LOAD,
        BAL_X86_MACRO_STORE,
        BAL_X86_MACRO_RET,
    } bal_x86_macro_opcode_t;

    /// Represents a single un-lowered x86 instruction inside the sliding window.
    typedef struct
    {
        bal_x86_macro_opcode_t opcode;
        bal_x86_register_t     destination;
        bal_x86_register_t     source;
        uint64_t               immediate_or_offset;
    } bal_x86_macro_t;

    /// The Sliding Window Context.
    ///
    /// This struct acts as a middleware between the high level Tier 1/Tier 2 compilers and the
    /// low-level `[bal_x86_assembler_t]`.
    ///
    /// # Warning
    ///
    /// Do not share an instance of this struct across threads to prevent false sharing.
    typedef struct
    {
        /// x86 Assembler context.
        bal_x86_assembler_t *assembler;

        /// Ring buffer holding the currently queued macros.
        bal_x86_macro_t macros[BAL_SLIDING_WINDOW_CAPACITY];

        /// The current number queued macros.
        size_t count;
    } bal_sliding_window_t;

    /// Initializes a new macro sliding window.
    ///
    /// # Safety
    /// `window` and `assembler must be valid and not `NULL`.
    /// `assembler` must remain valid for the entire lifetime of `window`.
    ///
    /// # Errors
    ///
    /// Returns [`BAL_SUCCESS`] on success.
    ///
    /// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if any pointer is `NULL`.
    BAL_COLD bal_error_t bal_sliding_window_init(bal_sliding_window_t *window,
                                                 bal_x86_assembler_t  *assembler);

    /// Pushes a new macro into the sliding window and triggers the peephole optimizer.
    ///
    /// # Safety
    ///
    /// `window` pointer must be initialized via [`bal_sliding_window_init`].
    BAL_HOT void bal_sliding_window_push(bal_sliding_window_t *window, bal_x86_macro_t macro);

    /// Flushes all remaining macros in the window to the executable buffer in `window->assembler`.
    ///
    /// This must
    BAL_HOT void bal_sliding_window_flush_all(bal_sliding_window_t *window);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // BALLISTIC_BAL_X86_SLIDING_WINDOW_H

/***  end of file ***/
