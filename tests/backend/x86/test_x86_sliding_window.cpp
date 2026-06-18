#include "backend/x86/bal_x86_assembler.h"
#include "backend/x86/bal_x86_sliding_window.h"
#include "bal_logging.h"
#include "gtest/gtest.h"
#include <cstring>

class BackendSlidingWindow : public testing::Test
{
protected:
    static constexpr size_t BUFFER_SIZE         = 4096;
    uint8_t                 buffer[BUFFER_SIZE] = {};
    bal_logger_t            logger              = {};
    bal_x86_assembler_t     assembler           = {};
    bal_sliding_window_t    window              = {};

    void SetUp() override
    {
        (void)std::memset(buffer, 0, BUFFER_SIZE);
        (void)std::memset(&logger, 0, sizeof(bal_logger_t));
        (void)std::memset(&assembler, 0, sizeof(bal_x86_assembler_t));
        (void)std::memset(&window, 0, sizeof(bal_sliding_window_t));
        bal_logger_init_default(&logger);
        const bal_executable_buffer_t executable_buffer = { buffer, buffer };
        const bal_error_t             error
            = bal_x86_assembler_init(&assembler, executable_buffer, BUFFER_SIZE, logger);
        ASSERT_EQ(error, BAL_SUCCESS);
    }

    static bal_x86_macro_t GenerateDummyMacro(const uint8_t id)
    {
        bal_x86_macro_t macro;
        (void)std::memset(&macro, 0, sizeof(bal_x86_macro_t));
        macro.opcode              = BAL_X86_MACRO_LOAD;
        macro.destination         = static_cast<bal_x86_register_t>(id % 16);
        macro.immediate_or_offset = id * 8;
        return macro;
    }
};

TEST_F(BackendSlidingWindow, Init_NullWindow)
{
    const bal_error_t status = bal_sliding_window_init(nullptr, &assembler);
    EXPECT_EQ(status, BAL_ERROR_INVALID_ARGUMENT);
}

TEST_F(BackendSlidingWindow, Init_NullAssembler)
{
    const bal_error_t status = bal_sliding_window_init(&window, nullptr);
    EXPECT_EQ(status, BAL_ERROR_INVALID_ARGUMENT);
}

TEST_F(BackendSlidingWindow, Init_Success)
{
    const bal_error_t status = bal_sliding_window_init(&window, &assembler);
    EXPECT_EQ(status, BAL_SUCCESS);
    EXPECT_EQ(window.count, 0);
    EXPECT_EQ(window.assembler, &assembler);
}

TEST_F(BackendSlidingWindow, Push_SingleMacro_IncrementsCount)
{
    ASSERT_EQ(bal_sliding_window_init(&window, &assembler), BAL_SUCCESS);
    const bal_x86_macro_t macro = GenerateDummyMacro(1);
    bal_sliding_window_push(&window, macro);
    EXPECT_EQ(window.count, 1);
    EXPECT_EQ(window.macros[0].opcode, macro.opcode);
    EXPECT_EQ(window.macros[0].destination, macro.destination);
    EXPECT_EQ(window.macros[0].immediate_or_offset, macro.immediate_or_offset);
}

TEST_F(BackendSlidingWindow, FlushAll_WithData_ResetsCount)
{
    ASSERT_EQ(bal_sliding_window_init(&window, &assembler), BAL_SUCCESS);
    bal_sliding_window_push(&window, GenerateDummyMacro(1));
    bal_sliding_window_push(&window, GenerateDummyMacro(2));
    ASSERT_EQ(window.count, 2);
    bal_sliding_window_flush_all(&window);
    EXPECT_EQ(window.count, 0);
}

TEST_F(BackendSlidingWindow, FlushAll_EmptyWindow_IsNoOp)
{
    ASSERT_EQ(bal_sliding_window_init(&window, &assembler), BAL_SUCCESS);
    ASSERT_EQ(window.count, 0);
    bal_sliding_window_flush_all(&window);
    ASSERT_EQ(window.count, 0);
}

TEST_F(BackendSlidingWindow, Push_ExactCapacity_Success)
{
    ASSERT_EQ(bal_sliding_window_init(&window, &assembler), BAL_SUCCESS);

    for (size_t i = 0; i < BAL_SLIDING_WINDOW_CAPACITY; ++i)
    {
        bal_sliding_window_push(&window, GenerateDummyMacro(i));
    }

    EXPECT_EQ(window.count, BAL_SLIDING_WINDOW_CAPACITY);
}

TEST_F(BackendSlidingWindow, Push_ExceedsCapacity_AutoFlushes)
{
    ASSERT_EQ(bal_sliding_window_init(&window, &assembler), BAL_SUCCESS);

    for (size_t i = 0; i < BAL_SLIDING_WINDOW_CAPACITY; ++i)
    {
        bal_sliding_window_push(&window, GenerateDummyMacro(i));
    }

    EXPECT_EQ(window.count, BAL_SLIDING_WINDOW_CAPACITY);
    const size_t          assembler_offset_before_flush = assembler.offset;
    const bal_x86_macro_t overflow_macro                = GenerateDummyMacro(99);
    bal_sliding_window_push(&window, overflow_macro);
    EXPECT_EQ(window.count, 1);
    EXPECT_EQ(window.macros[0].immediate_or_offset, overflow_macro.immediate_or_offset);
    EXPECT_GT(assembler.offset, assembler_offset_before_flush);
}

/*** end of file ***/
