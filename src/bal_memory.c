#include "bal_memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static void                   *default_allocate(bal_allocator_handle_t, size_t, size_t);
static void                    default_free(bal_allocator_handle_t, void *, size_t);
static bal_executable_buffer_t default_allocate_executable(bal_allocator_handle_t handle,
                                                           size_t                 alignment,
                                                           size_t                 size);
static void                    default_free_executable(bal_allocator_handle_t  handle,
                                                       bal_executable_buffer_t buffer,
                                                       size_t                  size);
static void                    default_protect_rw(bal_allocator_handle_t  handle,
                                                  bal_executable_buffer_t buffer,
                                                  size_t                  size);
static void                    default_protect_rx(bal_allocator_handle_t  handle,
                                                  bal_executable_buffer_t buffer,
                                                  size_t                  size);

BAL_HOT static const uint8_t *bal_flat_translation_interface_translate(void *,
                                                                       bal_guest_address_t,
                                                                       size_t *);

typedef struct
{
    uint8_t     *host;
    size_t       size;
    bal_logger_t logger;
    char         _pad[8];
} flat_translation_interface_t;

static_assert(0 == sizeof(flat_translation_interface_t) % 16, "Struct must be aligned to 16 bytes");

void
bal_allocator_default_init(bal_allocator_t *out_allocator)
{
    out_allocator->context             = NULL;
    out_allocator->allocate            = default_allocate;
    out_allocator->free                = default_free;
    out_allocator->allocate_executable = default_allocate_executable;
    out_allocator->protect_rw          = default_protect_rw;
    out_allocator->protect_rx          = default_protect_rx;
    out_allocator->free_executable     = default_free_executable;
}

BAL_COLD bal_error_t
bal_flat_translation_interface_init(bal_allocator_t *BAL_RESTRICT        allocator,
                                    bal_memory_interface_t *BAL_RESTRICT interface,
                                    void *BAL_RESTRICT                   buffer,
                                    const size_t                         size,
                                    const bal_logger_t                   logger)

{
    if (NULL == allocator || NULL == interface || NULL == buffer || 0 == size)
    {
        BAL_LOG_ERROR(&logger,
                      "Memory init failed. Invalid arguments (Allocator: %p, Interface: %p, "
                      "Buffer: %p, Size: %zu).",
                      allocator,
                      interface,
                      buffer,
                      size);

        return BAL_ERROR_INVALID_ARGUMENT;
    }

    BAL_LOG_INFO(
        &logger, "Initializing Flat Memory Model. Base: %p, Size: %zu bytes.", buffer, size);

    // ABI compliant 16-byte memory alignment.
    const size_t memory_alignment = 15U;

    if (((uintptr_t)buffer & memory_alignment) != 0)
    {
        BAL_LOG_ERROR(&logger, "Buffer %p is not 16-byte aligned.", buffer);
        return BAL_ERROR_MEMORY_ALIGNMENT;
    }

    const size_t                  memory_alignment_bytes = 16U;
    flat_translation_interface_t *flat_interface         = allocator->allocate(
        allocator->context, memory_alignment_bytes, sizeof(flat_translation_interface_t));

    if (NULL == flat_interface)
    {
        BAL_LOG_ERROR(&logger,
                      "Failed to allocate interface context (%zu bytes).",
                      sizeof(flat_translation_interface_t));
        return BAL_ERROR_ALLOCATION_FAILED;
    }

    flat_interface->host   = (uint8_t *)buffer;
    flat_interface->size   = size;
    flat_interface->logger = logger;
    interface->context     = flat_interface;
    interface->translate   = bal_flat_translation_interface_translate;

    BAL_LOG_INFO(&logger, "Flat interface created successfully at %p.", (void *)flat_interface);

    return BAL_SUCCESS;
}

bal_error_t
bal_flat_translation_interface_destroy(bal_allocator_t        *allocator,
                                       bal_memory_interface_t *interface)
{
    if (NULL == allocator || NULL == interface)
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (NULL == interface->context)
    {
        *interface = (bal_memory_interface_t) {};
        return BAL_SUCCESS;
    }

    allocator->free(allocator->context, interface->context, sizeof(flat_translation_interface_t));
    *interface = (bal_memory_interface_t) {};
    return BAL_SUCCESS;
}

#if BAL_PLATFORM_POSIX

#include <sys/mman.h>

static void *
default_allocate(bal_allocator_handle_t handle, size_t alignment, size_t size)
{
    (void)handle;

    if (0 == size)
    {
        return NULL;
    }

    void *memory = aligned_alloc(alignment, size);
    return memory;
}

static void
default_free(bal_allocator_handle_t handle, void *pointer, size_t size)
{
    (void)handle;
    (void)size;

    if (NULL == pointer)
    {
        return;
    }

    free(pointer);
}

#if BAL_PLATFORM_APPLE

#include <libkern/OSCacheControl.h>
#include <pthread.h>

static bal_executable_buffer_t
default_allocate_executable(bal_allocator_handle_t handle, size_t alignment, size_t size)
{
    (void)handle;
    (void)alignment;

    if (0 == size)
    {
        return (bal_executable_buffer_t) { NULL, NULL };
    }

    void *memory = mmap(NULL,
                        size,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT,
                        -1,
                        0);

    if (memory == MAP_FAILED)
    {
        return (bal_executable_buffer_t) { NULL, NULL };
    }

    return (bal_executable_buffer_t) { memory, memory };
}

static void
default_free_executable(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    (void)handle;

    if (NULL == buffer.rw_pointer)
    {
        return;
    }

    munmap(buffer.rw_pointer, size);
}

static void
default_protect_rw(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    (void)handle;
    (void)buffer;
    (void)size;
    pthread_jit_write_protect_np(0);
    return;
}

static void
default_protect_rx(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    (void)handle;

    if (BAL_UNLIKELY(NULL == buffer.rx_pointer))
    {
        return;
    }

    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(buffer.rx_pointer, size);
}

#endif /* BAL_PLATFORM_APPLE */

#if BAL_PLATFORM_LINUX
#define __USE_POSIX199309
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

bal_executable_buffer_t
default_allocate_executable(bal_allocator_handle_t handle, size_t alignment, size_t size)
{
    (void)handle;
    (void)alignment;

    if (0 == size)
    {
        return (bal_executable_buffer_t) { NULL, NULL };
    }

    const bal_executable_buffer_t invalid_buffer = { NULL, NULL };
    int fd = shm_open("/ballistic_jit_compiler_shm", O_RDWR | O_CREAT | O_EXCL, 0600);

    if (-1 == fd)
    {
        return invalid_buffer;
    }

    shm_unlink("/ballistic_jit_compiler_shm");

    if (size > LONG_MAX)
    {
        return invalid_buffer;
    }

    if (ftruncate(fd, (off_t)size) != 0)
    {
        close(fd);
        return invalid_buffer;
    }

    void *rw_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    void *rx_ptr = mmap(NULL, size, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0);

    if (MAP_FAILED == rw_ptr)
    {
        munmap(rw_ptr, size);
        return invalid_buffer;
    }

    if (MAP_FAILED == rx_ptr)
    {
        munmap(rw_ptr, size);
        return invalid_buffer;
    }

    return (bal_executable_buffer_t) { rw_ptr, rx_ptr };
}

void
default_free_executable(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    (void)handle;

    if (buffer.rx_pointer != NULL)
    {
        munmap(buffer.rx_pointer, size);
    }
    if (buffer.rw_pointer != NULL)
    {
        munmap(buffer.rw_pointer, size);
    }
}

void
default_protect_rw(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    // Dual mapping means rw_pointer is always writable.
    //
    (void)handle;
    (void)buffer;
    (void)size;
}

void
default_protect_rx(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    (void)handle;

    if (BAL_UNLIKELY(NULL == buffer.rx_pointer) || BAL_UNLIKELY(NULL == buffer.rw_pointer))
    {
        return;
    }

    // Flush the icache so the CPU fetches the new physical memory bytes
    __builtin___clear_cache(buffer.rw_pointer, buffer.rw_pointer + size);
}

#endif /* BAL_PLATFORM_LINUX */

#endif /* BAL_PLATFORM_POSIX */

#if BAL_PLATFORM_WINDOWS

#include <malloc.h>
#include <windows.h>

static void *
default_allocate(bal_allocator_handle_t handle, size_t alignment, size_t size)
{
    (void)handle;

    if (0 == size)
    {
        return NULL;
    }

    void *memory = _aligned_malloc(size, alignment);
    return memory;
}

static void
default_free(bal_allocator_handle_t handle, void *pointer, size_t size)
{
    (void)handle;
    (void)size;
    _aligned_free(pointer);
}

bal_executable_buffer_t
default_allocate_executable(bal_allocator_handle_t handle, size_t alignment, size_t size)
{
    (void)handle;
    (void)alignment;

    bal_executable_buffer_t invalid_buffer = { NULL, NULL };

    if (0 == size)
    {
        return invalid_buffer;
    }

    HANDLE mapping = CreateFileMapping(
        INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 0, (DWORD)size, NULL);

    if (!mapping)
    {
        return invalid_buffer;
    }

    void *rw_ptr = MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, size);
    void *rx_ptr = MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0, size);
    CloseHandle(mapping);

    if (NULL == rw_ptr)
    {
        UnmapViewOfFile(rw_ptr);
        return invalid_buffer;
    }

    if (NULL == rx_ptr)
    {
        UnmapViewOfFile(rx_ptr);
        return invalid_buffer;
    }

    return (bal_executable_buffer_t) { rw_ptr, rx_ptr };
}

void
default_protect_rw(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    // Dual mapping means rw_pointer is always writable.
    //
    (void)handle;
    (void)buffer;
    (void)size;
}

void
default_protect_rx(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    (void)handle;
    FlushInstructionCache(GetCurrentProcess(), buffer.rx_pointer, size);
}

void
default_free_executable(bal_allocator_handle_t handle, bal_executable_buffer_t buffer, size_t size)
{
    (void)handle;
    (void)size;

    if (buffer.rx_pointer != NULL)
    {
        UnmapViewOfFile(buffer.rx_pointer);
    }

    if (buffer.rw_pointer != NULL)
    {
        UnmapViewOfFile(buffer.rw_pointer);
    }
}

#endif /* BAL_PLATFORM_WINDOWS */

static const uint8_t *
bal_flat_translation_interface_translate(void *BAL_RESTRICT   interface,
                                         bal_guest_address_t  guest_address,
                                         size_t *BAL_RESTRICT max_readable_size)
{
    if (BAL_UNLIKELY(NULL == interface || NULL == max_readable_size))
    {
        return NULL;
    }

    const flat_translation_interface_t *BAL_RESTRICT context
        = ((bal_memory_interface_t *)interface)->context;

    if (NULL == context)
    {
        fprintf(stderr, "Casting memory interface returned NULL\n");
        return NULL;
    }

    // Is address out of bounds.
    //
    if (guest_address >= context->size)
    {
        BAL_LOG_ERROR(&context->logger,
                      "GVA 0x%llX Out of bounds (Limit: 0x%llX)",
                      (unsigned long long)guest_address,
                      (unsigned long long)context->size);
        return NULL;
    }

    *max_readable_size          = context->size - guest_address;
    const uint8_t *host_address = context->host + guest_address;

    BAL_LOG_TRACE(&context->logger,
                  "Translate 0x%llx -> Host %p",
                  (unsigned long long)guest_address,
                  (void *)host_address);
    return host_address;
}

/*** end of file ***/
