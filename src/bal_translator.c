#include "bal_translator.h" 

typedef struct
{
    bal_instruction_t     *BAL_RESTRICT instructions;
    bal_bit_width_t       *BAL_RESTRICT ssa_bit_widths;
    bal_source_variable_t *BAL_RESTRICT source_variables;
    size_t                 instruction_count;
} bal_translation_context_t;

/*** end of file ***/
