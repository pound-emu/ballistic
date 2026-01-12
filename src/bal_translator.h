/** @file bal_translator.h
 *
 * @brief ARM to Ballistic IR Translation Interface.
 *
 * @details
 *
 * This module represents the frontend of Ballistic. It performas the following
 * in a single pass:
 *
 * 1. Fetch
 * 2. Decode
 * 3. SSA Construction
 */

#ifndef BAL_TRANSLATOR_H
#define BAL_TRANSLATOR_H

#include "bal_engine.h"
#include "bal_types.h"

typedef struct
{
    bal_instruction_t     *instructions;
    bal_bit_width_t       *ssa_bit_widths;
    bal_source_variable_t *source_variables;
    size_t                 instruction_count;
} bal_translation_context_t;

#endif /* BAL_TRANSLATOR_H */

/*** end of file ***/
