#include "bal_translator.h"

typedef struct
{
    bal_instruction_t *BAL_RESTRICT     instructions;
    bal_bit_width_t *BAL_RESTRICT       ssa_bit_widths;
    bal_source_variable_t *BAL_RESTRICT source_variables;
    size_t                              instruction_count;
    uint32_t                            max_instructions;
    bal_error_t                         status;
} bal_translation_context_t;

bal_error_t
bal_translate_block (bal_engine_t *BAL_RESTRICT   engine,
                     const uint32_t *BAL_RESTRICT arm_code)
{
    // Copy pointers to the stack to deal with Double Indirection and Aliasing
    // Fear.
    //
    bal_translation_context_t context;
    context.instructions     = engine->instructions;
    context.ssa_bit_widths   = engine->ssa_bit_widths;
    context.source_variables = engine->source_variables;

    // Copy other important data to `context` to keep register pressure low.
    //
    context.instruction_count = engine->instruction_count;
    context.max_instructions
        = engine->instructions_size / sizeof(bal_instruction_t);
    context.status = BAL_SUCCESS;

    for (size_t i = 0; i < context.max_instructions; ++i)
    {
        // TODO
    }

    // Sync back to memory. We should only write to engine once in this 
    // function.
    //
    engine->instruction_count = context.instruction_count;
    engine->status            = context.status;

    return context.status;
}

/*** end of file ***/
