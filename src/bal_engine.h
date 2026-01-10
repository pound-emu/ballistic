/** @file bal_engine.h
 *
 * @brief Manages resources while translating ARM blocks to Itermediate
 * Representation.
 */

#ifndef BALLISTIC_ENGINE_H
#define BALLISTIC_ENGINE_H

#include "bal_types.h"
#include "bal_memory.h"
#include "bal_errors.h"
#include <stdint.h>

#define POISON_UNINITIALIZED_MEMORY 0x5a
#define POISON_FREED_MEMORY         0x6b

typedef struct
{
    uint32_t current_ssa_index;
    uint32_t original_variable_index;
} bal_source_variable_t;

__attribute__((aligned(64))) typedef struct
{
    // Hot Data
    //
    bal_source_variable_t *source_variables;
    bal_instruction_t     *instructions;
    bal_bit_width_t       *ssa_bit_widths;
    uint16_t               instruction_count;
    bal_error_t            status;
    char                   _pad[4];

    // Cold Data

    // The pointer returned during heap initialization.
    // We need this to free the engine's allocated arrays.
    //
    void  *arena_base;
    size_t arena_size;

} bal_engine_t;

bal_error_t bal_engine_init (bal_allocator_t *allocator, bal_engine_t *engine);

#endif /* BALLISTIC_ENGINE_H */

/*** end of file ***/
