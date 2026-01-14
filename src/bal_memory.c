#include "bal_memory.h"
#include "bal_platform.h"
#include <stdlib.h>
#include <stddef.h>

void *default_allocate(void *, size_t, size_t);
void  default_free(void *, void *, size_t);

void
get_default_allocator (bal_allocator_t *out_allocator)
{
    out_allocator->allocator = NULL;
    out_allocator->allocate  = default_allocate;
    out_allocator->free      = default_free;
}

#if BAL_PLATFORM_POSIX

void *
default_allocate (void *allocator, size_t alignment, size_t size)
{
    if (0 == size)
    {
        return NULL;
    }

    void *memory = aligned_alloc(alignment, size);

    return memory;
}

void
default_free (void *allocator, void *pointer, size_t size)
{
    free(pointer);
}

#endif /* BAL_PLATFORM_POSIX */

#if BAL_PLATFORM_WINDOWS

#include <malloc.h>

void *
default_allocate (void *allocator, size_t alignment, size_t size)
{
    if (0 == size)
    {
        return NULL;
    }

    void *memory = _aligned_malloc(alignment, size);

    return memory;
}

void
default_free (void *allocator, void *pointer, size_t size)
{
    _aligned_free(pointer);
} 

#endif /* BAL_PLATFORM_WINDOWS */

/*** end of file ***/
