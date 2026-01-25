#include "bal_decoder.h"
#include "decoder_table_gen.h"
#include <stddef.h>
#include <stdio.h>

const bal_decoder_instruction_metadata_t *
bal_decode_arm64(const uint32_t instruction)
{
    // Index is top 11 bits
    const uint16_t index = instruction >> 21;

    const decoder_bucket_t *bucket = &g_decoder_lookup_table[index];
    for (size_t i = 0; i < bucket->count; ++i)
    {
        const bal_decoder_instruction_metadata_t *metadata = bucket->instructions[i];

        if ((instruction & metadata->mask) == metadata->expected)
        {
            return metadata;
        }
    }

    return NULL;
}
