#include "bal_jit_debug.h"
#include "bal_safety.h"
#include "unity.h"

static bal_allocator_t real_allocator = {};
static bal_allocator_t mock_allocator = {};
static size_t          allocations_before_failure;
static size_t          allocation_count;
static size_t          free_count;
static void           *last_freed_pointer;
static size_t          last_freed_size;

static void *
mock_allocate(bal_allocator_handle_t handle, const size_t alignment, const size_t size)
{
    (void)handle;
    if (allocation_count >= allocations_before_failure)
    {
        return NULL;
    }
    ++allocation_count;
    return real_allocator.allocate(real_allocator.context, alignment, size);
}

static void
mock_free(bal_allocator_handle_t handle, void *pointer, const size_t size)
{
    (void)handle;
    ++free_count;
    last_freed_pointer = pointer;
    last_freed_size    = size;
    real_allocator.free(real_allocator.context, pointer, size);
}

void
setUp(void)
{
    bal_allocator_default_init(&real_allocator);
    mock_allocator              = real_allocator;
    mock_allocator.allocate     = mock_allocate;
    mock_allocator.free         = mock_free;
    allocations_before_failure  = SIZE_MAX;
    allocation_count            = 0U;
    free_count                  = 0U;
    last_freed_pointer          = NULL;
    last_freed_size             = 0U;
    bal_thread_logger.min_level = BAL_LOG_LEVEL_NONE;
}

void
tearDown(void)
{
}

static void
test_BalJitDebugInit_NullContextReturnsErrorInvalidArgument(void)
{
    const bal_error_t error = bal_jit_debug_init(&mock_allocator, NULL);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, error);
}

static void
test_BalJitDebugInit_NullAllocatorReturnsErrorInvalidArgument(void)
{
    bal_jit_debug_context_t context = {};
    const bal_error_t       error   = bal_jit_debug_init(NULL, &context);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, context.status);
}

static void
test_BalJitDebugInit_EntriesAllocationFailsReturnsErrorAllocationFailed(void)
{
    bal_jit_debug_context_t context = {};
    allocations_before_failure      = 0U;
    const bal_error_t error         = bal_jit_debug_init(&mock_allocator, &context);
    TEST_ASSERT_EQUAL(BAL_ERROR_ALLOCATION_FAILED, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_ALLOCATION_FAILED, context.status);
    TEST_ASSERT_EQUAL_size_t(0, allocation_count);
    TEST_ASSERT_EQUAL_size_t(0, free_count);
}

static void
test_BalJitDebugInit_ArenaAllocationFailsReturnsErrorAllocationFailed(void)
{
    bal_jit_debug_context_t context = {};
    allocations_before_failure      = 1U;
    const bal_error_t error         = bal_jit_debug_init(&mock_allocator, &context);
    TEST_ASSERT_EQUAL(BAL_ERROR_ALLOCATION_FAILED, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_ALLOCATION_FAILED, context.status);
    TEST_ASSERT_EQUAL_size_t(1, allocation_count);
    TEST_ASSERT_EQUAL_size_t(1, free_count);
    TEST_ASSERT_EQUAL_size_t(BAL_JIT_DEBUG_ENTRY_CAPACITY * sizeof(bal_jit_block_entry_t),
                             last_freed_size);
}

static void
test_BalJitDebugInit_Success(void)
{
    bal_jit_debug_context_t context = { 0 };
    const bal_error_t       error   = bal_jit_debug_init(&mock_allocator, &context);
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);
    TEST_ASSERT_EQUAL(BAL_SUCCESS, context.status);
    TEST_ASSERT_NOT_NULL(context.entries);
    TEST_ASSERT_NOT_NULL(context.metadata_arena);
    TEST_ASSERT_EQUAL_size_t(BAL_JIT_DEBUG_ENTRY_CAPACITY, context.entry_capacity);
    TEST_ASSERT_EQUAL_size_t(BAL_JIT_DEBUG_ARENA_CAPACITY_BYTES, context.arena_capacity);
    TEST_ASSERT_EQUAL_size_t(0, context.entry_count);
    TEST_ASSERT_EQUAL_size_t(0, context.arena_offset);
    TEST_ASSERT_EQUAL_UINT32(BAL_JIT_DEBUG_MAGIC_ALIVE, context.magic);

    bal_jit_debug_destroy(&mock_allocator, &context);
}

static void
test_BalJitDebugDestroy_NullContextDoesNotCrash(void)
{
    bal_jit_debug_destroy(&mock_allocator, NULL);
    TEST_PASS();
}

static void
test_BalJitDebugDestroy_NullAllocatorDoesNotCrash(void)
{
    bal_jit_debug_context_t context = {};
    bal_jit_debug_destroy(NULL, &context);
    TEST_ASSERT_EQUAL_size_t(0, free_count);
}

static void
test_BalJitDebugDestroy_NullEntriesSkipsEntry(void)
{
    bal_jit_debug_context_t context = {};
    const bal_error_t       error   = bal_jit_debug_init(&mock_allocator, &context);
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);
    void *leaked_entries = context.entries;
    context.entries      = NULL;
    bal_jit_debug_destroy(&mock_allocator, &context);
    TEST_ASSERT_EQUAL_size_t(1, free_count);
    TEST_ASSERT_EQUAL_UINT32(BAL_JIT_DEBUG_MAGIC_DEAD, context.magic);
    real_allocator.free(real_allocator.context,
                        leaked_entries,
                        BAL_JIT_DEBUG_ENTRY_CAPACITY * sizeof(bal_jit_block_entry_t));
}

static void
test_BalJitDebugDestroy_NullArenaSkipsArena(void)
{
    bal_jit_debug_context_t context = {};
    const bal_error_t       error   = bal_jit_debug_init(&mock_allocator, &context);
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);
    void *leaked_arena     = context.metadata_arena;
    context.metadata_arena = NULL;
    bal_jit_debug_destroy(&mock_allocator, &context);
    TEST_ASSERT_EQUAL_size_t(1, free_count);
    TEST_ASSERT_EQUAL_size_t(BAL_JIT_DEBUG_ENTRY_CAPACITY * sizeof(bal_jit_block_entry_t),
                             last_freed_size);
    TEST_ASSERT_EQUAL_UINT32(BAL_JIT_DEBUG_MAGIC_DEAD, context.magic);
    real_allocator.free(real_allocator.context, leaked_arena, BAL_JIT_DEBUG_ARENA_CAPACITY_BYTES);
}

static void
test_BalJitDebugDestroy_ZeroedContextSetsDeadMagicWithoutFreeing(void)
{
    bal_jit_debug_context_t context = {};
    bal_jit_debug_destroy(&mock_allocator, &context);
    TEST_ASSERT_EQUAL_size_t(0, free_count);
    TEST_ASSERT_EQUAL_UINT32(BAL_JIT_DEBUG_MAGIC_DEAD, context.magic);
}

int
main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_BalJitDebugInit_NullContextReturnsErrorInvalidArgument);
    RUN_TEST(test_BalJitDebugInit_NullAllocatorReturnsErrorInvalidArgument);
    RUN_TEST(test_BalJitDebugInit_EntriesAllocationFailsReturnsErrorAllocationFailed);
    RUN_TEST(test_BalJitDebugInit_ArenaAllocationFailsReturnsErrorAllocationFailed);
    RUN_TEST(test_BalJitDebugInit_Success);
    RUN_TEST(test_BalJitDebugDestroy_NullContextDoesNotCrash);
    RUN_TEST(test_BalJitDebugDestroy_NullAllocatorDoesNotCrash);
    RUN_TEST(test_BalJitDebugDestroy_NullEntriesSkipsEntry);
    RUN_TEST(test_BalJitDebugDestroy_NullArenaSkipsArena);
    RUN_TEST(test_BalJitDebugDestroy_ZeroedContextSetsDeadMagicWithoutFreeing);
    return UNITY_END();
}