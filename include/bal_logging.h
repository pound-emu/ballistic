//! This module defines the logging system used by Ballistic. It routes log messages through a
//! user-provided callback so users can integrate this module into their application's logging
//! backend.
//!
//! # Configuration
//!
//! The verbosity is controled by two mechanisms:
//!
//! 1. Compile Time: The `BAL_MAX_LOG_LEVEL` macro determins the maximum severity compiled into
//!    the binary. This is set in CMakeLists.txt. Logs below this level are compiled out via
//!    dead code elimination.
//! 2. Runtime: The `min_level` field in `bal_logger_t` filters messages dynamically.
//!
//! # Examples
//!
//! ## Default Initialization
//!
//! ```c
//! #include "bal_logging.h"
//!
//! bal_logger_t logger = {0};
//! bal_logger_init_default(&logger);
//! BAL_LOG_INFO(&logger, "Engine initialized.");
//! ```
//!
//! ## Custom Backend
//!
//! ```c
//! #include "bal_logging.h"
//! #include <stdio.h>
//!
//! void my_file_logger(void *user_data, bal_log_data_t *bal_data, const char *fmt, va_list args)
//! {
//!     FILE *f = (FILE *)user_data;
//!
//!     // Format: [LOG_LEVEL] file:line message
//!     fprintf(f, "[%d] %s:%d ", bal_data->level, bal_data->filename, bal_data->line);
//!     vfprintf(f, fmt, args);
//!     fprintf(f, "\n");
//! }
//!
//! // ---
//!
//! FILE *log_file = fopen("jit.log", "w");
//! bal_logger_t logger = {
//!     .user_data = log_file,
//!     .log = my_file_logger,
//!     .min_level = BAL_LOG_LEVEL_DEBUG
//! };
//!
//! BAL_LOG_DEBUG(&logger, "Writing to custom file backend");
//! ```

#ifndef BALLISTIC_LOGGING_H
#define BALLISTIC_LOGGING_H

#include "bal_attributes.h"
#include <stdarg.h>

/// Defines the severity of a log message.
typedef enum
{
    /// Critical errors that likely result in immediate termination or undefined behaviour.
    BAL_LOG_LEVEL_ERROR = 0,

    /// Non-critical issues that may result in degraded performance or functionality loss.
    BAL_LOG_LEVEL_WARN = 1,

    /// General operational events.
    BAL_LOG_LEVEL_INFO = 2,

    /// Information useful for debugging logic errors.
    BAL_LOG_LEVEL_DEBUG = 3,

    /// Extreme verbose output.
    BAL_LOG_LEVEL_TRACE = 4,
} bal_log_level_t;

/// Metadata associated with a specific log event.
typedef struct
{
    /// Source file where the log occured.
    const char *filename;

    /// The function name where the log occured.
    const char *function;

    /// The log level.
    bal_log_level_t level;

    /// The line number where the log occured
    int line;
} bal_log_data_t;

/// A function pointer defining a custom logging backend.
///
/// Implementations of this function are responsible for formatting and persisting the  log
/// message. The `user_data` pointer is passed through from the `bal_logger_t` context. The
/// `bal_data` struct contains metadata about the event, while `format`, and `args` provide the
/// standard printf-style mesaage content.
typedef void (*bal_log_function_t)(void           *user_data,
                                   bal_log_data_t *bal_data,
                                   const char     *format,
                                   va_list         args);

/// The main logging context.
typedef struct
{
    /// An opaque pointer passed to the log callback. This can be used to store file handles or
    /// other context-specific data.
    void *user_data;

    /// The callback invoked when a message needs to be logged. If this is `NULL`, logging is
    /// disabled for this context.
    bal_log_function_t log;

    /// The minimum severity level required for a message to be processed.
    bal_log_level_t min_level;
} bal_logger_t;

// Remove all log code if log level not defined.
//
#ifndef BAL_MAX_LOG_LEVEL

#define BAL_MAX_LOG_LEVEL BAL_LOG_LEVEL_ERROR

#endif

/// Dispatches a log message to the configured backend.
///
/// This is the entry point into the logging system. It invokes the callback defined in `logger`.
///
/// # Warning
///
/// Do not call this function directly. Use our logging macros like `BAL_LOG_INFO`.
///
/// # Safety
///
/// The `format` string must match the arguments provided in the variadic list, following standard
/// `printf`.
BAL_COLD void bal_log_message(bal_logger_t *logger, bal_log_data_t *data, const char *format, ...);

/// Populates `logger` with Ballistic's default logging implementation.
///
/// # Safety
///
/// `logger` must NOT be `NULL`.
BAL_COLD void bal_logger_init_default(bal_logger_t *logger);

/// Logs a message if the severity and configuration allows it.
///
/// The `logger` argument is the context handle, `log_level` is a `bal_log_level_t` value.
/// `format` and the variable arguments follow standard `printf` syntax.
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
                                        .line     = __LINE__ };              \
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
// DEBUG and TRACE macros are compiled out completely in release builds.
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
