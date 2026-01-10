/** @file bal_memory.h
 *
 * @brief Memory management interface for Ballistic.
 *
 * @par
 * The host is responsible for providing an allocator capable of handling
 * aligned memory requests.
 */

#ifndef BALLISTIC_MEMORY_H
#define BALLISTIC_MEMORY_H

#include <stdint.h>
#include <stddef.h>

/*!
 * @brief Function signature for memory allocation.
 *
 * @param[in] allocator The opaque pointer registered in @ref bal_allocator_t.
 * @param[in] alignment The required alignment in bytes. Must be power of 2.
 * @param[in] size      The number of bytes to allocate.
 *
 * @return A pointer to the allocated memory, or NULL on failure.
 */
typedef void *(*bal_allocate_function_t)(void  *allocator,
                                         size_t alignment,
                                         size_t size);

/*!
 * @brief Function signature for memory deallocation.
 *
 * @param[in] allocator The opaque pointer registered in @ref al_allocator_t.
 * @param[in] pointer   The pointer to the memory to free.
 * @param[in] size      The size of the allocation being freed.
 */
typedef void (*bal_free_function_t)(void  *allocator,
                                    void  *pointer,
                                    size_t size);

typedef struct
{
    /*!
     * @brief Use this to store heap state or tracking information.
     */
    void *allocator;

    /*!
     * @brief Callback for allocating aligned memory.
     * @warning Must return 64-byte aligned memory if requested.
     */
    bal_allocate_function_t allocate;

    /*!
     * @brief Callback for freeing memory.
     */
    bal_free_function_t frre;

} bal_allocator_t;

#endif /* BALLISTIC_MEMORY_H */

/*** end of file ***/
