#include "bal_decoder.h"
#include "decoder_table_gen.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#if defined(_MSC_VER)
#include <intrin.h>
#define POPCOUNT(x) __popcnt(x)
#else
#define POPCOUNT(x) __builtin_popcount(x)
#endif

#define DECODER_HASH_SHIFT 21
#define DECODER_HASH_MASK  0x7FF

int
main (void)
{
    printf("Starting Decoder Test (0x00000000 - 0xFFFFFFFF)...\n");
    uint64_t total_decoded = 0;
    uint64_t total_collisions = 0;
    uint64_t total_errors = 0;

    for (uint64_t i = 0; i <= UINT32_MAX; ++i)
    {
        uint32_t instruction = (uint32_t)i;
        const bal_decoder_instruction_metadata_t *metadata
            = bal_decoder_arm64_decode(instruction);

        uint32_t hash_index = (instruction >> DECODER_HASH_SHIFT) & DECODER_HASH_MASK;
        const decoder_bucket_t *bucket = &g_decoder_lookup_table[hash_index];
        const bal_decoder_instruction_metadata_t *best_match = NULL;
        int max_priority = -1;

        if (bucket->instructions != NULL)
        {
            for (size_t j = 0; j < bucket->count; ++j)
            {
                const bal_decoder_instruction_metadata_t *candidate = bucket->instructions[j];

                if (candidate->expected == (instruction & candidate->mask))
                {
                    int priority = POPCOUNT(candidate->mask);

                    if (priority > max_priority)
                    {
                        max_priority = priority;
                        best_match = candidate;
                    }
                    else
                    {
                        // Two instructions with equal priority claim these bits.
                        // There is a bug in the XML parser.
                        //
                        if (best_match && strcmp(best_match->name, candidate->name) != 0)
                        {
                            ++total_collisions;
                        }
                    }
                }
            }
        }

        if (metadata != best_match)
        {
            printf("[FAIL] Priority Error at 0x%08x\n", instruction);
            printf("       Decoder returned: %s\n", metadata ? metadata->name : "NULL");
            printf("       Reference found: %s\n", best_match ? best_match->name : "NULL");

            if (metadata != NULL)
            {
                printf("       Decoder Mask: 0x%08x (Priority: %d)\n", metadata->mask, POPCOUNT(metadata->mask));
            }

            if (best_match != NULL)
            {
                printf("       Ref Mask: 0x%08x (Priority: %d)\n", best_match->mask, POPCOUNT(best_match->mask));
            }

            ++total_errors;

            if (total_errors > 10)
            {
                printf("[FATAL] Too many errors. Aborting...");
                return 1;
            }
        }

        if (metadata != NULL)
        {
            // The decoder should never return a match that failed the mask.
            if ((instruction & metadata->mask) != metadata->expected)
            {
                printf("[FAIL] Consistency: Inst 0x%08x matched '%s' but failed mask check.\n", instruction, metadata->name);
                ++total_errors;
            }
            ++total_decoded;
        }
        
        if (0 == (instruction & 0x0FFFFFFF))
        {
            printf("Progress: %3.0f%% (0x%08x)\n", (double)i / 42949672.96, instruction);
        }
    }

    printf("--- Results ---\n");
    printf("Total Decoded:  %" PRIu64 "\n", total_decoded);
    printf("Total Collisions:  %" PRIu64 "\n", total_collisions);
    printf("Total Errors:  %" PRIu64 "\n", total_errors);

    if (total_errors > 0)
    {
        printf("[FAILURE] Ballistic Decoder is flawed.\n");
        return 1;
    }

    printf("[SUCCESS] Ballistic Decoder is mathematically correct.\n");
    return 0;
}
