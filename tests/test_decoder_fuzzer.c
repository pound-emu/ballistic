#include "decoder.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static uint32_t rng_state = 0x87654321;

/*
 * Seeding function to set the initial state.
 */
static void
fast_srand (uint32_t seed)
{
    if (seed == 0)
    {
        seed = 0x87654321;
    }
    rng_state = seed;
}
/*
 * Xorshift32 Algorithm
 */
static inline uint32_t
fast_rand (void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

int
main (void)
{
    printf("Starting Decoder Fuzzer Test...\n");
    fast_srand((uint32_t)time(0));

    int failed = 0;
    for (size_t i = 0; i < 100000; ++i)
    {
        uint32_t random_instruction = fast_rand();
        const bal_decoder_instruction_metadata_t *meta
            = bal_decoder_arm64_decode(random_instruction);

        if (NULL != meta)
        {
            if ((random_instruction & meta->mask) != meta->expected)
            {
                printf("[FAIL] %s, 0x%08x & 0x%08x != 0x%08x",
                       meta->name,
                       random_instruction,
                       meta->mask,
                       meta->expected);
                ++failed;
                continue;
            }
        }
    }

    if (failed > 0)
    {
        printf("FAILED %d tests. \n", failed);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
