#ifndef BALLISTIC_BAL_X86_ASSEMBLER_H
#define BALLISTIC_BAL_X86_ASSEMBLER_H

#include "bal_errors.h"
#include "bal_logging.h"
#include "bal_memory.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    typedef enum
    {
        BAL_X86_INVALID = -1,

        /// Accumulator Register.
        BAL_X86_RAX = 0,

        /// Counter Register.
        BAL_X86_RCX,

        /// Data Register.
        BAL_X86_RDX,

        /// Base register.
        BAL_X86_RBX,

        /// Stack pointer.
        BAL_X86_RSP,

        /// Base pointer.
        BAL_X86_RBP,

        /// Source Index.
        BAL_X86_RSI,

        /// Destination Index.
        BAL_X86_RDI,

        /// Extended Register 8.
        BAL_X86_R8,

        /// Extended Register 9.
        BAL_X86_R9,

        /// Extended Register 10.
        BAL_X86_R10,

        /// Extended Register 11.
        BAL_X86_R11,

        /// Extended Register 12.
        BAL_X86_R12,

        /// Extended Register 13.
        BAL_X86_R13,

        /// Extended Register 14.
        BAL_X86_R14,

        /// Extended Register 15.
        BAL_X86_R15,
    } bal_x86_register_t;

    /// The x86-64 assembler state.
    typedef struct
    {
        /// Memory buffer where machine code bytes are written.
        uint8_t *buffer;

        /// Pointer to the executable view of the memory buffer.
        uint8_t *rx_buffer;

        /// Maximum number of bytes this assembler can write.
        size_t capacity;

        /// Current byte index in the buffer.
        size_t offset;
        /// Logger instance.
        bal_logger_t logger;

        /// Current error state. If this is not [`BAL_SUCCESS`], all emit calls will fail.
        bal_error_t status;
    } bal_x86_assembler_t;

    /// Initializes the x86-64 assembler with `executable_buffer`.
    ///
    /// # Safety
    ///
    /// `executable_buffer` must point to a valid allocation of at least `size` bytes. The buffer
    /// must also be configured with executable page permissions by the host OS.
    ///
    /// # Errors
    ///
    /// Returns [`BAL_SUCCESS`] on success.
    ///
    /// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if `assembler` or `executable_buffer` is `NULL` or
    /// `size` is 0.
    bal_error_t bal_x86_assembler_init(bal_x86_assembler_t    *assembler,
                                       bal_executable_buffer_t executable_buffer,
                                       size_t                  size,
                                       bal_logger_t            logger);

    /// Emits a bitwise AND instruction between two 64-bit registers.
    ///
    /// Assembly equivalent: `and destination, source`.
    ///
    /// # Safety
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_and_r64_r64(bal_x86_assembler_t *assembler,
                                  bal_x86_register_t   destination,
                                  bal_x86_register_t   source);

    /// Emits a memory load using the 32-bit displacement `offset`.
    ///
    /// Assembler equivalent: `mov destination, [rbp + offset]`
    ///
    /// # Warning
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_load_r64_rbp_offset(bal_x86_assembler_t *assembler,
                                          bal_x86_register_t   destination,
                                          int32_t              offset);

    /// Emits a memory store using a 32-bit displacement.
    ///
    /// Assembly equivalent: `mov[rbp + offset], reg64`
    ///
    /// # Warning
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_store_r64_rbp_offset(bal_x86_assembler_t *assembler,
                                           bal_x86_register_t   source,
                                           int32_t              offset);

    /// Emits an instruction to move 64-bit register `source` into `destination`.
    ///
    /// Assembly equivalent: `mov reg64, reg64`
    ///
    /// # Warning
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_mov_r64_r64(bal_x86_assembler_t *assembler,
                                  bal_x86_register_t   destination,
                                  bal_x86_register_t   source);

    /// Emits an instruction to move a 64-bit immediate value into a 64-bit register.
    ///
    /// Assembly equivalent: `mov destination, immediate`
    ///
    /// # Warning
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_mov_r64_imm64(bal_x86_assembler_t *assembler,
                                    bal_x86_register_t   destination,
                                    uint64_t             immediate);

    /// Emits a near return instruction.
    ///
    /// Assembly equivalent: `ret`
    ///
    /// # Warning
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_ret(bal_x86_assembler_t *assembler);

    /// Emits a bitwise OR instruction between two 64-bit registers.
    ///
    /// Assembly equivalent: `or destination, source`.
    ///
    /// # Safety
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_or_r64_r64(bal_x86_assembler_t *assembler,
                                 bal_x86_register_t   destination,
                                 bal_x86_register_t   source);

    /// Emits a Stack Push instruction.
    ///
    /// Assembly equivalent: `push reg64`
    ///
    /// # Warning
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_push_r64(bal_x86_assembler_t *assembler, bal_x86_register_t reg);

    /// Emits a Stack Pop instruction.
    ///
    /// Assembly equivalent: `push reg64`
    ///
    /// # Warning
    ///
    /// This function fails if:
    ///
    /// - `assembler` is `NULL`.
    /// - `assembler->status != [`BAL_SUCCESS`]
    /// - `assembler->buffer` is full.
    void bal_x86_emit_pop_r64(bal_x86_assembler_t *assembler, bal_x86_register_t reg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // BALLISTIC_BAL_X86_ASSEMBLER_H

/*** end of file ***/
