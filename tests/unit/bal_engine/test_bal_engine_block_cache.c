#include "bal_engine.h"
#include "bal_engine_flags.h"
#include "bal_memory.h"
#include "unity.h"
#include <stdint.h>
#include <string.h>

#define GUEST_MEMORY_SIZE 4096U

#define BLOCK_COUNT     5U
#define FIRST_BLOCK_GVA 0x100U
#define BLOCK_STRIDE    0x200U

static uint32_t
movz_x8(const uint32_t imm16)
{
    return 0xD2800000U | (imm16 << 5) | 8U;
}

#define RET_ENCODING 0xD65F03C0U

static bal_allocator_t        allocator        = {};
static bal_memory_interface_t memory_interface = {};
static uint32_t               guest_memory[GUEST_MEMORY_SIZE / sizeof(uint32_t)];

void
setUp(void)
{
    (void)memset(guest_memory, 0, sizeof(guest_memory));

    for (uint32_t i = 0; i < BLOCK_COUNT; ++i)
    {
        const uint32_t word_index            = (FIRST_BLOCK_GVA + i * BLOCK_STRIDE) / sizeof(uint32_t);
        guest_memory[word_index]             = movz_x8(i + 1U);
        guest_memory[word_index + 1]         = RET_ENCODING;
    }

    bal_allocator_default_init(&allocator);
    TEST_ASSERT_EQUAL_INT(BAL_SUCCESS,
                          bal_flat_translation_interface_init(
                              &allocator, &memory_interface, guest_memory, sizeof(guest_memory)));

    bal_thread_logger.min_level = BAL_LOG_LEVEL_NONE;
}

void
tearDown(void)
{
    (void)bal_flat_translation_interface_destroy(&allocator, &memory_interface);
}

static void
execute_block(bal_engine_t *engine, bal_cpu_t *cpu, const uint32_t index)
{
    cpu->pc   = FIRST_BLOCK_GVA + index * BLOCK_STRIDE;
    cpu->x[8] = 0ULL;

    TEST_ASSERT_EQUAL_INT(BAL_SUCCESS, bal_engine_run_thread(engine));
    TEST_ASSERT_EQUAL_UINT64(index + 1ULL, cpu->x[8]);
}

static void
test_BlockCacheFiveBlocksInOneSet_EvictedBlockReExecutesCorrectly(void)
{
    bal_cpu_t cpu = {};
    cpu.x[30] = BAL_ENGINE_SENTINEL;

    bal_engine_t engine = {};
    TEST_ASSERT_EQUAL_INT(
        BAL_SUCCESS, bal_engine_init(&engine, &cpu, &allocator, &memory_interface));

    for (uint32_t i = 0; i < BLOCK_COUNT; ++i)
    {
        execute_block(&engine, &cpu, i);
    }

    for (uint32_t i = 0; i < BLOCK_COUNT; ++i)
    {
        execute_block(&engine, &cpu, i);
    }

    bal_engine_destroy(&engine);
}

static void
test_ClearCacheRequest_ResetsCacheAndLayout(void)
{
    bal_cpu_t cpu = {};
    cpu.x[30] = BAL_ENGINE_SENTINEL;

    bal_engine_t engine = {};
    TEST_ASSERT_EQUAL_INT(
        BAL_SUCCESS, bal_engine_init(&engine, &cpu, &allocator, &memory_interface));

    for (uint32_t i = 0; i < BLOCK_COUNT; ++i)
    {
        execute_block(&engine, &cpu, i);
    }

    bal_engine_clear_cache(&engine);

    for (uint32_t i = 0; i < BLOCK_COUNT; ++i)
    {
        execute_block(&engine, &cpu, i);
    }

    bal_engine_destroy(&engine);
}

static void
test_DisableBlockCacheFlag_EveryBlockRecompiles(void)
{
    bal_cpu_t cpu = {};
    cpu.x[30] = BAL_ENGINE_SENTINEL;

    bal_engine_t engine = {};
    TEST_ASSERT_EQUAL_INT(
        BAL_SUCCESS, bal_engine_init(&engine, &cpu, &allocator, &memory_interface));

    engine.flags |= BAL_ENGINE_FLAG_DISABLE_BLOCK_CACHE;

    for (uint32_t pass = 0; pass < 2U; ++pass)
    {
        for (uint32_t i = 0; i < BLOCK_COUNT; ++i)
        {
            execute_block(&engine, &cpu, i);
        }
    }

    bal_engine_destroy(&engine);
}

int
main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_BlockCacheFiveBlocksInOneSet_EvictedBlockReExecutesCorrectly);
    RUN_TEST(test_ClearCacheRequest_ResetsCacheAndLayout);
    RUN_TEST(test_DisableBlockCacheFlag_EveryBlockRecompiles);
    return UNITY_END();
}
