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
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    /// The register index for X0.
    BAL_REGISTER_X0 = 0,

    /// The register index for X1.
    BAL_REGISTER_X1 = 1,

    /// The register index for X2.
    BAL_REGISTER_X2 = 2,

    /// The register index for X3.
    BAL_REGISTER_X3 = 3,

    /// The register index for X4.
    BAL_REGISTER_X4 = 4,

    /// The register index for X5.
    BAL_REGISTER_X5 = 5,

    /// The register index for X6.
    BAL_REGISTER_X6 = 6,

    /// The register index for X7.
    BAL_REGISTER_X7 = 7,

    /// The register index for X8.
    BAL_REGISTER_X8 = 8,

    /// The register index for X9.
    BAL_REGISTER_X9 = 9,

    /// The register index for 10.
    BAL_REGISTER_X10 = 10,

    /// The register index for 11.
    BAL_REGISTER_X11 = 11,

    /// The register index for 12.
    BAL_REGISTER_X12 = 12,

    /// The register index for 13.
    BAL_REGISTER_X13 = 13,

    /// The register index for 14.
    BAL_REGISTER_X14 = 14,

    /// The register index for 15.
    BAL_REGISTER_X15 = 15,

    /// The register index for 16.
    BAL_REGISTER_X16 = 16,

    /// The register index for 17.
    BAL_REGISTER_X17 = 17,

    /// The register index for 18.
    BAL_REGISTER_X18 = 18,

    /// The register index for 19.
    BAL_REGISTER_X19 = 19,

    /// The register index for 20.
    BAL_REGISTER_X20 = 20,

    /// The register index for 21.
    BAL_REGISTER_X21 = 21,

    /// The register index for 22.
    BAL_REGISTER_X22 = 22,

    /// The register index for 23.
    BAL_REGISTER_X23 = 23,

    /// The register index for 24.
    BAL_REGISTER_X24 = 24,

    /// The register index for 25.
    BAL_REGISTER_X25 = 25,

    /// The register index for 26.
    BAL_REGISTER_X26 = 26,

    /// The register index for 27.
    BAL_REGISTER_X27 = 27,

    /// The register index for 28.
    BAL_REGISTER_X28 = 28,

    /// The register index for 29 (Frame Pointer).
    BAL_REGISTER_X29 = 29,

    /// The register index for 30 (Link Register).
    BAL_REGISTER_X30 = 30,

    /// The register index for the Zero Register (XZR).
    BAL_REGISTER_XZR = 31,
} bal_register_index_t;

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
    /// Once this is set to anything other than [`BAL_SUCCESS`], all subsequent emit calls will be
    /// ignored until the assembler is reset.
    bal_error_t status;
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

/// Emit a `MOVZ` (Move Wide with Zero) instruction.
///
/// Moves a 16-bit immediate into a register, shifting it left by 0, 16, 32, or 48 bits, and
/// the rest of the register to zero.
///
/// # Safety
///
/// * `shift` must be 0, 16, 32, or 48.
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
/// * `rd` muse be less than 32.
/// * `shift` must be 0, 16, 32, or 48.
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
/// * `rd` muse be less than 32.
/// * `shift` must be 0, 16, 32, or 48.
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

#endif /* BALLISTIC_ASSEMBLER_H */

/*** end of file ***/
