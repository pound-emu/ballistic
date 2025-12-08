#include "decoder.h"
#include "decoder_table_gen.h"
#include <stddef.h>

const bal_decoder_instruction_metadata_t *
bal_decoder_arm64_decode (const uint32_t instruction)
{
    for (size_t i = 0; i < BAL_DECODER_ARM64_INSTRUCTIONS_SIZE; ++i)
    {
        const bal_decoder_instruction_metadata_t *metadata
            = &g_bal_decoder_arm64_instructions[i];

        if ((instruction & metadata->mask) == metadata->expected)
        {
            return metadata;
        }
    }
    return NULL;
}
