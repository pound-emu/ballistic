#include "bal_jit_debug.h"
#include "bal_safety.h"
#include <string.h>

bal_error_t
bal_jit_debug_init(const bal_allocator_t *BAL_RESTRICT   allocator,
                   bal_jit_debug_context_t *BAL_RESTRICT context)
{
    if (BAL_UNLIKELY(NULL == context))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: context is NULL");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(NULL == allocator))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: allocator is NULL.");
        context->status = BAL_ERROR_INVALID_ARGUMENT;
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    bal_jit_debug_context_t c       = { 0 };
    c.entry_capacity                = BAL_JIT_DEBUG_ENTRY_CAPACITY;
    const size_t total_entries_size = c.entry_capacity * sizeof(bal_jit_block_entry_t);
    const size_t memory_alignment   = 64U;
    void *entries = allocator->allocate(allocator->context, memory_alignment, total_entries_size);
    (void)memcpy(&c.entries, &entries, sizeof(entries));

    if (BAL_UNLIKELY(NULL == c.entries))
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Aborting function: failed to allocate JIT debug block entries");
        context->status = BAL_ERROR_ALLOCATION_FAILED;
        return BAL_ERROR_ALLOCATION_FAILED;
    }

    c.arena_capacity = BAL_JIT_DEBUG_ARENA_CAPACITY_BYTES;
    void *arena      = allocator->allocate(allocator->context, memory_alignment, c.arena_capacity);
    (void)memcpy(&c.metadata_arena, &arena, sizeof(arena));

    if (BAL_UNLIKELY(NULL == c.metadata_arena))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Failed to allocate JIT debug metadata arena.");
        allocator->free(allocator->context, c.entries, total_entries_size);
        context->status = BAL_ERROR_ALLOCATION_FAILED;
        return BAL_ERROR_ALLOCATION_FAILED;
    }

    (void)memset(context, 0, sizeof(bal_jit_debug_context_t));
    context->entries        = c.entries;
    context->metadata_arena = c.metadata_arena;
    context->entry_capacity = c.entry_capacity;
    context->arena_capacity = c.arena_capacity;
    context->magic          = BAL_JIT_DEBUG_MAGIC_ALIVE;

    BAL_LOG_INFO(&bal_thread_logger,
                 "JIT Debug Context initialized. Entries Capacity: %zu, Arena: %zu bytes.",
                 context->entry_capacity,
                 context->arena_capacity);
    return BAL_SUCCESS;
}

void
bal_jit_debug_destroy(const bal_allocator_t *BAL_RESTRICT   allocator,
                      bal_jit_debug_context_t *BAL_RESTRICT context)
{
    if (BAL_UNLIKELY(NULL == context))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == allocator))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: allocator is NULL.");
        return;
    }

    if (context->entries != NULL)
    {
        allocator->free(allocator->context,
                        context->entries,
                        context->entry_capacity * sizeof(bal_jit_block_entry_t));
        context->entries = NULL;
    }

    if (context->metadata_arena != NULL)
    {
        allocator->free(allocator->context, context->metadata_arena, context->arena_capacity);
        context->metadata_arena = NULL;
    }

    (void)memset(context, 0, sizeof(bal_jit_debug_context_t));
    context->magic = BAL_JIT_DEBUG_MAGIC_DEAD;
}

bal_error_t
bal_jit_debug_add_block(bal_jit_debug_context_t *BAL_RESTRICT         context,
                        void *BAL_RESTRICT                            rx_start,
                        const uint32_t                                rx_size,
                        const uint64_t                                base_guest_pc,
                        const bal_jit_instruction_map_t *BAL_RESTRICT mapping,
                        const uint32_t                                instruction_count)
{
    const bal_error_t invalid_argument = BAL_ERROR_INVALID_ARGUMENT;

    if (BAL_UNLIKELY(NULL == context))
    {
        return invalid_argument;
    }

    if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: context->status != BAL_SUCCESS");
        return context->status;
    }

    context->status = invalid_argument;

    if (BAL_UNLIKELY(NULL == rx_start))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: rx_start is NULL.");
        return invalid_argument;
    }

    if (BAL_UNLIKELY(NULL == mapping))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: mapping is NULL.");
        return invalid_argument;
    }

    if (BAL_UNLIKELY(0 == instruction_count))
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting function: instruction_count == 0");
        return invalid_argument;
    }

    if (BAL_UNLIKELY(context->entry_count >= context->entry_capacity))
    {
        BAL_LOG_WARN(&bal_thread_logger,
                     "Aborting function: JIT debug entries full, block tracking skipped.");
        context->status = BAL_ERROR_BUFFER_OVERFLOW;
        return BAL_ERROR_BUFFER_OVERFLOW;
    }

    BAL_CHECK_MAGIC(
        context, BAL_JIT_DEBUG_MAGIC_ALIVE, BAL_JIT_DEBUG_MAGIC_DEAD, "bal_jit_debug_context_t");

    const size_t total_mapping_size    = instruction_count * sizeof(bal_jit_instruction_map_t);
    const size_t total_memory_required = total_mapping_size + sizeof(bal_jit_block_metadata_t);

    if (BAL_UNLIKELY(context->arena_offset + total_memory_required > context->arena_capacity))
    {
        BAL_LOG_WARN(&bal_thread_logger,
                     "Aborting function: JIT debug arena full, block tracking skipped.");
        context->status = BAL_ERROR_BUFFER_OVERFLOW;
        return BAL_ERROR_BUFFER_OVERFLOW;
    }

    uint8_t *arena_cursor     = &context->metadata_arena[context->arena_offset];
    void    *metadata_address = arena_cursor;
    void    *mapping_address  = &arena_cursor[sizeof(bal_jit_block_metadata_t)];

    bal_jit_block_metadata_t metadata_local = {};
    metadata_local.base_guest_pc            = base_guest_pc;
    metadata_local.instruction_count        = instruction_count;

    (void)memcpy(&metadata_local.mappings, &mapping_address, sizeof(mapping_address));
    (void)memcpy(arena_cursor, &metadata_local, sizeof(metadata_local));
    (void)memcpy(mapping_address, mapping, total_mapping_size);
    context->arena_offset += total_memory_required;

    bal_jit_block_entry_t *BAL_RESTRICT entry = &context->entries[context->entry_count];
    entry->rx_start                           = rx_start;
    entry->rx_size                            = rx_size;
    (void)memcpy(&entry->metadata, &metadata_address, sizeof(metadata_address));
    context->entry_count++;
    context->status = BAL_SUCCESS;
    return BAL_SUCCESS;
}