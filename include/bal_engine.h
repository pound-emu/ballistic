#ifndef BALLISTIC_ENGINE_H
#define BALLISTIC_ENGINE_H

#include "bal_attributes.h"
#include "bal_errors.h"
#include "bal_logging.h"
#include "bal_memory.h"
#include "bal_types.h"
#include <stdint.h>

/// A byte pattern written to memory during initialization, poisoning allocated
/// regions. This is mainly used for detecting reads from uninitialized memory.
#define POISON_UNINITIALIZED_MEMORY 0xFF

/// IR Instruction Bitfield Layout:
///
/// 63               51 50        34 33        17 16        00
/// |-----------------| |----------| |----------| |----------|
///        opc             src1         src2         src3

/// Opcode bitfield least significant bit.
#define BAL_OPCODE_SHIFT_POSITION  51U

/// Source1 bitfield least significant bit.
#define BAL_SOURCE1_SHIFT_POSITION 34U

/// Source2 bitfield least significant bit.
#define BAL_SOURCE2_SHIFT_POSITION 17U

/// The maximum value for an Opcode.
#define BAL_OPCODE_SIZE (1U << 11U)

/// The maximum value for an Operand Index.
/// Bit 17 is reserved for the "Is Constant" flag.
#define BAL_SOURCE_SIZE (1U << 16U)

/// The bit position for the is constant flag in a bal_instruction_t.
#define BAL_IS_CONSTANT_BIT_POSITION (1U << 16U)

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
typedef struct 
{
    /* Hot Data */

    /// Map of ARM registers to their current SSA definitions.
    bal_source_variable_t *source_variables;

    /// The linear buffer of generated IR instructions for the current
    /// compilation unit.
    bal_instruction_t *instructions;

    /// Metadata tracking the bit-width (32 or 64 bit) for each variable.
    bal_bit_width_t *ssa_bit_widths;

    /// Linear buffer of constants generated in the current compilation unit.
    bal_constant_t *constants;

    /// The size of the `source_variables` array.
    size_t source_variables_size;

    /// The size of the `instructions` array.
    size_t instructions_size;

    /// The size of the `constants` array.
    size_t constants_size;

    /// The current number of instructions emitted.
    ///
    /// This tracks the current position in `instructions` and `ssa_bit_widths`
    /// arrays.
    bal_instruction_count_t instruction_count;

    /// The number of constants emiited.
    ///
    /// This tracks the current position in the `constants` array.
    bal_constant_count_t constant_count;

    /// The current error state of the Engine.
    ///
    /// If an operation fails, this field is set to a specific error code.
    /// See [`bal_opcode_t`]. Once set to an error state, subsequent operation
    /// on this engine will silently fail until [`bal_engine_reset`] is called.
    bal_error_t status;

    /* Cold Data */

    /// The base pointer returned during the underlying heap allocation. This
    /// is required to correctly free the engine's internal arrays.
    void *arena_base;

    /// The total size of the allocated arena.
    size_t arena_size;

    /// Handles logging for this engine.
    bal_logger_t logger;

} bal_engine_t;

/// Initializes a Ballistic engine.
///
/// Populates `engine` with empty buffers allocated with `allocator`. This is
/// a high cost memory operation that reserves a lot of memory and should
/// be called sparingly.
///
/// Returns [`BAL_SUCCESS`] if the engine iz ready for use.
///
/// # Errors
///
/// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if the pointers are `NULL`.
///
/// Returns [`BAL_ERROR_ALLOCATION_FAILED`] if the allocator cannot fulfill the
/// request.
BAL_COLD bal_error_t bal_engine_init(bal_allocator_t *allocator, bal_engine_t *engine);

/// Translates machine code starting at `arm_instruction_cursor` into the engine's
/// internal IR. `interface` provides memory access handling (like instruction
/// fetching).
///
/// Returns [`BAL_SUCCESS`] on success.
///
/// # Errors
///
/// Returns [`BAL_ERROR_ENGINE_STATE_INVALID`] if `engine` is not initialized
/// or `engine->status != BAL_SUCCESS`.
///
/// Returns [`BAL_ERROR_INSTRUCTION_OVERFLOW`] if the array `engine->constants` overflows.
BAL_HOT bal_error_t bal_engine_translate(bal_engine_t *BAL_RESTRICT           engine,
                                         bal_memory_interface_t *BAL_RESTRICT interface,
                                         const uint32_t *BAL_RESTRICT arm_instruction_cursor,
                                         size_t                       arm_size);

/// Resets `engine` for the next compilation unit. This is a low cost memory
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
BAL_COLD void bal_engine_destroy(bal_allocator_t *allocator, bal_engine_t *engine);
#endif /* BALLISTIC_ENGINE_H */

/*** end of file ***/
