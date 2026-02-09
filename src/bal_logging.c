#include "bal_logging.h"
#include <stdio.h>

BAL_COLD static void bal_default_logger(void           *user_data,
                                        bal_log_level_t level,
                                        const char     *filename,
                                        const char     *function,
                                        int             line,
                                        const char     *format,
                                        va_list         args);

void
bal_log_message(bal_logger_t   *logger,
                bal_log_level_t level,
                const char     *filename,
                const char     *function,
                int             line,
                const char     *format,
                ...)
{
    va_list args;
    va_start(args, format);
    logger->log(logger->user_data, level, filename, function, line, format, args);
    va_end(args);
}

void
bal_logger_init_default(bal_logger_t *logger)
{
    logger->log       = bal_default_logger;
    logger->min_level = BAL_LOG_LEVEL_TRACE;
}

BAL_COLD static void
bal_default_logger(void           *user_data,
                   bal_log_level_t level,
                   const char     *filename,
                   const char     *function,
                   int             line,
                   const char     *format,
                   va_list         args)
{
    (void)user_data;
    const char *level_string = "MISSING";

    switch (level)
    {
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

    fprintf(stderr, "[%s] [%s] [%s:%d] ", level_string, function, filename, line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

/*** end of file ***/
