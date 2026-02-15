#include "bal_memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

static void                  *default_allocate(bal_allocator_handle_t, size_t, size_t);
static void                   default_free(bal_allocator_handle_t, void *, size_t);
BAL_HOT static const uint8_t *bal_translate_flat(void *, bal_guest_address_t, size_t *);

typedef struct
{
    uint8_t     *host_base;
    size_t       size;
    bal_logger_t logger;
    char         _pad[8];
} flat_translation_interface_t;

static_assert(0 == sizeof(flat_translation_interface_t) % 16, "Struct must be aligned to 16 bytes");

void
bal_get_default_allocator(bal_allocator_t *out_allocator)
{
    out_allocator->handle   = NULL;
    out_allocator->allocate = default_allocate;
    out_allocator->free     = default_free;
}

BAL_COLD bal_error_t
bal_memory_init_flat(bal_allocator_t *BAL_RESTRICT        allocator,
                     bal_memory_interface_t *BAL_RESTRICT interface,
                     void *BAL_RESTRICT                   buffer,
                     size_t                               size,
                     bal_logger_t                         logger)

{
    if (NULL == allocator || NULL == interface || NULL == buffer || 0 == size)
    {
        BAL_LOG_ERROR(&logger,
                      "Memory init failed. Invalid arguments (Buffer: %p, Size: %zu).",
                      buffer,
                      size);

        return BAL_ERROR_INVALID_ARGUMENT;
    }

    BAL_LOG_INFO(
        &logger, "Initializing Flat Memory Model. Base: %p, Size: %zu bytes.", buffer, size);

    // ABI compliant 16-byte memory alignment.
    size_t memory_alignment = 15U;

    if (((uintptr_t)buffer & memory_alignment) != 0)
    {
        BAL_LOG_ERROR(&logger, "Buffer %p is not 16-byte aligned.", buffer);
        return BAL_ERROR_MEMORY_ALIGNMENT;
    }

    size_t                        memory_alignment_bytes = 16U;
    flat_translation_interface_t *flat_interface
        = (flat_translation_interface_t *)allocator->allocate(
            allocator->handle, memory_alignment_bytes, sizeof(flat_translation_interface_t));

    if (NULL == flat_interface)
    {
        BAL_LOG_ERROR(&logger,
                      "Failed to allocate interface context (%zu bytes).",
                      sizeof(flat_translation_interface_t));
        return BAL_ERROR_ALLOCATION_FAILED;
    }

    flat_interface->host_base = (uint8_t *)buffer;
    flat_interface->size      = size;
    flat_interface->logger    = logger;
    interface->context        = flat_interface;
    interface->translate      = bal_translate_flat;

    BAL_LOG_DEBUG(&logger, "Flat interface created successfully at %p.", (void *)flat_interface);

    return BAL_SUCCESS;
}

void
bal_memory_destroy_flat(bal_allocator_t *allocator, bal_memory_interface_t *interface)
{
    if (NULL == allocator || NULL == interface)
    {
        return;
    }

    if (NULL == interface->context)
    {
        return;
    }

    allocator->free(allocator->handle, interface->context, sizeof(flat_translation_interface_t));
}

#if BAL_PLATFORM_POSIX

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
    free(pointer);
}

#endif /* BAL_PLATFORM_POSIX */

#if BAL_PLATFORM_WINDOWS

#include <malloc.h>

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

#endif /* BAL_PLATFORM_WINDOWS */

static const uint8_t *
bal_translate_flat(void *BAL_RESTRICT   interface,
                   bal_guest_address_t  guest_address,
                   size_t *BAL_RESTRICT max_readable_size)
{
    if (BAL_UNLIKELY(NULL == interface || 0 == guest_address || NULL == max_readable_size))
    {
        return NULL;
    }

    flat_translation_interface_t *BAL_RESTRICT context
        = (flat_translation_interface_t *)((bal_memory_interface_t *)interface)->context;

    // Is address out of bounds.
    //
    if (guest_address >= context->size)
    {
        BAL_LOG_ERROR(&context->logger,
                      "GVA 0x%llx Out of bounds (Limit: 0x%zx)",
                      (unsigned long long)guest_address,
                      context->size);
        return NULL;
    }

    *max_readable_size          = context->size - guest_address;
    const uint8_t *host_address = context->host_base + guest_address;

    BAL_LOG_TRACE(&context->logger,
                  "Translate 0x%llx -> Host %p",
                  (unsigned long long)guest_address,
                  (void *)host_address);
    return host_address;
}

/*** end of file ***/
