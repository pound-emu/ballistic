//! This module provides a low-level interface for generating ARM64 instructions into a
//! pre-allocated memory buffer.
//!
//! # Examples
//!
//! ```c
//! #include "bal_logging.h"
//! #include "bal_assembler.h"
//!
//! uint32_t code[128];
//! bal_assembler_t assembler;
//! bal_logger_t logger = {0};
//!
//! bal_error_t error = bal_assembler_init(&assembler, code, 128, logger);
//! if (error == BAL_SUCCESS)
//! {
//!     // MOV X0, #42
//!     bal_emit_movz(&assembler, BAL_REGISTER_X0, 42, 0);
//! }
//! ```

#ifndef BALLISTIC_ASSEMBLER_H
#define BALLISTIC_ASSEMBLER_H

#include "bal_errors.h"
#include "bal_logging.h"
#include "bal_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /// This struct manages a linear buffer of 32-bit integers where ARM64 machine code is written.
    /// It tracks the current write position and handles boundary checking.
    typedef struct
    {
        /// A pointer to the start of the code buffer.
        uint32_t *buffer;

        /// The maximum number of instructions that can fit in the buffer.
        size_t capacity;

        /// The current write index within the buffer.
        size_t offset;

        /// The logging context used to report details and errors.
        bal_logger_t logger;

        /// The current error state of the assembler.
        ///
        /// Once this is set to anything other than [`BAL_SUCCESS`], all subsequent emit calls will
        /// be ignored until the assembler is reset.
        bal_error_t status;

        uint32_t pad;
    } bal_assembler_t;

    /// Initializes the assembler with a specific memory buffer and the size of the buffer in
    /// `uint32_t` elements.
    ///
    /// # Returns
    ///
    /// * [`BAL_SUCCESS`] on success.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if `assembler` or `buffer` is NULL.
    /// * [`BAL_ERROR_MEMORY_ALIGNMENT`] if `buffer` is not 4-byte aligned.
    bal_error_t bal_assembler_init(bal_assembler_t *assembler,
                                   void            *buffer,
                                   size_t           size,
                                   bal_logger_t     logger);

    /// Resets the assembler back to its initial state.
    ///
    /// # Safety
    ///
    /// `assembler` must be valid.
    void bal_assembler_reset(bal_assembler_t *assembler);

    /// Emit a `ADD` (Immediate) instruction.
    ///
    /// Adds a register value `rn` and 12 bit immediate value `imm12` with the optional left shift
    /// `sh` to apply to the immediate, and writes the result to the destination register `rd`.
    ///
    /// # Safety
    ///
    /// * `rn` will be truncated if it exceeds 5 bits.
    /// * `imm12` will be truncated if it exceeds 12 bits.
    /// * `sh` must have the value `0`, or `1`
    /// * Function does not emit instructions if `assembler->status != BALL_SUCCESS`.
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity`.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_add_immediate(bal_assembler_t     *assembler,
                                bal_register_index_t rd,
                                uint8_t              rn,
                                uint16_t             imm12,
                                uint8_t              shift);

    /// Emits a `ADD` (Shifted Register) instruction.
    ///
    /// Adds a register value `rn` and a shifted register `rm`, and writes the result to `rd`.
    ///
    /// # Safety
    ///
    /// * `rd`, `rn`, `rm` must be valid registers (0-31).
    /// * `shift` must be between 0 and 63.
    /// * `shift_type` must be 0 (LSL), 1 (LSR), or 2 (ASR).
    /// * Function does not emit instructions if `assembler->status` != [`BAL_SUCCESS`].
    void bal_emit_add_shifted_register(bal_assembler_t     *assembler,
                                       bal_register_index_t rd,
                                       bal_register_index_t rn,
                                       bal_register_index_t rm,
                                       uint8_t              shift,
                                       uint8_t              shift_type);

    /// Emits a `B` (Branch) instruction.
    ///
    /// Branches unconditionally to a PC-relative offset.
    ///
    /// # Safety
    ///
    /// * `offset` will be truncated if it exceeds 26 bits.
    /// *  Does not emit if `assembler->status` != [`BAL_SUCCESS`]
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity`.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_b(bal_assembler_t *assembler, int32_t offset);

    /// Emits a `BR` (Branch Register) instruction.
    ///
    /// Branches unconditionally to an address in a register.
    ///
    /// # Safety
    ///
    /// * `rn` must be between 0-31 inclusive.
    /// * Does not emit if `assembler->status` != [`BAL_SUCCESS`]
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_br(bal_assembler_t *assembler, bal_register_index_t rn);

    /// Emit a `SUB` (Immediate) instruction.
    ///
    /// Subtracts a 12 bit immediate value `imm12` with the optional left shift `sh` from register
    /// value `rn`, and writes the result to the destination register `rd`.
    ///
    /// # Safety
    ///
    /// * `rn` will be truncated if it exceeds 5 bits.
    /// * `imm12` will be truncated if it exceeds 12 bits.
    /// * `sh` must have the value `0`, or `1`
    /// * Function does not emit instructions if `assembler->status != BAL_SUCCESS`.
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity`.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_sub_immediate(bal_assembler_t     *assembler,
                                bal_register_index_t rd,
                                uint8_t              rn,
                                uint16_t             imm12,
                                uint8_t              shift);

    /// Emit a `MOVZ` (Move Wide with Zero) instruction.
    ///
    /// Moves a 16-bit immediate into a register, shifting it left by 0, 16, 32, or 48 bits, and
    /// the rest of the register to zero.
    ///
    /// # Safety
    ///
    /// * `shift` must be 0, 16, 32, or 48.
    /// * Does not emit if `assembler->status != BAL_SUCCESS`.
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity`.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_movz(bal_assembler_t     *assembler,
                       bal_register_index_t rd,
                       uint16_t             imm,
                       uint8_t              shift);

    /// Emits a `MOVK` (Move Wide with Keep) instruction.
    ///
    /// Moves a 16-bit immediate into a specific 16-bit field of a register, leaving the other bits
    /// unchanged.
    ///
    /// # Safety
    ///
    /// * `shift` must be 0, 16, 32, or 48.
    /// * Does not emit if `assembler->status != BAL_SUCCESS`.
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity`.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_movk(bal_assembler_t     *assembler,
                       bal_register_index_t rd,
                       uint16_t             imm,
                       uint8_t              shift);

    /// Emits a `MOVN` (Move Wide with Not) instruction.
    ///
    /// Moves the bitwise inverse of a 16-bit immediate (shifted left) into a register, setting all
    /// other bits to 1.
    ///
    /// # Safety
    ///
    /// * `shift` must be 0, 16, 32, or 48.
    /// * Does not emit if `assembler->status != BAL_SUCCESS`.
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity`.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_movn(bal_assembler_t     *assembler,
                       bal_register_index_t rd,
                       uint16_t             imm,
                       uint8_t              shift);

    /// Emits a `RET` instruction.
    ///
    /// Returns from subroutine branches unconditionally to an address in a register with a hint
    /// that this is a subroutine return.
    ///
    /// # Safety
    ///
    /// * `rn` must be between 0-31 inclusive.
    /// * Does not emit if `assembler->status != BAL_SUCCESS`.
    ///
    /// # Errors
    ///
    /// Modifies `assembler->status` to the following if an error occurs:
    ///
    /// * [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if `assembler->offset >= assembler->capacity`.
    /// * [`BAL_ERROR_INVALID_ARGUMENT`] if function arguments are invalid.
    void bal_emit_ret(bal_assembler_t *assembler, bal_register_index_t rn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BALLISTIC_ASSEMBLER_H */

/*** end of file ***/
