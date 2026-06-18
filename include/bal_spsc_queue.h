#ifndef BALLISTIC_BAL_SPSC_QUEUE_H
#define BALLISTIC_BAL_SPSC_QUEUE_H

#include "bal_attributes.h"
#include "bal_types.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>

#define BAL_SPSC_QUEUE_CAPACITY 256
#define BAL_SPSC_QUEUE_MASK     (BAL_SPSC_QUEUE_CAPACITY - 1)

static_assert(!(BAL_SPSC_QUEUE_CAPACITY & BAL_SPSC_QUEUE_CAPACITY - 1),
              "Must be a power of 2 for fast bit arithmatic");

/// Lock free Single Producer, Single Consumer queue.
typedef struct
{
    /// Written by main thread, read by background thread.
    BAL_ALIGNED(64) atomic_size_t head;

    /// Written by background thread, read by main thread.
    BAL_ALIGNED(64) atomic_size_t tail;

    /// Ring buffer holding the guest addresses.
    bal_guest_address_t buffer[BAL_SPSC_QUEUE_CAPACITY];
} bal_spsc_queue_t;

/// Initializes the SPSC queue.
void bal_spsc_queue_init(bal_spsc_queue_t *queue);

/// Pushes `address` into the queue.
///
/// # Safety
///
/// This function must ONLY be called by the main thread.
///
/// # Errors
///
/// Returns `true` on success.
///
/// Returns `false` if `queue` is NULL.
///
/// Returns `false` the queue is full.
BAL_HOT bool bal_spsc_queue_push(bal_spsc_queue_t *queue, bal_guest_address_t address);

/// Pops a guest virtual address from `queue` into `out_address`.
///
/// # Safety
///
/// This function must ONLY be called by the background thread.
///
/// # Errors
///
/// Returns `true` on success.
///
/// Returns `false` if any function argument is NULL.
///
/// Returns `false` if `queue` is full.
BAL_HOT bool bal_spsc_queue_pop(bal_spsc_queue_t *queue, bal_guest_address_t *out_address);

#endif // BALLISTIC_BAL_SPSC_QUEUE_H

/*** end of file ***/
