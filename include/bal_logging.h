#ifndef BALLISTIC_LOGGING_H
#define BALLISTIC_LOGGING_H

#include "bal_attributes.h"
#include <stdarg.h>

typedef enum
{
    BAL_LOG_LEVEL_ERROR = 0,
    BAL_LOG_LEVEL_WARN  = 1,
    BAL_LOG_LEVEL_INFO  = 2,
    BAL_LOG_LEVEL_DEBUG = 3,
    BAL_LOG_LEVEL_TRACE = 4,
} bal_log_level_t;

typedef struct
{
    /// Source file where the log occured.
    const char *filename;

    /// The function name where the log occured.
    const char *function;

    // The log level.
    bal_log_level_t level;

    // The line number where the log occured
    int line;
} bal_log_data_t;

typedef void (*bal_log_function_t)(void           *user_data,
                                   bal_log_data_t *bal_data,
                                   const char     *format,
                                   va_list         args);

typedef struct
{
    void              *user_data;
    bal_log_function_t log;
    bal_log_level_t    min_level;
} bal_logger_t;

// Remove all log code if log level not defined.
//
#ifndef BAL_MAX_LOG_LEVEL

#define BAL_MAX_LOG_LEVEL BAL_LOG_LEVEL_ERROR

#endif

BAL_COLD void bal_log_message(bal_logger_t *logger, bal_log_data_t *data, const char *format, ...);

/// Populates `logger` with Ballistic's default logging implementation.
///
/// # Safety
///
/// `logger` must NOT be `NULL`.
BAL_COLD void bal_logger_init_default(bal_logger_t *logger);

#define BAL_LOG(logger, log_level, format, ...)                          \
    do                                                                   \
    {                                                                    \
        if (!(logger))                                                   \
        {                                                                \
            break;                                                       \
        }                                                                \
                                                                         \
        if (log_level <= BAL_MAX_LOG_LEVEL)                              \
        {                                                                \
            if ((logger)->log && log_level <= (logger)->min_level)       \
            {                                                            \
                bal_log_data_t data = { .filename = __FILE__,            \
                                        .function = __func__,            \
                                        .level    = log_level,           \
                                        .line     = __LINE__ };          \
                bal_log_message((logger), &data, format, ##__VA_ARGS__); \
            }                                                            \
        }                                                                \
    } while (0)

#define BAL_LOG_ERROR(logger, format, ...) \
    BAL_LOG(logger, BAL_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define BAL_LOG_WARN(logger, format, ...) BAL_LOG(logger, BAL_LOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define BAL_LOG_INFO(logger, format, ...) BAL_LOG(logger, BAL_LOG_LEVEL_INFO, format, ##__VA_ARGS__)

#ifndef NDEBUG
#define BAL_LOG_DEBUG(logger, format, ...) \
    BAL_LOG(logger, BAL_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define BAL_LOG_TRACE(logger, format, ...) \
    BAL_LOG(logger, BAL_LOG_LEVEL_TRACE, format, ##__VA_ARGS__)
#else
#define BAL_LOG_DEBUG(logger, format, ...) \
    do                                     \
    {                                      \
    } while (0)
#define BAL_LOG_TRACE(logger, format, ...) \
    do                                     \
    {                                      \
    } while (0)
#endif

#endif /* BALLISTIC_LOGGING_H */

/*** end of file ***/
