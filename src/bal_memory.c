#include "bal_memory.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

static void                  *default_allocate(void *, size_t, size_t);
static void                   default_free(void *, void *, size_t);
BAL_HOT static const uint8_t *bal_translate_flat(void *, bal_guest_address_t, size_t *);

typedef struct
{
    uint8_t *host_base;
    size_t   size;
} flat_translation_interface_t;

static_assert(0 == sizeof(flat_translation_interface_t) % 16, "Struct must be aligned to 16 bytes");

void
bal_get_default_allocator(bal_allocator_t *out_allocator)
{
    out_allocator->allocator = NULL;
    out_allocator->allocate  = default_allocate;
    out_allocator->free      = default_free;
}

bal_error_t
bal_memory_init_flat(bal_allocator_t *BAL_RESTRICT        allocator,
                     bal_memory_interface_t *BAL_RESTRICT interface,
                     void *BAL_RESTRICT                   buffer,
                     size_t                               size)
{
    if (NULL == allocator || NULL == interface || NULL == buffer || 0 == size)
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    // ABI compliant 16-byte  memory alignment.
    size_t memory_alignment_bytes = 16U;

    if (((uintptr_t)buffer & memory_alignment_bytes) != 0)
    {
        return BAL_ERROR_MEMORY_ALIGNMENT;
    }

    flat_translation_interface_t *flat_interface
        = (flat_translation_interface_t *)allocator->allocate(
            allocator, memory_alignment_bytes, sizeof(flat_translation_interface_t));

    if (NULL == flat_interface)
    {
        return BAL_ERROR_ALLOCATION_FAILED;
    }

    flat_interface->host_base = (uint8_t *)buffer;
    flat_interface->size      = size;
    interface->context        = flat_interface;
    interface->translate      = bal_translate_flat;
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

    allocator->free(allocator, interface->context, sizeof(flat_translation_interface_t));
}

#if BAL_PLATFORM_POSIX

static void *
default_allocate(void *allocator, size_t alignment, size_t size)
{
    if (0 == size)
    {
        return NULL;
    }

    void *memory = aligned_alloc(alignment, size);
    return memory;
}

static void
default_free(void *allocator, void *pointer, size_t size)
{
    free(pointer);
}

#endif /* BAL_PLATFORM_POSIX */

#if BAL_PLATFORM_WINDOWS

#include <malloc.h>

static void *
default_allocate(void *allocator, size_t alignment, size_t size)
{
    if (0 == size)
    {
        return NULL;
    }

    void *memory = _aligned_malloc(size, alignment);
    return memory;
}

static void
default_free(void *allocator, void *pointer, size_t size)
{
    _aligned_free(pointer);
}

#endif /* BAL_PLATFORM_WINDOWS */

static const uint8_t *
bal_translate_flat(void *BAL_RESTRICT    interface,
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
        return NULL;
    }

    *max_readable_size          = context->size - guest_address;
    const uint8_t *host_address = context->host_base + guest_address;
    return host_address;
}

/*** end of file ***/
