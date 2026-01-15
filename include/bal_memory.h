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

#include "bal_attributes.h"
#include "bal_types.h"
#include "bal_errors.h"
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

/*!
 * @brief Translate a Guest Virtual Address to a Host Virtual Address.
 *
 * @details
 * Ballistic calls this when it needs to fetch instructions. The implementer
 * must return a pointer to the host memory corresponding to the requested
 * guest address.
 *
 * @param[in]  context           The oapque pointer provided in @ref
 *                               bal_memory_interface_t.
 * @param[in]  guest_address     The guest address to translate.
 * @param[out] max_readable_size The implementer MUST write the number of
 *                               contiguous bytes valid to read from the
 *                               returned pointer. This prevents Ballistic from
 *                               reading off the end of a mapped page or buffer.
 *
 * @return A pointer to the host memory containing the data at @p guest_address.
 * @return NULL if the address is unmapped or invalid.
 */
typedef const uint8_t *(*bal_translate_function_t)(
    void               *context,
    bal_guest_address_t guest_address,
    size_t             *max_readable_size);

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
    bal_free_function_t free;

} bal_allocator_t;

typedef struct
{
    void                    *context;
    bal_translate_function_t translate;
} bal_memory_interface_t;

/*!
 * @brief Populates an allocator struct with the default implementation.
 *
 * @param[out] allocator The struct to populate. Must not be NULL.
 *
 * @warn Only supports Windows and POSIX systems.
 */
BAL_COLD void bal_get_default_allocator(bal_allocator_t *out_allocator);

/*!
 * @brief Initializes a translation interface that uses a flat, contiguous
 * memory buffer.
 *
 * @params[in] allocator  The allocator used to allocate the internal interface
 *                        structure.
 * @params[out] interface The interface struct to populate.
 * @params[in]  buffer    Pointer to the pre-allocated host memory containing
 *                        the guest code.
 * @params[in]  size      The size of the buffer in bytes.
 *
 * @return BAL_SUCCESS on success.
 * @return BAL_ERROR_INVALID_ARGUMENT if arguments are NULL.
 * @return BAL_ERROR_MEMORY_ALIGNMENT if buffer is not align to 4 bytes.
 *
 * @warning The caller retains ownership of buffer. It must remain valid for
 * the lifetime of the interface.
 */
BAL_COLD bal_error_t
bal_memory_init_flat(bal_allocator_t *BAL_RESTRICT        allocator,
                     bal_memory_interface_t *BAL_RESTRICT interface,
                     void *BAL_RESTRICT                   buffer,
                     size_t                               size);

/*!
 * @brief Frees the interface's internal state.
 *
 * @details
 * This does not free the buffer passed during initialization, as Ballistic
 * does not own it.
 */
BAL_COLD void bal_memory_destroy_flat(bal_allocator_t        *allocator,
                                      bal_memory_interface_t *interface);
#endif /* BALLISTIC_MEMORY_H */

/*** end of file ***/
