#include "bal_log.h"
#include "unity.h"
#include <string.h>

void
setUp(void)
{
    (void)memset(&bal_thread_logger, 0, sizeof(bal_thread_logger));
    bal_thread_logger.min_level = BAL_LOG_LEVEL_NONE;
}

void
tearDown(void)
{
}

static void
test_BalLoggerInitDefault_SetCallbackToNonNull(void)
{
    bal_logger_init_default();
    TEST_ASSERT_NOT_NULL(bal_thread_logger.log);
}

static void
test_BalLoggerInitDefault_OverwritesPreviousState(void)
{
    bal_thread_logger.log       = NULL;
    bal_thread_logger.min_level = BAL_LOG_LEVEL_NONE;
    bal_thread_logger.user_data = (void *)0xDEADU;
    bal_logger_init_default();
    TEST_ASSERT_NOT_NULL(bal_thread_logger.log);
    TEST_ASSERT_EQUAL_INT(BAL_LOG_LEVEL_TRACE, bal_thread_logger.min_level);
}

int
main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_BalLoggerInitDefault_SetCallbackToNonNull);
    RUN_TEST(test_BalLoggerInitDefault_OverwritesPreviousState);
    return UNITY_END();
}