#include "bal_translator.h" 

typedef struct
{
    bal_instruction_t     *instructions;
    bal_bit_width_t       *ssa_bit_widths;
    bal_source_variable_t *source_variables;
    size_t                 instruction_count;
} bal_translation_context_t;

/*** end of file ***/
