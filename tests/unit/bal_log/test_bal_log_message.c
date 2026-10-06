#include "bal_log.h"
#include "unity.h"
#include <stdarg.h>
#include <string.h>

static int            callback_call_count = 0;
static bal_log_data_t last_log_data       = { 0 };
static char           last_format[256]    = { 0 };
static char           last_message[256]   = { 0 };
static void          *last_user_data      = NULL;

static void
test_log_callback(void *user_data, bal_log_data_t *bal_data, const char *format, va_list args)
{
    callback_call_count++;
    last_user_data = user_data;

    if (bal_data != NULL)
    {
        last_log_data = *bal_data;
    }

    if (format != NULL)
    {
        (void)strncpy(last_format, format, sizeof(last_format) - 1U);
        last_format[sizeof(last_format) - 1U] = '\0';
        (void)vsnprintf(last_message, sizeof(last_message), format, args);
    }
}

static void
reset_capture(void)
{
    callback_call_count = 0U;
    (void)memset(&last_log_data, 0, sizeof(last_log_data));
    (void)memset(last_format, 0, sizeof(last_format));
    (void)memset(last_message, 0, sizeof(last_message));
    last_user_data = NULL;
}

void
setUp(void)
{
    reset_capture();
    bal_thread_logger.min_level = BAL_LOG_LEVEL_NONE;
}

void
tearDown(void)
{
}

static void
test_BalLogMessage_NullLoggerDoesNotCrash(void)
{
    bal_log_message(NULL, BAL_LOG_LEVEL_ERROR, "file.c", "func", 1, "test %d", 42);
    TEST_PASS();
}

static void
test_BalLogMessage_NullCallbackDoesNotCrash(void)
{
    bal_logger_t logger = {};
    logger.log          = NULL;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_ERROR, "file.c", "func", 1, "test %d", 1);
    TEST_ASSERT_EQUAL_INT(0, callback_call_count);
}

static void
test_BalLogMessage_ZeroedLoggerDoesNotCrash(void)
{
    const bal_logger_t logger = {};
    bal_log_message(&logger, BAL_LOG_LEVEL_ERROR, "file.c", "func", 1, "test");
    TEST_ASSERT_EQUAL_INT(0, callback_call_count);
}

static void
test_BalLogMessage_DispatchWhenLevelEqualsMinimumLevel(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_ERROR;
    bal_log_message(&logger, BAL_LOG_LEVEL_ERROR, "file.c", "func", 1, "test");
    TEST_ASSERT_EQUAL_INT(1, callback_call_count);
}

static void
test_BalLogMessage_DispatchWhenLevelBelowMinimumLevel(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_ERROR;
    bal_log_message(&logger, BAL_LOG_LEVEL_ERROR, "file.c", "func", 1, "test");
    TEST_ASSERT_EQUAL_INT(1, callback_call_count);
}

static void
test_BalLogMessage_FiltersWhenLevelAboveMinimumLevel(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_ERROR;
    bal_log_message(&logger, BAL_LOG_LEVEL_DEBUG, "file.c", "func", 1, "test");
    TEST_ASSERT_EQUAL_INT(0, callback_call_count);
}

static void
test_BalLogMessage_MinimumLevelNoneFiltersAll(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_NONE;
    bal_log_message(&logger, BAL_LOG_LEVEL_ERROR, "file.c", "func", 1, "test");
    TEST_ASSERT_EQUAL_INT(0, callback_call_count);
}

static void
test_BalLogMessage_MinimumLevelTraceDispatchesAll(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;

    for (int level = BAL_LOG_LEVEL_ERROR; level <= BAL_LOG_LEVEL_TRACE; ++level)
    {
        bal_log_message(&logger, level, "file.c", "func", 1, "");
    }

    TEST_ASSERT_EQUAL_INT(5, callback_call_count);
}

static void
test_BalLogMessage_UserDataPassedToCallback(void)
{
    int          sentinel = 42;
    bal_logger_t logger   = {};
    logger.user_data      = &sentinel;
    logger.log            = test_log_callback;
    logger.min_level      = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "file.c", "func", 1, "test");
    TEST_ASSERT_EQUAL_PTR(&sentinel, last_user_data);
}

static void
test_BalLogMessage_MetadataFilenamePassed(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "my_file.c", "func", 1, "test");
    TEST_ASSERT_EQUAL_STRING("my_file.c", last_log_data.filename);
}

static void
test_BalLogMessage_MetadataFunctionPassed(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "file.c", "my_func", 1, "test");
    TEST_ASSERT_EQUAL_STRING("my_func", last_log_data.function);
}

static void
test_BalLogMessage_MetadataLinePassed(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "file.c", "func", 123, "test");
    TEST_ASSERT_EQUAL_INT(123, last_log_data.line);
}

static void
test_BalLogMessage_FormatStringPassed(void)
{
    bal_logger_t logger = {};
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "file.c", "func", 1, "hello %s", "world");
    TEST_ASSERT_EQUAL_STRING("hello %s", last_format);
}

static void
test_BalLogMessage_VariadicArgumentsFormatted(void)
{
    bal_logger_t logger = { 0 };
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "file.c", "func", 1, "value=%d str=%s", 42, "abc");
    TEST_ASSERT_EQUAL_STRING("value=42 str=abc", last_message);
}

static void
test_BalLogMessage_MultipleCallsAccumulate(void)
{
    bal_logger_t logger = { 0 };
    logger.log          = test_log_callback;
    logger.min_level    = BAL_LOG_LEVEL_TRACE;
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "f.c", "fn", 1, "a");
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "f.c", "fn", 2, "b");
    bal_log_message(&logger, BAL_LOG_LEVEL_INFO, "f.c", "fn", 3, "c");
    TEST_ASSERT_EQUAL_INT(3, callback_call_count);
}

int
main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_BalLogMessage_NullLoggerDoesNotCrash);
    RUN_TEST(test_BalLogMessage_NullCallbackDoesNotCrash);
    RUN_TEST(test_BalLogMessage_ZeroedLoggerDoesNotCrash);
    RUN_TEST(test_BalLogMessage_DispatchWhenLevelEqualsMinimumLevel);
    RUN_TEST(test_BalLogMessage_DispatchWhenLevelBelowMinimumLevel);
    RUN_TEST(test_BalLogMessage_FiltersWhenLevelAboveMinimumLevel);
    RUN_TEST(test_BalLogMessage_MinimumLevelNoneFiltersAll);
    RUN_TEST(test_BalLogMessage_MinimumLevelTraceDispatchesAll);
    RUN_TEST(test_BalLogMessage_UserDataPassedToCallback);
    RUN_TEST(test_BalLogMessage_MetadataFilenamePassed);
    RUN_TEST(test_BalLogMessage_MetadataFunctionPassed);
    RUN_TEST(test_BalLogMessage_MetadataLinePassed);
    RUN_TEST(test_BalLogMessage_FormatStringPassed);
    RUN_TEST(test_BalLogMessage_VariadicArgumentsFormatted);
    RUN_TEST(test_BalLogMessage_MultipleCallsAccumulate);
    return UNITY_END();
}