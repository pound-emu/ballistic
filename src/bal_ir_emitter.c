#include "bal_ir_emitter.h"
#include <stdbool.h>

#define BAL_OPCODE_SHIFT_POSITION  53U
#define BAL_SOURCE1_SHIFT_POSITION 35U
#define BAL_SOURCE2_SHIFT_POSITION 17U

#define BAL_IS_CONSTANT_BIT_POSITION (1U << 16U)

bal_error_t
emit_instruction (bal_engine_t   *engine,
                  uint32_t        opcode,
                  uint32_t        source1,
                  uint32_t        source2,
                  uint32_t        source3,
                  bal_bit_width_t bit_width)
{
    // This is a hot function, no argument error handling. Segfault if the user
    // passes NULL.

    if (engine->status != BAL_SUCCESS)
    {
        return BAL_ENGINE_STATE_INVALID;
    }

    bool is_greater_than_instructions_array
        = (engine->instruction_count >= engine->instructions_size);

    bool is_greater_than_source_size
        = (engine->instruction->count >= (BAL_SOURCE_SIZE - 1));

    if ((true == is_greater_than_instructions_array)
        || (true == is_greater_than_source_size))
    {
        engine->status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return engine->status;
    }

    bal_instruction_t opcode_bits = (BAL_OPCODE_SIZE - 1U) & opcode;
    bal_instruction_t source1_bits
        = ((BAL_SOURCE_SIZE - 1U) | BAL_IS_CONSTANT_BIT_POSITION) & source1;

    bal_instruction_t source2_bits
        = ((BAL_SOURCE_SIZE - 1U) | BAL_IS_CONSTANT_BIT_POSITION) & source2;

    bal_instruction_t source3_bits
        = ((BAL_SOURCE_SIZE - 1U) | BAL_IS_CONSTANT_BIT_POSITION) & source3;

    bal_instruction_t instruction
        = (opcode_bits << BAL_OPCODE_SHIFT_POSITION)
          | (source1_bits << BAL_SOURCE1_SHIFT_POSITION)
          | (source2_bits << BAL_SOURCE2_SHIFT_POSITION) | source3_bits;

    engine->instructions[engine->instruction_count]   = instruction;
    engine->ssa_bit_widths[engine->instruction_count] = bit_width;
    engine->status                                    = BAL_SUCCESS;
    ++engine->instruction_count;

    return engine->status;
}

/*** end of file ***/
