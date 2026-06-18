#include "bal_spsc_queue.h"
#include <stddef.h>

void
bal_spsc_queue_init(bal_spsc_queue_t *queue)
{
    if (BAL_UNLIKELY(NULL == queue))
    {
        return;
    }

    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
    bal_guest_address_t *address_cursor = queue->buffer;

    for (size_t i = 0; i < BAL_SPSC_QUEUE_CAPACITY; ++i)
    {
        *address_cursor++ = 0;
    }
}

bool
bal_spsc_queue_push(bal_spsc_queue_t *queue, const bal_guest_address_t address)
{
    if (BAL_UNLIKELY(NULL == queue))
    {
        return false;
    }

    const size_t current_head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    const size_t next_head    = current_head + 1 & BAL_SPSC_QUEUE_MASK;
    const size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_acquire);

    if (next_head == current_tail)
    {
        // Queue is full. Drop the request.
        return false;
    }

    queue->buffer[current_head] = address;
    atomic_store_explicit(&queue->head, next_head, memory_order_release);
    return true;
}

bool
bal_spsc_queue_pop(bal_spsc_queue_t *queue, bal_guest_address_t *out_address)
{
    if (BAL_UNLIKELY(NULL == queue || NULL == out_address))
    {
        return false;
    }

    const size_t current_tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    const size_t current_head = atomic_load_explicit(&queue->head, memory_order_acquire);

    if (current_head == current_tail)
    {
        // Queue is empty.
        return false;
    }

    *out_address           = queue->buffer[current_head];
    const size_t next_tail = current_tail + 1 & BAL_SPSC_QUEUE_MASK;
    atomic_store_explicit(&queue->tail, next_tail, memory_order_release);
    return true;
}