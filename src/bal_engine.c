#include "bal_engine.h"
#include <stddef.h>
#include <string.h>

#define MAX_INSTRUCTIONS 65536

// Not sure what exact value to put here.
//
#define MAX_GUEST_REGISTERS 128

// Helper macro to align `x` UP to the nearest memory alignment.
//
#define BAL_ALIGN_UP(x, memory_alignment) \
    (((x) + ((memory_alignment) - 1)) & ~((memory_alignment) - 1))

bal_error_t
bal_engine_init (bal_allocator_t *allocator, bal_engine_t *engine)
{
    if (NULL == allocator || NULL == engine)
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    size_t source_variables_size
        = MAX_GUEST_REGISTERS * sizeof(bal_source_variable_t);

    size_t ssa_bit_widths_size = MAX_INSTRUCTIONS * sizeof(bal_bit_width_t);
    size_t instructions_size   = MAX_INSTRUCTIONS * sizeof(bal_instruction_t);
    size_t constants_size      = MAX_INSTRUCTIONS * sizeof(bal_instruction_t);

    // Calculate amount of memory needed for all arrays in engine.
    //
    size_t memory_alignment = 64U;

    size_t offset_instructions
        = BAL_ALIGN_UP(source_variables_size, memory_alignment);

    size_t offset_ssa_bit_widths = BAL_ALIGN_UP(
        (offset_instructions + instructions_size), memory_alignment);

    size_t offset_constants = BAL_ALIGN_UP(
        (offset_ssa_bit_widths + ssa_bit_widths_size), memory_alignment);

    size_t total_size_with_padding
        = BAL_ALIGN_UP((offset_constants + constants_size), memory_alignment);

    uint8_t *data = (uint8_t *)allocator->allocate(
        allocator, memory_alignment, total_size_with_padding);

    if (NULL == data)
    {
        engine->status = BAL_ERROR_ALLOCATION_FAILED;
        return engine->status;
    }

    engine->source_variables = (bal_source_variable_t *)data;
    engine->instructions   = (bal_instruction_t *)(data + offset_instructions);
    engine->ssa_bit_widths = (bal_bit_width_t *)(data + offset_ssa_bit_widths);
    engine->constants      = (bal_constant_t *)(data + offset_constants);
    engine->source_variables_size = source_variables_size;
    engine->instructions_size     = instructions_size;
    engine->constants_size        = constants_size;
    engine->instruction_count     = 0;
    engine->status                = BAL_SUCCESS;
    engine->arena_base            = (void *)data;
    engine->arena_size            = total_size_with_padding;

    (void)memset(engine->source_variables,
                 POISON_UNINITIALIZED_MEMORY,
                 source_variables_size);

    (void)memset(
        engine->instructions, POISON_UNINITIALIZED_MEMORY, instructions_size);

    (void)memset(engine->ssa_bit_widths,
                 POISON_UNINITIALIZED_MEMORY,
                 ssa_bit_widths_size);

    (void)memset(
        engine->constants, POISON_UNINITIALIZED_MEMORY, constants_size);

    return engine->status;
}

bal_error_t
bal_engine_translate (bal_engine_t *BAL_RESTRICT           engine,
                      bal_memory_interface_t *BAL_RESTRICT interface,
                      const uint32_t *BAL_RESTRICT         arm_entry_point)
{
    if (BAL_UNLIKELY(NULL == engine || NULL == arm_entry_point))
    {
        return BAL_ERROR_ENGINE_STATE_INVALID;
    }

    // Load state to registers.
    //
    bal_instruction_t *BAL_RESTRICT ir_instruction_cursor
        = engine->instructions + engine->instruction_count;

    bal_instruction_t *BAL_RESTRICT ir_instruction_end
        = engine->instructions + engine->instructions_size;

    bal_bit_width_t *BAL_RESTRICT bit_width_cursor
        = engine->ssa_bit_widths + engine->instruction_count;

    bal_source_variable_t *BAL_RESTRICT source_variables_base
        = engine->source_variables;

    const uint32_t *BAL_RESTRICT arm_instruction_cursor = arm_entry_point;

    while (ir_instruction_cursor < ir_instruction_end)
    {
        ir_instruction_cursor++;
        bit_width_cursor++;
        arm_instruction_cursor++;
    }

    return engine->status;
}

bal_error_t
bal_engine_reset (bal_engine_t *engine)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    engine->instruction_count = 0;
    engine->status            = BAL_SUCCESS;

    (void)memset(engine->source_variables,
                 POISON_UNINITIALIZED_MEMORY,
                 engine->source_variables_size);

    (void)memset(
        engine->constants, POISON_UNINITIALIZED_MEMORY, engine->constants_size);

    return engine->status;
}

void
bal_engine_destroy (bal_allocator_t *allocator, bal_engine_t *engine)
{
    // No argument error handling. Segfault if user passes NULL.

    allocator->free(allocator, engine->arena_base, engine->arena_size);
    engine->arena_base       = NULL;
    engine->source_variables = NULL;
    engine->instructions     = NULL;
    engine->ssa_bit_widths   = NULL;
}
