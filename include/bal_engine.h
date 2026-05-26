#ifndef BALLISTIC_ENGINE_H
#define BALLISTIC_ENGINE_H

#include "backend/bal_cpu.h"
#include "bal_attributes.h"
#include "bal_errors.h"
#include "bal_logging.h"
#include "bal_memory.h"
#include "bal_types.h"
#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/// A byte pattern written to memory during initialization, poisoning allocated
/// regions. This is mainly used for detecting reads from uninitialized memory.
#define POISON_UNINITIALIZED_MEMORY 0xFF

/// IR Instruction Bitfield Layout:
///
/// 63               51 50        34 33        17 16        00
/// |-----------------| |----------| |----------| |----------|
///        opc             src1         src2         src3

/// Opcode bitfield least significant bit.
#define BAL_OPCODE_SHIFT_POSITION 51U

/// Source1 bitfield least significant bit.
#define BAL_SOURCE1_SHIFT_POSITION 34U

/// Source2 bitfield least significant bit.
#define BAL_SOURCE2_SHIFT_POSITION 17U

/// The maximum value for an Opcode.
#define BAL_OPCODE_SIZE (1U << 11U)

/// The mask for any source field. This does not include the Is Constant flag at Bit 16.
#define BAL_SOURCE_MASK ((1U << 16U) - 1U)

/// The mask for any source bitfield. This includes the Is Constant flag at Bit 16.
/// Developers should clear Bit 16 after applying this mask to get the raw source.
#define BAL_SOURCE_MASK_WITH_FLAG ((1U << 17U) - 1U)

/// The bit position for the is constant flag in a bal_instruction_t.
#define BAL_IS_CONSTANT_BIT_POSITION (1U << 16U)

/// The engine is currently executing guest code.
#define BAL_ENGINE_FLAG_RUNNING 1

    /// Represents the mapping of a Guest Register to an SSA variable.
    /// This is only used during Single Static Assignment construction
    /// to track variable definitions across basic blocks.
    typedef struct
    {
        /// The index of the most recent SSA definition for this register.
        uint32_t current_ssa_index;

        /// The index of the SSA definition that existed at the start of the
        /// current block.
        uint32_t original_variable_index;
    } bal_source_variable_t;

    /// Holds the Intermediate Representation buffers, SSA state, and other
    /// important metadata. The structure is divided into hot and cold data aligned
    /// to 64 bytes. Both hot and cold data lives on their own cache lines.
    BAL_ALIGNED(64) typedef struct
    {
        /// The guest CPU state.
        bal_cpu_t *cpu;

        /// The allocator used for all internal engine memory.
        const bal_allocator_t *allocator;

        /// The memory interface for all guest-to-host address translation.
        const bal_memory_interface_t *memory_interface;

        /// Opaque pointer to the internal engine's state.
        void *engine_state;

        /// Handles logging for this engine.
        bal_logger_t logger;

        /// The current error state of the engine.
        ///
        /// If an operation fails, this field is set to a specific error code.
        /// See [`bal_opcode_t`]. Once set to an error state, subsequent operation
        /// on this engine will silently fail until [`bal_engine_reset`] is called.
        bal_error_t status;

        /// Execution flags (e.g., `BAL_ENGINE_FLAG_RUNNING`).
        uint32_t flags;
    } bal_engine_t;

    static_assert(sizeof(bal_engine_t) <= 64, "Engine must fit in a L1 Cache line");

    /// Initializes a Ballistic engine.
    ///
    /// # Errors
    ///
    /// Returns [`BAL_SUCCESS`] if the engine is ready for use.
    ///
    /// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if the pointers are `NULL`.
    ///
    /// Returns [`BAL_ERROR_ALLOCATION_FAILED`] if the allocator cannot allocate a memory buffer.
    BAL_COLD bal_error_t bal_engine_init(bal_engine_t                 *engine,
                                         bal_cpu_t                    *cpu,
                                         const bal_allocator_t        *allocator,
                                         const bal_memory_interface_t *memory_interface,
                                         bal_logger_t                  logger);

    /// The sole entry point for executing guest code.
    BAL_HOT bal_error_t bal_engine_run(bal_engine_t *engine);

    /// Translates machine code starting at `guest_address_start` into the engine's
    /// internal IR. `interface` provides memory access handling (like instruction
    /// fetching).
    ///
    /// Returns [`BAL_SUCCESS`] on success.
    ///
    /// # Safety
    ///
    /// `guest_address_start` must be non-NULL.
    ///
    /// # Errors
    ///
    /// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if functino arguments are invalid or
    /// or `engine->status != BAL_SUCCESS`.
    ///
    /// Returns [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if the array `engine->constants` overflows.
    // BAL_HOT bal_error_t
    // bal_engine_translate_tier2(bal_engine_t *BAL_RESTRICT                 engine,
    // const bal_memory_interface_t *BAL_RESTRICT interface,
    // bal_guest_address_t                       *guest_address_start,
    // size_t                                     max_instructions);

    /// Resets `engine` for the next compilation unit. This is a low-cost memory
    /// operation designed to be called between translation units.
    ///
    /// Returns [`BAL_SUCCESS`] on success.
    ///
    /// # Errors
    ///
    /// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if `engine` is `NULl`.
    BAL_HOT bal_error_t bal_engine_reset(bal_engine_t *engine);

    /// Frees all `engine` heap-allocated resources using `allocator`.
    ///
    /// # Warning
    ///
    /// This function does not free the [`bal_engine_t`] struct itself, as the
    /// caller may have allocated it on the stack.
    BAL_COLD void bal_engine_destroy(const bal_allocator_t *allocator, bal_engine_t *engine);

    /// Returns the IR instructions array.
    ///
    /// # Safety
    ///
    /// Returns `NULl` if `engine` or `engine->arena_base` is `NULL`.
    const bal_instruction_t *bal_engine_get_ir_instructions(const bal_engine_t *engine);

    /// Returns the constant generated from the IR layer at `index`.
    ///
    /// # Safety
    ///
    /// Returns `NULL` if `engine` or `engine->arena_base` is `NULL`.
    const bal_constant_t *bal_engine_get_constant(const bal_engine_t *engine, bal_constant_t index);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* BALLISTIC_ENGINE_H */

/*** end of file ***/
