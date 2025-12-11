#include "decoder.h"
#include "decoder_table_gen.h"
#include <stddef.h>
#include <stdio.h>

const bal_decoder_instruction_metadata_t *
bal_decoder_arm64_decode (const uint32_t instruction)
{
    /* Extract hash key: Bits [27:20] and [7:4] */
    const uint32_t major = (instruction >> 20U) & 0xFFU;
    const uint32_t minor = (instruction >> 4U) & 0xFU;
    const uint16_t index = (uint16_t)((major << 4U) | minor);

    const decoder_bucket_t *bucket = &g_decoder_lookup_table[index];
    for (size_t i = 0; i < bucket->count; ++i)
    {
        const bal_decoder_instruction_metadata_t* metadata = bucket->instructions[i];

        if ((instruction & metadata->mask) == metadata->expected)
        {
            return metadata;
        }
    }

    return NULL;
}
