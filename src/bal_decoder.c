#include "bal_decoder.h"
#include "bal_decoder_table_gen.h"
#include <stddef.h>
#include <stdio.h>

const bal_decoder_instruction_metadata_t *
bal_decode_arm64(const uint32_t instruction)
{
    // Index is top 11 bits
    const uint16_t                                   index  = (uint16_t)(instruction >> 21);
    const decoder_bucket_t                          *bucket = &g_decoder_lookup_table[index];
    const bal_decoder_instruction_metadata_t *const *candidate
        = &g_decoder_hash_candidates[bucket->index];
    const uint8_t count = bucket->count;

    for (size_t i = 0; i < count; ++i)
    {
        const bal_decoder_instruction_metadata_t *metadata = *candidate++;

        if ((instruction & metadata->mask) == metadata->expected)
        {
            return metadata;
        }
    }

    return NULL;
}
