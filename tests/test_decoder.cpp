#include "bal_attributes.h"
#include "bal_decoder.h"
#include "bal_decoder_table_gen.h"
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#define POPCOUNT(x) __popcnt(x)
#else
#define POPCOUNT(x) __builtin_popcount(x)
#endif

#define DECODER_HASH_SHIFT      21
#define DECODER_HASH_MASK       0x7FF
#define DECODER_HASH_TABLE_SIZE 2048
#define BLOCK_SIZE              (1U << DECODER_HASH_SHIFT)
#define MAX_LOCAL_CANDIDATES    255

typedef struct
{
    uint32_t                                  mask;
    uint32_t                                  expected;
    int                                       priority;
    const bal_decoder_instruction_metadata_t *metadata;
} hot_candidate_t;

TEST(Arm64Decoder, DecodeSuccess)
{
    constexpr uint32_t                        ret_instruction = 0xD65F0000;
    const bal_decoder_instruction_metadata_t *metadata        = bal_decode_arm64(ret_instruction);
    ASSERT_NE(metadata, nullptr);
}

TEST(Arm64Decoder, DecodeFailsImmediately)
{
    uint32_t empty_bucket_index = 0;

    for (size_t i = 0; i < 2048; ++i)
    {
        if (0 == g_decoder_lookup_table[i].count)
        {
            empty_bucket_index = i;
            break;
        }
    }

    const uint32_t                            dummy_instruction = empty_bucket_index << 21;
    const bal_decoder_instruction_metadata_t *metadata = bal_decode_arm64(dummy_instruction);
    ASSERT_EQ(metadata, nullptr);
}

TEST(Arm64Decoder, HashTableIntegrity)
{
    std::atomic<uint64_t> total_collisions = 0;
    std::atomic<uint64_t> total_errors     = 0;
    unsigned int          thread_count     = std::thread::hardware_concurrency();

    if (0 == thread_count)
    {
        thread_count = 4;
    }

    std::vector<std::thread> threads;
    const uint32_t           chunk_size = DECODER_HASH_TABLE_SIZE / thread_count;

    for (unsigned int i = 0; i < thread_count; ++i)
    {
        uint32_t start_index = chunk_size * i;

        // The last thread picks up any remaining buckets it doesn't divide perfectly.
        //
        uint32_t end_index
            = i == thread_count - 1 ? DECODER_HASH_TABLE_SIZE : start_index + chunk_size;

        threads.emplace_back([start_index, end_index, &total_collisions, &total_errors] {
            for (uint32_t hash_index = start_index; hash_index < end_index; ++hash_index)
            {
                const decoder_bucket_t *bucket = &g_decoder_lookup_table[hash_index];
                hot_candidate_t         local_candidates[MAX_LOCAL_CANDIDATES];
                size_t                  candidate_count  = 0;
                const uint32_t          base_instruction = hash_index << DECODER_HASH_SHIFT;

                if (bucket->count != 0)
                {
                    GTEST_ASSERT_LT(bucket->count, MAX_LOCAL_CANDIDATES)
                        << "[FATAL] Bucket " << hash_index << " has " << +bucket->count
                        << " items. Increase MAX_LOCAL_CANDIDATES.\n";

                    uint16_t bucket_index = bucket->index;
                    for (size_t ii = 0; ii < bucket->count; ++ii)
                    {
                        const bal_decoder_instruction_metadata_t *metadata
                            = g_decoder_hash_candidates[bucket_index++];
                        local_candidates[ii].mask     = metadata->mask;
                        local_candidates[ii].expected = metadata->expected;
                        local_candidates[ii].priority = POPCOUNT(metadata->mask);
                        local_candidates[ii].metadata = metadata;
                    }

                    candidate_count = bucket->count;
                }
                else
                {
                    if (bal_decode_arm64(base_instruction) != nullptr)
                    {
                        ++total_errors;
                    }
                    continue;
                }

                for (uint32_t offset = 0; offset < BLOCK_SIZE; ++offset)
                {
                    const uint32_t instruction = base_instruction + offset;

                    // Device Under Test.
                    //
                    const bal_decoder_instruction_metadata_t *dut_result
                        = bal_decode_arm64(instruction);

                    const bal_decoder_instruction_metadata_t *ref_result = nullptr;

                    for (size_t k = 0; k < candidate_count; ++k)
                    {
                        if ((instruction & local_candidates[k].mask)
                            == local_candidates[k].expected)
                        {
                            ref_result = local_candidates[k].metadata;

                            // Collision Check. Does the next candidate also match
                            // and is of the same priority?
                            //
                            if (BAL_UNLIKELY(k + 1 < candidate_count))
                            {
                                if (local_candidates[k].priority
                                    == local_candidates[k + 1].priority)
                                {
                                    if ((instruction & local_candidates[k + 1].mask)
                                        == local_candidates[k + 1].expected)
                                    {
                                        if (strcmp(ref_result->name,
                                                   local_candidates[k + 1].metadata->name)
                                            != 0)
                                        {
                                            ++total_collisions;
                                        }
                                    }
                                }
                            }

                            // Found high priority match.
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
                            GTEST_FAIL();
                        }
                    }
                }

                if (0 == (hash_index & 0x7F))
                {
                    printf("Progress: %3.0F%% (Bucket %d/%d)\n",
                           static_cast<double>(hash_index) / DECODER_HASH_TABLE_SIZE * 100.0,
                           hash_index,
                           DECODER_HASH_TABLE_SIZE);
                }
            }
        });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    printf("--- Results ---\n");
    printf("Total Collisions:  %" PRIu64 "\n", total_collisions.load());
    printf("Total Errors:  %" PRIu64 "\n", total_errors.load());

    if (total_errors > 0)
    {
        printf("[FAILURE] Ballistic Decoder is flawed.\n");
        GTEST_FAIL();
    }

    printf("[SUCCESS] Ballistic Decoder is mathematically correct.\n");
}
