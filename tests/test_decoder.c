#include "bal_decoder.h"

#include "bal_attributes.h"
#include "decoder_table_gen.h"
#include <stdio.h>
#include <stdlib.h>
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
#define DECODER_HASH_TABLE_SIZE 2048
#define BLOCK_SIZE         (1U << DECODER_HASH_SHIFT)
#define MAX_LOCAL_CANDIDATES 1024

typedef struct
{
    uint32_t mask;
    uint32_t expected;
    int priority;
    const bal_decoder_instruction_metadata_t *metadata;
} hot_candidate_t;

int
main (void)
{
    uint64_t total_collisions = 0;
    uint64_t total_errors = 0;

    for (uint32_t hash_index = 0; hash_index < DECODER_HASH_TABLE_SIZE; ++hash_index)
    {
        const decoder_bucket_t *bucket = &g_decoder_lookup_table[hash_index];
        hot_candidate_t local_candidates[MAX_LOCAL_CANDIDATES];
        size_t candidate_count = 0;
        
        if (bucket->instructions != NULL)
        {
            if (BAL_UNLIKELY(bucket->count > MAX_LOCAL_CANDIDATES))
            {
                printf("[FATAL] Bucket %u has %zu items. Increase MAX_LOCAL_CANDIDATES.\n",
                        hash_index, bucket->count);
                return 1;
            }

            for (size_t i = 0; i < bucket->count; ++i)
            {
                const bal_decoder_instruction_metadata_t *metadata = bucket->instructions[i];
                local_candidates[i].mask = metadata->mask;
                local_candidates[i].expected = metadata->expected;
                local_candidates[i].priority = POPCOUNT(metadata->mask);
                local_candidates[i].metadata = metadata;
            }

            candidate_count = bucket->count;
        }

        uint32_t base_instruction = hash_index << DECODER_HASH_SHIFT;

        for (uint32_t offset = 0; offset < BLOCK_SIZE; ++offset)
        {
            uint32_t instruction = base_instruction + offset;

            // Device Under Test.
            // 
            const bal_decoder_instruction_metadata_t *dut_result
                  = bal_decode_arm64(instruction);

            const bal_decoder_instruction_metadata_t *ref_result = NULL;

            for (size_t k = 0; k < candidate_count; ++k)
            {
                if ((instruction & local_candidates[k].mask) == local_candidates[k].expected)
                {
                    ref_result = local_candidates[k].metadata;

                    // Collision Check. Does the next candidate also matches
                    // and has the same priority?
                    // 
                    if (BAL_UNLIKELY(k + 1 < candidate_count))
                    {
                        if (local_candidates[k].priority == local_candidates[k+1].priority)
                        {
                            if ((instruction & local_candidates[k+1].mask) == local_candidates[k+1].expected)
                            {
                                if (strcmp(ref_result->name, local_candidates[k+1].metadata->name) != 0)
                                {
                                    ++total_collisions;
                                }
                            }
                        }
                    }

                    // Found hight priority match.
                    //
                    break; 
                }
            }

            if (BAL_UNLIKELY(dut_result != ref_result))
            {
                printf("[FAIL] Mismatch at 0x%08x\n", instruction);
                printf("DUT: %s\n", dut_result ? dut_result->name : "NULL");
                printf("REF: %s\n", ref_result ? ref_result->name : "NULL");
                ++total_errors;

                if (total_errors > 10)
                {
                    return 1;
                }
            }
        }

        if (0 == (hash_index & 0x7F))
        {
            printf("Progress: %3.0F%% (Bucket %d/%d)\n",
                    (double)hash_index / DECODER_HASH_TABLE_SIZE * 100.0,
                    hash_index, DECODER_HASH_TABLE_SIZE);
        }
    }

    printf("--- Results ---\n");
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
