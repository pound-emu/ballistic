/** @file bal_engine.h
 *
 * @brief Manages resources while translating ARM blocks to Itermediate
 * Representation.
 */

#ifndef BALLISTIC_ENGINE_H
#define BALLISTIC_ENGINE_H

#include "bal_attributes.h"
#include "bal_types.h"
#include "bal_memory.h"
#include "bal_errors.h"
#include <stdint.h>

/*!
 * @brief Pattern written to memory during initialization.
 * @details Used to detect reads from uninitialized allocated memory.
 */
#define POISON_UNINITIALIZED_MEMORY 0x5a

/*!
 * @brief Represents the mapping of a Guest Register to an SSA variable.
 * @details This is state used only used during the SSA construction pass.
 */
typedef struct
{
    /*!
     * @brief The most recent SSA definition for this register.
     */
    uint32_t current_ssa_index;

    /*!
     * @brief The SSA definition that existed at the start of the block.
     */
    uint32_t original_variable_index;
} bal_source_variable_t;

BAL_ALIGNED(64) typedef struct
{
    /* Hot Data */

    /*!
     * @brief Map of ARM registers to curret SSA definitions.
     */
    bal_source_variable_t *source_variables;

    /*!
     * @brief The linear buffer of generated IR instructions.
     */
    bal_instruction_t *instructions;

    /*!
     * @brief Metadata tracking the bit-width (32/64) of each SSA definition.
     */
    bal_bit_width_t *ssa_bit_widths;

    /*!
     * @brief Size of source variable array.
     */
    size_t source_variables_size;

    /*!
     * @brief Size of instruction array.
     */
    size_t instructions_size;

    /*!
     * @brief Size of ssa bit width array.
     */
    size_t ssa_bit_widths_size;

    /*!
     * @brief The current number of instructions emitted.
     */
    bal_instruction_count_t instruction_count;

    /*!
     * @brief The Engine's error state.
     * @details If an operation fails, this is set to a specific error code.
     * Subsequent operations will silently fail until the engine is reset.
     */
    bal_error_t status;
    char        _pad[4];

    /* Cold Data */

    // The pointer returned during heap initialization.
    // We need this to free the engine's allocated arrays.
    //
    void  *arena_base;
    size_t arena_size;

} bal_engine_t;

/*!
 * @brief Initialize a Ballistic engine.
 *
 * @details
 * This is a high-cost memory allocation operation that should be done
 * sparingly.
 *
 * @param[in]  allocator Pointer to the memory allocator interface.
 * @param[out] engine    Pointer to the engine struct to initialize.
 *
 * @return BAL_SUCCESS on success.
 * @return BAL_ERROR_INVALID_ARG if arguments are NULL.
 * @return BAL_ERROR_ALLOCATION_FAILED if the allocator returns NULL.
 */
BAL_COLD bal_error_t bal_engine_init(bal_allocator_t *allocator,
                                     bal_engine_t    *engine);
/*!
 * @brief Executes the main JIT compilation loop.
 *
 * @param[in,out] engine           The engine context. Must be initialized.
 * @param[in]     arm_entry_point  Pointer to the start of the ARM machine code
 *                                 to translate.
 *
 * @return BAL_SUCCESS on successfull translation of arm_entry_point.
 * @return BAL_ERROR_ENGINE_STATE_INVALID if any function parameters are NULL.
 */
BAL_HOT bal_error_t
bal_engine_run(bal_engine_t *BAL_RESTRICT   engine,
               const uint32_t *BAL_RESTRICT arm_entry_point);

/*!
 * @brief Resets the engine for the next compilation unit.
 *
 * @details
 * This is a low cost memory operation.
 *
 * @param[in,out] engine The engine to reset.
 *
 * @return BAL_SUCCESS on success.
 * @return BAL_ERROR_INVALID_ARG if arguments are NULL.
 */
BAL_HOT bal_error_t bal_engine_reset(bal_engine_t *engine);

/*!
 * Frees all engine heap allocated resources.
 *
 * @param[in,out] engine The engine to destroy.
 *
 * @warning The engine struct itself is NOT freed (it may be stack allocated).
 */
BAL_COLD void bal_engine_destroy(bal_allocator_t *allocator,
                                 bal_engine_t    *engine);

#endif /* BALLISTIC_ENGINE_H */

/*** end of file ***/
