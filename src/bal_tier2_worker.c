#define _POSIX_C_SOURCE 199309L // includes nanosleep()

#include "bal_tier2_worker.h"
#include <stdbool.h>

#if BAL_PLATFORM_POSIX

#include <errno.h>
#include <time.h>

#endif // BAL_PLATFORM_POSIX

#if BAL_PLATFORM_WINDOWS

static DWORD WINAPI
tier2_worker_loop(LPVOID context)

#else

static void *
tier2_worker_loop(void *context)

#endif

{
    bal_tier2_worker_t *worker = context;
    BAL_LOG_INFO(&worker->logger, "Tier 2 Background thread started");

    while (true == atomic_load_explicit(&worker->is_running, memory_order_acquire))
    {
#if BAL_PLATFORM_WINDOWS

        Sleep(1);

#else

        struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
        nanosleep(&ts, NULL);

#endif
    }

    BAL_LOG_INFO(&worker->logger, "Tier 2 Background thread stopped");

#if BAL_PLATFORM_WINDOWS

    return 0;

#else

    return NULL;

#endif
}

bal_error_t
bal_tier2_worker_init(bal_tier2_worker_t           *worker,
                      const bal_allocator_t        *allocator,
                      const bal_memory_interface_t *memory_interface,
                      const bal_logger_t            logger)
{
    if (NULL == worker)
    {
        BAL_LOG_ERROR(&logger, "worker is NULL, aborting function");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (NULL == allocator)
    {
        BAL_LOG_ERROR(&logger, "allocator is NULL, aborting function");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (NULL == memory_interface)
    {
        BAL_LOG_ERROR(&logger, "memory_interface is NULL, aborting function");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    worker->allocator        = *allocator;
    worker->memory_interface = *memory_interface;
    worker->logger           = logger;
    const bal_error_t status = bal_engine_init(&worker->allocator, &worker->engine, logger);

    if (status != BAL_SUCCESS)
    {
        return status;
    }

    atomic_init(&worker->is_running, true);

#if BAL_PLATFORM_WINDOWS

    worker->thread_handle = CreateThread(NULL, 0, tier2_worker_loop, worker, 0, NULL);

    if (NULL == worker->thread_handle)
    {
        BAL_LOG_ERROR(&logger, "CreateThread failed with error code %lu", GetLastError());
        bal_engine_destroy(&worker->allocator, &worker->engine);
        return BAL_ERROR_THREAD_CREATION;
    }

#else

    int       thread_status = 0;
    const int max_retries   = 3;

    for (int attempt = 0; attempt < max_retries; ++attempt)
    {
        thread_status = pthread_create(&worker->thread_handle, NULL, tier2_worker_loop, worker);

        if (0 == thread_status)
        {
            break;
        }

        if (EAGAIN == thread_status)
        {
            BAL_LOG_WARN(&worker->logger,
                         "Resource limit hit (EAGAIN). Retrying Tier 2 thread creation (%d/%d)",
                         attempt + 1,
                         max_retries);
        }
        else
        {
            // Rest errors are fatal, no point to keep trying.
            break;
        }
    }

    if (thread_status != 0)
    {
        if (EAGAIN == thread_status)
        {
            BAL_LOG_WARN(&worker->logger,
                         "System out of thread resources (EAGAIN). Tier 2 JIT is disabled.");
        }
        else if (EINVAL == thread_status)
        {
            BAL_LOG_ERROR(&worker->logger,
                          "Invalid thread attributes (EINTVAL). Tier 2 JIT is disabled.");
        }
        else if (EPERM == thread_status)
        {
            BAL_LOG_ERROR(
                &worker->logger,
                "Permission denied setting thread scheduling (EPERM). Tier 2 JIT is disabled.");
        }
        else
        {
            BAL_LOG_ERROR(&worker->logger,
                          "Unknown pthread_create error (%d). Tier 2 is disabled.",
                          thread_status);
        }

        atomic_store_explicit(&worker->is_running, false, memory_order_release);
        bal_engine_destroy(&worker->allocator, &worker->engine);
        return BAL_ERROR_THREAD_CREATION;
    }
#endif

    return status;
}

bal_error_t
bal_tier2_worker_destroy(bal_tier2_worker_t *worker)
{
    if (NULL == worker)
    {
        BAL_LOG_ERROR(&worker->logger, "worker is NULL, aborting function");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    atomic_store_explicit(&worker->is_running, false, memory_order_release);

#if BAL_PLATFORM_WINDOWS

    DWORD wait_result = WaitForSingleObject(worker->thread_handle, INFINITE);

    if (WAIT_FAILED == wait_result)
    {
        DWORD error = GetLastError();

        if (ERROR_INVALID_HANDLE == error)
        {
            BAL_LOG_WARN(&worker->logger,
                         "WaitForSingleObject: Invalid handle, proceeding with cleanup");
        }
        else
        {
            BAL_LOG_ERROR(
                &worker->logger, "WaitForSingleObject failed (%lu), aborting cleanup", error);
            return BAL_ERROR_THREAD_CLEANUP;
        }
    }

    CloseHandle(worker->thread_handle);

#else
    const int join_result = pthread_join(worker->thread_handle, NULL);

    if (join_result != 0)
    {
        switch (join_result)
        {
            case ESRCH:
                BAL_LOG_WARN(&worker->logger,
                             "Thread not found (ESRCH). It may have already exited, proceeding "
                             "with cleanup");
                break;
            case EINVAL:
                BAL_LOG_ERROR(&worker->logger,
                              "Thread not joinable (EINVAL). Proceeding with cleanup");
                break;
            case EDEADLK:
                BAL_LOG_ERROR(
                    &worker->logger,
                    "Deadlock detected (EDEADLK). Aborting memory cleanup to prevent crash");
                return BAL_ERROR_THREAD_CLEANUP;
            default:
                BAL_LOG_ERROR(
                    &worker->logger, "Unknown error code %d, proceeding with cleanup", join_result);
                break;
        }
    }

#endif

    bal_engine_destroy(&worker->allocator, &worker->engine);
    return BAL_SUCCESS;
}

/*** end of file ***/
