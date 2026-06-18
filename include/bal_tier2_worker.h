#ifndef BALLISTIC_TIER2_WORKER_H
#define BALLISTIC_TIER2_WORKER_H

#include "bal_attributes.h"
#include "bal_engine.h"
#include "bal_memory.h"
#include "bal_platform.h"
#include <stdatomic.h>

#if BAL_PLATFORM_WINDOWS

#include <windows.h>
typedef HANDLE bal_thread_t;

#elif BAL_PLATFORM_POSIX

#include <pthread.h>
typedef pthread_t bal_thread_t;

#else

#error "Unsupported platform for threading"

#endif

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    /// Context for Tier 2 background compilation thread.
    BAL_ALIGNED(64) typedef struct
    {
        /// The JIT engine for Tier 2 compilation.
        bal_engine_t engine;

        /// The private allocator instance for the background thread.
        bal_allocator_t allocator;

        /// The logger instance.
        bal_logger_t logger;

        /// The guest memory interface.
        bal_memory_interface_t memory_interface;

        /// The thread handle.
        bal_thread_t thread_handle;
        /// Used to terminate the thread.
        atomic_bool is_running;
    } bal_tier2_worker_t;

#ifdef __cplusplus
}
#endif // __cplusplus

/// Initializes the Tier 2 background worker and spawns the OS thread.
///
/// # Safety
///
/// Returns [`BAL_SUCCESS`] on success.
///
/// Returns [`BAL_ERROR_THREAD_CREATION`] if thread creation failed.
///
/// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if the function arguments are NULL.
///
/// See [`bal_engine_init`] for more errors that occur.
bal_error_t bal_tier2_worker_init(bal_tier2_worker_t           *worker,
                                  const bal_allocator_t        *allocator,
                                  const bal_memory_interface_t *memory_interface,
                                  bal_logger_t                  logger);

/// Signals the background thread to stop and waits for it to exit.
///
/// # Safety
///
/// Returns [`BAL_SUCCESS`] on success.
///
/// Returns [`BAL_ERROR_INVALID_ARGUMENT`] if `worker` is NULL.
/// Returns [`BAL_ERROR_THREAD_CLEANUP`] if thread cleanup failed.
bal_error_t bal_tier2_worker_destroy(bal_tier2_worker_t *worker);

#endif // BALLISTIC_TIER2_WORKER_H

/*** end of file ***/
