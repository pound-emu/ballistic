#include "bal_jit_debug.h"
#include "bal_safety.h"
#include "unity.h"

static bal_jit_debug_context_t   context;
static bal_allocator_t           allocator;
static uint8_t                   fake_jit_code[64];
static bal_jit_instruction_map_t mappings[2];

static bal_error_t
add_valid_block(void)
{
    return bal_jit_debug_add_block(&context, fake_jit_code, 64U, 0x1000U, mappings, 2U);
}

void
setUp(void)
{
    bal_allocator_default_init(&allocator);
    (void)memset(&context, 0, sizeof(context));
    (void)memset(fake_jit_code, 0, sizeof(fake_jit_code));
    mappings[0] = (bal_jit_instruction_map_t) { .x86_offset = 0, .guest_pc_offset = 0 };
    mappings[1] = (bal_jit_instruction_map_t) { .x86_offset = 16, .guest_pc_offset = 4 };
    bal_thread_logger.min_level = BAL_LOG_LEVEL_NONE;
    const bal_error_t error     = bal_jit_debug_init(&allocator, &context);
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);
}

void
tearDown(void)
{
    bal_jit_debug_destroy(&allocator, &context);
}

static void
test_BalJitDebugAddBlock_NullContextReturnsErrorInvalidArgument(void)
{
    const bal_error_t error
        = bal_jit_debug_add_block(NULL, fake_jit_code, 64U, 0x1000U, mappings, 2U);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, error);
}

static void
test_BalJitDebugAddBlock_PoisonedStatusReturnsSameStatus(void)
{
    context.status          = BAL_ERROR_MEMORY_FAULT;
    const bal_error_t error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_ERROR_MEMORY_FAULT, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_MEMORY_FAULT, context.status);
    TEST_ASSERT_EQUAL_size_t(0, context.entry_count);
}

static void
test_BalJitDebugAddBlock_NullRxStartReturnsInvalidArgument(void)
{
    const bal_error_t error = bal_jit_debug_add_block(&context, NULL, 64U, 0x1000U, mappings, 2U);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, context.status);
    TEST_ASSERT_EQUAL_size_t(0, context.entry_count);
}

static void
test_BalJitDebugAddBlock_NullMappingReturnsErrorInvalidArgument(void)
{
    const bal_error_t error
        = bal_jit_debug_add_block(&context, fake_jit_code, 64U, 0x1000U, NULL, 2U);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, context.status);
}

static void
test_BalJitDebugAddBlock_ZeroInstructionCountReturnsErrorInvalidArgument(void)
{
    const bal_error_t error
        = bal_jit_debug_add_block(&context, fake_jit_code, 64U, 0x1000U, mappings, 0U);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_INVALID_ARGUMENT, context.status);
}

static void
test_BalJitDebugAddBlock_FullEntriesReturnsErrorBufferOverflow(void)
{
    context.entry_capacity  = 0U;
    const bal_error_t error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_ERROR_BUFFER_OVERFLOW, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_BUFFER_OVERFLOW, context.status);
    TEST_ASSERT_EQUAL(0, context.entry_count);
}

static void
test_BalJitDebugAddBlock_DeadMagicReturnsErrorStructCorrupted(void)
{
    bal_jit_debug_destroy(&allocator, &context);
    const bal_error_t error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_ERROR_STRUCT_CORRUPTED, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_STRUCT_CORRUPTED, context.status);
}

static void
test_BalJitDebugAddBlock_FullArenaReturnsErrorBufferOverflow(void)
{
    context.arena_capacity  = 1U;
    const bal_error_t error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_ERROR_BUFFER_OVERFLOW, error);
    TEST_ASSERT_EQUAL(BAL_ERROR_BUFFER_OVERFLOW, context.status);
    TEST_ASSERT_EQUAL_size_t(0, context.entry_count);
    TEST_ASSERT_EQUAL_size_t(0, context.arena_offset);
}

static void
test_BalJitDebugAddBlock_ExactFitArenaSucceedsThenOverflows(void)
{
    context.arena_capacity
        = sizeof(bal_jit_block_metadata_t) + 2U * sizeof(bal_jit_instruction_map_t);

    bal_error_t error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);
    TEST_ASSERT_EQUAL_size_t(1, context.entry_count);
    TEST_ASSERT_EQUAL_size_t(context.arena_capacity, context.arena_offset);

    error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_ERROR_BUFFER_OVERFLOW, error);
    TEST_ASSERT_EQUAL_size_t(1, context.entry_count);
}

static void
test_BalJitDebugAddBlock_SingleBlockSuccess(void)
{
    const bal_error_t error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);
    TEST_ASSERT_EQUAL(BAL_SUCCESS, context.status);
    TEST_ASSERT_EQUAL_size_t(1, context.entry_count);

    const bal_jit_block_entry_t *entry = &context.entries[0];
    TEST_ASSERT_EQUAL_PTR(fake_jit_code, entry->rx_start);
    TEST_ASSERT_EQUAL_UINT32(64, entry->rx_size);
    TEST_ASSERT_NOT_NULL(entry->metadata);

    const bal_jit_block_metadata_t *metadata = entry->metadata;
    TEST_ASSERT_EQUAL_PTR(context.metadata_arena, metadata);
    TEST_ASSERT_EQUAL_UINT64(0x1000, metadata->base_guest_pc);
    TEST_ASSERT_EQUAL_UINT32(2, metadata->instruction_count);
}

static void
test_BalJitDebugAddBlock_MultipleBlockSuccess(void)
{
    bal_error_t error = add_valid_block();
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);

    const bal_jit_instruction_map_t second_mapping = { .x86_offset = 0U, .guest_pc_offset = 0U };
    uint8_t                         second_jit_code[32] = {};
    error                                               = bal_jit_debug_add_block(
        &context, second_jit_code, sizeof(second_jit_code), 0x2000U, &second_mapping, 1U);
    TEST_ASSERT_EQUAL(BAL_SUCCESS, error);
    TEST_ASSERT_EQUAL_size_t(2, context.entry_count);
    const bal_jit_block_entry_t *entry0 = &context.entries[0];
    const bal_jit_block_entry_t *entry1 = &context.entries[1];
    TEST_ASSERT_EQUAL_PTR(fake_jit_code, entry0->rx_start);
    TEST_ASSERT_EQUAL_PTR(second_jit_code, entry1->rx_start);
    TEST_ASSERT_EQUAL_UINT32(32, entry1->rx_size);
    TEST_ASSERT_EQUAL_UINT64(0x2000, entry1->metadata->base_guest_pc);
    TEST_ASSERT_EQUAL_UINT32(1, entry1->metadata->instruction_count);

    const size_t expected_second_offset
        = sizeof(bal_jit_block_metadata_t) + 2U * sizeof(bal_jit_instruction_map_t);
    TEST_ASSERT_EQUAL_PTR(context.metadata_arena + expected_second_offset, entry1->metadata);
    TEST_ASSERT_EQUAL_size_t(expected_second_offset + sizeof(bal_jit_block_metadata_t)
                                 + 1U * sizeof(bal_jit_instruction_map_t),
                             context.arena_offset);
}

int
main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_BalJitDebugAddBlock_NullContextReturnsErrorInvalidArgument);
    RUN_TEST(test_BalJitDebugAddBlock_PoisonedStatusReturnsSameStatus);
    RUN_TEST(test_BalJitDebugAddBlock_NullRxStartReturnsInvalidArgument);
    RUN_TEST(test_BalJitDebugAddBlock_NullMappingReturnsErrorInvalidArgument);
    RUN_TEST(test_BalJitDebugAddBlock_ZeroInstructionCountReturnsErrorInvalidArgument);
    RUN_TEST(test_BalJitDebugAddBlock_FullEntriesReturnsErrorBufferOverflow);
    RUN_TEST(test_BalJitDebugAddBlock_DeadMagicReturnsErrorStructCorrupted);
    RUN_TEST(test_BalJitDebugAddBlock_FullArenaReturnsErrorBufferOverflow);
    RUN_TEST(test_BalJitDebugAddBlock_ExactFitArenaSucceedsThenOverflows);
    RUN_TEST(test_BalJitDebugAddBlock_SingleBlockSuccess);
    RUN_TEST(test_BalJitDebugAddBlock_MultipleBlockSuccess);
    return UNITY_END();
}