//! This signal handler relies entirely on the RBP register. If the fault did not originate in
//! JIT buffer, the handler resets the OS signal actin to SIG_DFL and returns. The host OS should
//! then re-trigger the fault and generate a core dump.

#include "backend/bal_cpu.h"
#include "bal_jit_debug.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if BAL_PLATFORM_LINUX

#define __USE_POSIX199309
#define __USE_POSIX
#include <signal.h>
#include <ucontext.h>

#elif BAL_PLATFORM_WINDOWS

#include <windows.h>

#else

#error "Ballistic does not support SIGSEGV handling for this platform"

#endif // BAL_PLATFORM_POSIX

#ifndef BALLISTIC_BUILD_TESTS

static bool handle_jit_fault(uint64_t rip, uint64_t rbp);

#endif // BALLISTIC_BUILD_TESTS

#if BAL_PLATFORM_WINDOWS

static LONG WINAPI segv_handler(EXCEPTION_POINTERS *info);

#else

static void segv_handler(int sig, siginfo_t *info, void *ucontext);

#endif // BAL_PLATFORM_WINDOWS

static bool extract_fault_context(void *os_context, uint64_t *out_rip, uint64_t *out_rbp);

bal_error_t
bal_jit_debug_register_signal_handler(bal_jit_debug_context_t *BAL_RESTRICT context,
                                      void *BAL_RESTRICT                    jit_buffer_start,
                                      const size_t                          jit_buffer_size)
{
    const bal_error_t invalid_argument = BAL_ERROR_INVALID_ARGUMENT;

    if (NULL == context)
    {
        return invalid_argument;
    }

    if (NULL == jit_buffer_start)
    {
        BAL_LOG_ERROR(&context->logger, "Aborting function: jit_buffer_start is NULL.");
        return invalid_argument;
    }

    if (0 == jit_buffer_size)
    {
        BAL_LOG_ERROR(&context->logger, "Aborting function: jit_buffer_size == 0.");
        return invalid_argument;
    }

    context->jit_buffer_start = jit_buffer_start;
    context->jit_buffer_end   = (void *)((uintptr_t)jit_buffer_start + jit_buffer_size);

#if BAL_PLATFORM_LINUX

    struct sigaction sa;
    sa.sa_sigaction = segv_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGSEGV, &sa, NULL) != 0)
    {
        BAL_LOG_ERROR(&context->logger,
                      "Aborting function: failed to register SIGSEGV signal handler");
        return BAL_ERROR_ALLOCATION_FAILED;
    }

    if (sigaction(SIGILL, &sa, NULL) != 0)
    {
        BAL_LOG_ERROR(&context->logger,
                      "Aborting function: failed to register SIGKILL signal handler");
        return BAL_ERROR_ALLOCATION_FAILED;
    }

#elif BAL_PLATFORM_WINDOWS

    if (NULL == AddVectoredExceptionHandler(1, segv_handler))
    {
        BAL_LOG_ERROR(&context->logger, "Aborting function: AddVectoredExceptionHandler() failed.");
        return BAL_ERROR_ALLOCATION_FAILED;
    }

#else

#error "Ballistic does not support SIGSEGV handling for this platform"

#endif

    return BAL_SUCCESS;
}

void
bal_jit_debug_unregister_signal_handler(bal_jit_debug_context_t *BAL_RESTRICT context)
{
    if (NULL == context)
    {
        return;
    }

#if BAL_PLATFORM_LINUX

    struct sigaction sa = { 0 };
    sa.sa_handler       = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);

#endif // BAL_PLATFORM_LINUX

    context->jit_buffer_start = NULL;
    context->jit_buffer_end   = NULL;
}

bool
handle_jit_fault(const uint64_t rip, const uint64_t rbp)
{
    const bal_cpu_t *BAL_RESTRICT cpu = (bal_cpu_t *)rbp;

    if (NULL == cpu || NULL == cpu->debug_context)
    {
        return false;
    }

    const bal_jit_debug_context_t *BAL_RESTRICT context = cpu->debug_context;

    if (NULL == context)
    {
        return false;
    }

    if (NULL == context->entries)
    {
        BAL_LOG_ERROR(&context->logger, "Aborting function: context->entries is NULL.");
        return false;
    }

    const uintptr_t buffer_start = (uintptr_t)context->jit_buffer_start;

    for (size_t i = 0; i < context->entry_count; ++i)
    {
        const uintptr_t start = (uintptr_t)context->entries[i].rx_start;
        const uintptr_t end   = start + context->entries[i].rx_size;

        if (rip >= start && rip < end)
        {
            const uint32_t buffer_offset                          = (uint32_t)(rip - buffer_start);
            const uint32_t block_offset                           = (uint32_t)(rip - start);
            const bal_jit_block_metadata_t *BAL_RESTRICT metadata = context->entries[i].metadata;
            uint64_t                                     guest_pc = metadata->base_guest_pc;

            for (uint32_t ii = 0; ii < metadata->instruction_count; ++ii)
            {
                if (metadata->mappings[ii].x86_offset > block_offset)
                {
                    break;
                }

                guest_pc = metadata->base_guest_pc + metadata->mappings[ii].guest_pc_offset;
            }

            if (NULL == context->crash_callback)
            {
                BAL_LOG_ERROR(
                    &context->logger,
                    "JIT CRASH! Host RIP: 0x%llX | JIT Buffer Offset: 0x%X | Guest PC: 0x%llX",
                    (unsigned long long)rip,
                    buffer_offset,
                    (unsigned long long)guest_pc);
            }
            else
            {
                context->crash_callback(context->crash_callback_user_data,
                                        guest_pc,
                                        rip,
                                        block_offset,
                                        context->entries[i].rx_start,
                                        context->entries[i].rx_size);
            }

            abort();
        }
    }

    if (rip >= (uintptr_t)context->jit_buffer_start && rip < (uintptr_t)context->jit_buffer_end)
    {
        BAL_LOG_ERROR(&context->logger,
                      "JIT CRASH in UNKNOWN block! Host RIP: 0x%llX (Buffer: %p - %p)",
                      (unsigned long long)rip,
                      context->jit_buffer_start,
                      context->jit_buffer_end);
        abort();
    }

    return false;
}

static bool
extract_fault_context(void *os_context, uint64_t *out_rip, uint64_t *out_rbp)
{
#if BAL_PLATFORM_LINUX
    const ucontext_t *BAL_RESTRICT uc = (ucontext_t *)os_context;

    if (NULL == uc)
    {
        return false;
    }

    *out_rip = (uint64_t)uc->uc_mcontext.__gregs[16];
    *out_rbp = (uint64_t)uc->uc_mcontext.__gregs[10];

    return true;

#elif BAL_PLATFORM_WINDOWS

    EXCEPTION_POINTERS *BAL_RESTRICT ep = (EXCEPTION_POINTERS *)os_context;

    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        || ep->ExceptionRecord->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION)
    {
        *out_rip = (uint64_t)ep->ContextRecord->Rip;
        *out_rbp = (uint64_t)ep->ContextRecord->Rbp;
        return true;
    }

    return false;

#else

    (void)os_context;
    (void)out_rip;
    (void)out_rbp;
    return false;

#endif // BAL_PLATFORM_LINUX
}

#if BAL_PLATFORM_LINUX

static void
segv_handler(const int sig, siginfo_t *info, void *ucontext)
{
    (void)sig;
    (void)info;
    uint64_t rip = 0;
    uint64_t rbp = 0;

    const bool extract_status = extract_fault_context(ucontext, &rip, &rbp);
    const bool handler_status = handle_jit_fault(rip, rbp);

    if (true == extract_status && true == handler_status)
    {
        return;
    }

    // Not a JIT fault. Let the OS re-trigger the fault.
    struct sigaction sa = { 0 };
    sa.sa_handler       = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
}

#elif BAL_PLATFORM_WINDOWS

static LONG WINAPI
segv_handler(EXCEPTION_POINTERS *info)
{

    uint64_t rip = 0;
    uint64_t rbp = 0;

    const bool extract_status = extract_fault_context(info, &rip, &rbp);
    const bool handler_status = handle_jit_fault(rip, rbp);

    if (true == extract_status && true == handler_status)
    {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

#else

#error "Ballistic does not support SIGSEGV handling for this platform"

#endif // BAL_PLATFORM_LINUX

/*** end of file ***/
