#include "bal_engine.h"
#include "bal_decoder.h"
#include <stddef.h>
#include <string.h>

#define MAX_INSTRUCTIONS 65536

// Not sure what exact value to put here.
//
#define MAX_GUEST_REGISTERS 128

#define BAL_OPCODE_SHIFT_POSITION  51U
#define BAL_SOURCE1_SHIFT_POSITION 34U
#define BAL_SOURCE2_SHIFT_POSITION 17U

/// The maximum value for an Opcode.
#define BAL_OPCODE_SIZE (1U << 11U)

/// The maximum value for an Operand Index.
/// Bit 17 is reserved for the "Is Constant" flag.
#define BAL_SOURCE_SIZE (1U << 16U)

/// The bit position for the is constant flag in a bal_instruction_t.
#define BAL_IS_CONSTANT_BIT_POSITION (1U << 16U)

/// Helper macro to align `x` UP to the nearest memory alignment.
#define BAL_ALIGN_UP(x, memory_alignment) \
    (((x) + ((memory_alignment) - 1)) & ~((memory_alignment) - 1))

static uint32_t extract_operand_value(const uint32_t,
                                      const bal_decoder_operand_t *);
static uint32_t intern_constant(bal_constant_t,
                                bal_constant_t *,
                                bal_constant_count_t *,
                                size_t,
                                bal_error_t *);

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
    engine->source_variables_size
        = source_variables_size / sizeof(bal_source_variable_t);
    engine->instructions_size = instructions_size / sizeof(bal_instruction_t);
    engine->constants_size    = constants_size / sizeof(bal_constant_t);
    engine->instruction_count = 0;
    engine->status            = BAL_SUCCESS;
    engine->arena_base        = (void *)data;
    engine->arena_size        = total_size_with_padding;

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

    bal_instruction_t *BAL_RESTRICT ir_instruction_cursor
        = engine->instructions + engine->instruction_count;

    bal_instruction_t *BAL_RESTRICT ir_instruction_end
        = engine->instructions + engine->instructions_size;

    bal_bit_width_t *BAL_RESTRICT bit_width_cursor
        = engine->ssa_bit_widths + engine->instruction_count;

    bal_source_variable_t *BAL_RESTRICT source_variables_base
        = engine->source_variables;

    bal_constant_t               constant_count    = engine->constant_count;
    bal_instruction_count_t      instruction_count = engine->instruction_count;
    const uint32_t *BAL_RESTRICT arm_instruction_cursor = arm_entry_point;
    uint32_t                     arm_register_values[BAL_OPERANDS_SIZE] = { 0 };

    while (ir_instruction_cursor < ir_instruction_end)
    {
        const bal_decoder_instruction_metadata_t *metadata
            = bal_decode_arm64(*arm_instruction_cursor);
        const bal_decoder_operand_t *BAL_RESTRICT operands = metadata->operands;

        for (size_t i = 0; i < BAL_OPERANDS_SIZE; ++i)
        {
            arm_register_values[i]
                = extract_operand_value(*arm_instruction_cursor, &operands[i]);
        }

        switch (metadata->ir_opcode)
        {
            case OPCODE_ADD: {
                uint32_t rd = arm_register_values[0];
                uint32_t rn = arm_register_values[1];
                uint32_t rm = arm_register_values[2];
                // TODO
            }
            default:
                break;
        }

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

BAL_HOT static uint32_t
extract_operand_value (const uint32_t               instruction,
                       const bal_decoder_operand_t *operand)
{
    if (BAL_OPERAND_TYPE_NONE == operand->type)
    {
        return 0;
    }

    uint32_t mask = (1U << operand->bit_width) - 1;
    uint32_t bits = (instruction >> operand->bit_position) & mask;
    return bits;
}

BAL_HOT static uint32_t
intern_constant (bal_constant_t                     constant,
                 bal_constant_t *BAL_RESTRICT       constants,
                 bal_constant_count_t *BAL_RESTRICT count,
                 size_t                             constants_max_size,
                 bal_error_t *BAL_RESTRICT          status)
{
    if (BAL_UNLIKELY(*status != BAL_SUCCESS))
    {
        return 0 | BAL_IS_CONSTANT_BIT_POSITION;
    }

    if (BAL_UNLIKELY(*count >= constants_max_size))
    {
        *status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return 0 | BAL_IS_CONSTANT_BIT_POSITION;
    }

    constants[*count] = constant;
    uint32_t index    = *count | BAL_IS_CONSTANT_BIT_POSITION;
    (*count)++;
    return index;
}
