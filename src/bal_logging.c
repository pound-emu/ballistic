#include "bal_logging.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

BAL_COLD static void bal_default_logger(void           *user_data,
                                        bal_log_data_t *bal_data,
                                        const char     *format,
                                        va_list         args);

void
bal_log_message(const bal_logger_t   *logger,
                const bal_log_level_t log_level,
                const char           *filename,
                const char           *function,
                const int             line,
                const char           *format,
                ...)
{
    if ((uintptr_t)logger == (uintptr_t)NULL)
    {
        return;
    }

    if (log_level <= BAL_MAX_LOG_LEVEL)
    {
        if (logger->log && log_level <= logger->min_level)
        {
            bal_log_data_t bal_log_data
                = { .filename = filename, .function = function, .level = log_level, .line = line };

            va_list args;
            va_start(args, format);
            logger->log(logger->user_data, &bal_log_data, format, args);
            va_end(args);
        }
    }
}

void
bal_logger_init_default(bal_logger_t *logger)
{
    logger->log       = bal_default_logger;
    logger->min_level = BAL_LOG_LEVEL_TRACE;
}

BAL_COLD static void
bal_default_logger(void *user_data, bal_log_data_t *bal_data, const char *format, va_list args)
{
    (void)user_data;
    const char *level_string = "MISSING";

    switch (bal_data->level)
    {
        case BAL_LOG_LEVEL_NONE: {
            level_string = "NONE";
            break;
        }
        case BAL_LOG_LEVEL_ERROR: {
            level_string = "ERROR";
            break;
        }

        case BAL_LOG_LEVEL_WARN: {
            level_string = "WARN";
            break;
        }

        case BAL_LOG_LEVEL_INFO: {
            level_string = "INFO";
            break;
        }

        case BAL_LOG_LEVEL_DEBUG: {
            level_string = "DEBUG";
            break;
        }

        case BAL_LOG_LEVEL_TRACE: {
            level_string = "TRACE";
            break;
        }
    }

    const char *BAL_RESTRICT filename = bal_data->filename;

#if BAL_COMPILER_MSVC

    const char *BAL_RESTRICT slash      = strrchr(filename, '/');
    const char *BAL_RESTRICT backslash  = strrchr(filename, '\\');
    const char *BAL_RESTRICT last_slash = slash > backslash ? slash : backslash;

    if (last_slash)
    {
        filename = last_slash;
    }

#endif // BAL_COMPILER_MSVC

    fprintf(
        stderr, "[%s] [%s] [%s:%d] ", level_string, bal_data->function, filename, bal_data->line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

/*** end of file ***/
