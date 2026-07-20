#include "backend/bal_cpu.h"
#include "bal_jit_debug.h"
#include "bal_memory.h"
#include "bal_safety.h"

#include "gtest/gtest.h"

class JitDebug : public testing::Test
{
protected:
    bal_allocator_t         allocator = {};
    char                    pad0[56]  = {};
    bal_jit_debug_context_t context   = {};
    bal_logger_t            logger    = {};
    char                    pad1[40]  = {};

    void SetUp() override
    {
        bal_allocator_default_init(&allocator);
        bal_logger_init_default(&logger);
    }
};

TEST_F(JitDebug, Init_Success)
{
    EXPECT_EQ(bal_jit_debug_init(&allocator, &context, logger), BAL_SUCCESS);
    EXPECT_NE(context.entries, nullptr);
    EXPECT_NE(context.metadata_arena, nullptr);
    EXPECT_EQ(context.entry_capacity, BAL_JIT_DEBUG_ENTRY_CAPACITY);
    EXPECT_EQ(context.arena_capacity, BAL_JIT_DEBUG_ARENA_CAPACITY_BYTES);
    EXPECT_EQ(context.magic, BAL_JIT_DEBUG_MAGIC_ALIVE);
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, Destroy_Success)
{
    bal_jit_debug_init(&allocator, &context, logger);
    bal_jit_debug_destroy(&allocator, &context);
    EXPECT_EQ(context.entries, nullptr);
    EXPECT_EQ(context.metadata_arena, nullptr);
    EXPECT_EQ(context.magic, BAL_JIT_DEBUG_MAGIC_DEAD);
}

TEST_F(JitDebug, AddBlock_Success)
{
    bal_jit_debug_init(&allocator, &context, logger);
    const bal_jit_instruction_map_t map      = { 0, 0 };
    const auto                      dummy_rx = reinterpret_cast<void *>(0x1000);

    EXPECT_EQ(bal_jit_debug_add_block(&context, dummy_rx, 64, 0x2000, &map, 1), BAL_SUCCESS);
    EXPECT_EQ(context.entry_count, 1);
    EXPECT_EQ(context.entries[0].rx_start, dummy_rx);
    EXPECT_EQ(context.entries[0].rx_size, 64);
    EXPECT_EQ(context.entries[0].metadata->base_guest_pc, 0x2000);
    EXPECT_EQ(context.entries[0].metadata->instruction_count, 1);
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, SignalRegistration_Success)
{
    bal_jit_debug_init(&allocator, &context, logger);
    auto dummy_buffer = reinterpret_cast<void *>(0x3000);

    EXPECT_EQ(bal_jit_debug_register_signal_handler(&context, dummy_buffer, 4096), BAL_SUCCESS);
    EXPECT_EQ(context.jit_buffer_start, dummy_buffer);
    EXPECT_EQ(context.jit_buffer_end,
              reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(dummy_buffer) + 4096));
    bal_jit_debug_unregister_signal_handler(&context);
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, SignalUnregistration_Success)
{
    bal_jit_debug_init(&allocator, &context, logger);
    const auto dummy_buffer = reinterpret_cast<void *>(0x3000);
    bal_jit_debug_register_signal_handler(&context, dummy_buffer, 4096);
    bal_jit_debug_unregister_signal_handler(&context);
    EXPECT_EQ(context.jit_buffer_start, nullptr);
    EXPECT_EQ(context.jit_buffer_end, nullptr);
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, CrashAtBufferStart_Success)
{
    uint8_t                         dummy_block[64];
    const uint64_t                  base_guest_pc = 0x80000000;
    const bal_jit_instruction_map_t mappings[]    = { { 0, 0 }, { 16, 4 }, { 32, 8 } };

    bal_error_t error = bal_jit_debug_init(&allocator, &context, logger);
    ASSERT_EQ(error, BAL_SUCCESS);

    error = bal_jit_debug_add_block(
        &context, dummy_block, sizeof(dummy_block), base_guest_pc, mappings, 3);
    ASSERT_EQ(error, BAL_SUCCESS);

    bal_cpu_t cpu        = {};
    cpu.debug_context    = &context;
    const auto rbp       = (uint64_t)&cpu;
    const auto fault_rip = (uint64_t)dummy_block;
    EXPECT_DEATH(handle_jit_fault(fault_rip, rbp), "JIT CRASH!.*Guest PC: 0x80000000");
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, CrashAtBufferOffset_Success)
{
    uint8_t                         dummy_block[64];
    const uint64_t                  base_guest_pc = 0x80001000;
    const bal_jit_instruction_map_t mappings[]    = { { 0, 0 }, { 10, 4 }, { 20, 8 } };

    bal_error_t error = bal_jit_debug_init(&allocator, &context, logger);
    ASSERT_EQ(error, BAL_SUCCESS);

    error = bal_jit_debug_add_block(
        &context, dummy_block, sizeof(dummy_block), base_guest_pc, mappings, 3);
    ASSERT_EQ(error, BAL_SUCCESS);

    bal_cpu_t cpu            = {};
    cpu.debug_context        = &context;
    const auto     rbp       = (uint64_t)&cpu;
    const uint64_t fault_rip = (uint64_t)dummy_block + 15;
    EXPECT_DEATH(handle_jit_fault(fault_rip, rbp), "JIT CRASH!.*Guest PC: 0x80001004");
    bal_jit_debug_destroy(&allocator, &context);
}

static void
test_crash_callback(void       *user_data,
                    uint64_t    guest_pc,
                    uint64_t    host_rip,
                    uint32_t    jit_offset,
                    const void *jit_block_start,
                    uint32_t    jit_block_size)
{
    (void)user_data;

    fprintf(stderr,
            "CRASH_DATA: guest_pc=0x%llX, host_rip=0x%llX, jit_offset=%u, block_start=%p, "
            "block_size=%u\n",
            (unsigned long long)guest_pc,
            (unsigned long long)host_rip,
            jit_offset,
            jit_block_start,
            jit_block_size);
}

TEST_F(JitDebug, CrashWithCustomCallback_Success)
{
    uint8_t                         dummy_block[64];
    const uint64_t                  base_guest_pc = 0x80002000;
    const bal_jit_instruction_map_t mappings[]    = { { 0, 0 }, { 10, 4 }, { 20, 8 } };

    bal_error_t error = bal_jit_debug_init(&allocator, &context, logger);
    ASSERT_EQ(error, BAL_SUCCESS);

    error = bal_jit_debug_add_block(
        &context, dummy_block, sizeof(dummy_block), base_guest_pc, mappings, 3);
    ASSERT_EQ(error, BAL_SUCCESS);
    context.crash_callback           = test_crash_callback;
    context.crash_callback_user_data = nullptr;

    bal_cpu_t cpu            = {};
    cpu.debug_context        = &context;
    const auto     rbp       = (uint64_t)&cpu;
    const uint64_t fault_rip = (uint64_t)dummy_block + 25;
    EXPECT_DEATH(handle_jit_fault(fault_rip, rbp),
                 "CRASH_DATA: guest_pc=0x80002008.*jit_offset=25.*block_size=64");
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, ArenaCapacityExactFit_Success)
{
    // Arena capacity is 4MB == 4194304 bytes.
    // Metadata is 32 bytes. Each mapping is 8 bytes.
    // 32 + N * 8 = 4194304  =>  N * 8 = 4194272  =>  N = 524284
    const uint32_t instruction_count = 524284;

    const std::vector<bal_jit_instruction_map_t> mappings(instruction_count);
    uint8_t                                      dummy_block[64];
    bal_error_t error = bal_jit_debug_init(&allocator, &context, logger);
    ASSERT_EQ(error, BAL_SUCCESS);

    error = bal_jit_debug_add_block(
        &context, dummy_block, sizeof(dummy_block), 0x1000, mappings.data(), instruction_count);
    ASSERT_EQ(error, BAL_SUCCESS);

    EXPECT_EQ(error, BAL_SUCCESS);
    EXPECT_EQ(context.arena_offset, context.arena_capacity);
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, ArenaCapacityExactFitWithLoop_Success)
{
    uint8_t                         dummy_block[64];
    const bal_jit_instruction_map_t mapping = { 0, 0 };
    bal_error_t                     error   = bal_jit_debug_init(&allocator, &context, logger);
    EXPECT_EQ(error, BAL_SUCCESS);

    for (size_t i = 0; i < 8192; ++i)
    {
        error = bal_jit_debug_add_block(
            &context, dummy_block, sizeof(dummy_block), 0x1000 + i * 4, &mapping, 1);
        ASSERT_EQ(error, BAL_SUCCESS) << "Failed at block " << i;
    }

    EXPECT_EQ(context.entry_count, 8192);
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, ArenaCapacityOverflow_Failure)
{
    const uint32_t                               instruction_count = 524285;
    const std::vector<bal_jit_instruction_map_t> mappings(instruction_count);
    uint8_t                                      dummy_block[64];
    const size_t                                 old_offset = context.arena_offset;
    bal_error_t error = bal_jit_debug_init(&allocator, &context, logger);
    EXPECT_EQ(error, BAL_SUCCESS);

    error = bal_jit_debug_add_block(
        &context, dummy_block, sizeof(dummy_block), 0x1000, mappings.data(), instruction_count);

    EXPECT_EQ(error, BAL_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(context.arena_offset, old_offset);
    bal_jit_debug_destroy(&allocator, &context);
}

TEST_F(JitDebug, ArenaCapacityOverflowWithLoop_Failure)
{
    uint8_t                         dummy_block[64];
    const bal_jit_instruction_map_t mapping = { 0, 0 };
    bal_error_t                     error   = bal_jit_debug_init(&allocator, &context, logger);
    ASSERT_EQ(error, BAL_SUCCESS);

    for (size_t i = 0; i < 8192; ++i)
    {
        (void)bal_jit_debug_add_block(
            &context, dummy_block, sizeof(dummy_block), 0x1000 + i * 4, &mapping, 1);
    }

    error = bal_jit_debug_add_block(
        &context, dummy_block, sizeof(dummy_block), 0x1000 + 8192 * 4, &mapping, 1);

    EXPECT_EQ(error, BAL_ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(context.entry_count, 8192);
    bal_jit_debug_destroy(&allocator, &context);
}