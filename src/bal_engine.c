#include "bal_engine.h"
#include "backend/x86/bal_x86_tier1_compiler.h"
#include "bal_decoder.h"
#include "bal_engine_flags.h"
#include "bal_logging.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
#include <atomic>
typedef std::atomic<bool>   atomic_bool;
typedef std::atomic<size_t> atomic_size_t;

using std::atomic_init;
using std::atomic_load_explicit;
using std::atomic_store_explicit;
using std::atomic_compare_exchange_strong_explicit;

#if __cplusplus >= 202002L

#define memory_order_relaxed std::memory_order::relaxed
#define memory_order_consume std::memory_order::consume
#define memory_order_acquire std::memory_order::acquire
#define memory_order_release std::memory_order::release
#define memory_order_acq_rel std::memory_order::acq_rel
#define memory_order_seq_cst std::memory_order::seq_cst

#else

using std::memory_order_relaxed;
using std::memory_order_consume;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

#endif

#else

#include <stdatomic.h>

#endif // __cplusplus

#define MAX_INSTRUCTIONS 65535

// Not sure what exact value to put here.
//
#define MAX_GUEST_REGISTERS 128

/// The size of an ARM Instruction in bytes.
#define ARM_INSTRUCTION_SIZE_BYTES 4

/// Helper macro to align `x` UP to the nearest memory alignment.
#define BAL_ALIGN_UP(x, memory_alignment) \
    (((x) + ((memory_alignment) - 1)) & ~((memory_alignment) - 1))

#define MEMORY_ALIGNMENT        64U
#define TIER1_BUFFER_SIZE_BYTES (1024 * 1024 * 16) // 16 MiB

#define BLOCK_CACHE_SETS 4096 // 256 KB total footprint.
#define BLOCK_CACHE_WAYS 4
#define BLOCK_CACHE_MASK (BLOCK_CACHE_SETS - 1)

/// Represents a single entry (way) in the block cache.
typedef struct
{
    /// The GVA of the compiled block.
    bal_guest_address_t guest_address;

    /// Pointer to the JIT compiled host machine code. A NULL value indicates an empty/invalid
    /// cache slot.
    void *host_code;
} block_cache_entry_t;

static_assert(16 == sizeof(block_cache_entry_t), "Block cache entry size mismatch");

/// Represents a single set in the 4-way associative block cache.
typedef struct
{
    block_cache_entry_t ways[BLOCK_CACHE_WAYS];
} block_cache_set_t;

static_assert(64 == sizeof(block_cache_set_t), "Block cache set size mismatch");

typedef struct
{
    bal_tier1_compiler_t    tier1_compiler;
    bal_executable_buffer_t tier1_buffer;
    size_t                  tier1_buffer_size;
    char                    pad0[40];

    BAL_ALIGNED(64) struct
    {
        atomic_bool is_executing;
        atomic_bool stop_requested;
        atomic_bool clear_cache_requested;
        char        pad1[61];
    } thread_state;

    BAL_ALIGNED(64) block_cache_set_t block_cache[BLOCK_CACHE_SETS];
} internal_engine_state_t;

/// Looks up a JIT compiled host block by its GVA. Returns a pointer to the host JIT code if found,
/// NULL on a cache miss.
static void *block_cache_lookup(const block_cache_set_t *BAL_RESTRICT cache,
                                const bal_guest_address_t             pc);

/// Inserts a newly compiled JIT block into cache.
static void block_cache_insert(block_cache_set_t *BAL_RESTRICT cache,
                               const bal_guest_address_t       pc,
                               void                           *host_code);

BAL_COLD bal_error_t
bal_engine_init(bal_engine_t *BAL_RESTRICT                 engine,
                bal_cpu_t *BAL_RESTRICT                    cpu,
                const bal_allocator_t *BAL_RESTRICT        allocator,
                const bal_memory_interface_t *BAL_RESTRICT memory_interface,
                const bal_logger_t                         logger)
{
    // Check every pointer that will be used in the Engine.
    // This is tedious, but I currently don't give a fuck. Telling the user exactly what's
    // wrong is way better than seeing a random segfault in your terminal then going on Github
    // asking me why Ballistic is crashing on your system.

    if (BAL_UNLIKELY(NULL == engine))
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(NULL == cpu))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: CPU is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == allocator))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Allocator is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == allocator->allocate))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Allocator->Allocate() is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == allocator->free))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Allocator->Free() is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == allocator->allocate_executable))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Allocator->Allocate_Executable() is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == allocator->protect_rw))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Allocator->Protect_RW() is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == allocator->protect_rx))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Allocator->Protect_RX() is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == allocator->free_executable))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Allocator->Free_Executable function is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == memory_interface))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Memory_Interface is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == memory_interface->context))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Memory_Interface->Context is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == memory_interface->translate))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Memory_Interface->Translate function is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    internal_engine_state_t *BAL_RESTRICT internal_engine_state
        = (internal_engine_state_t *)allocator->allocate(
            allocator->context, MEMORY_ALIGNMENT, sizeof(internal_engine_state_t));

    if (BAL_UNLIKELY(NULL == internal_engine_state))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Failed to allocate Internal Engine State");
        engine->status = BAL_ERROR_ALLOCATION_FAILED;
        return engine->status;
    }

    (void)memset(internal_engine_state, 0, sizeof(internal_engine_state_t));

    internal_engine_state->tier1_buffer_size = TIER1_BUFFER_SIZE_BYTES;
    internal_engine_state->tier1_buffer      = allocator->allocate_executable(
        allocator->context, 4096, internal_engine_state->tier1_buffer_size);

    if (NULL == internal_engine_state->tier1_buffer.rw_pointer)
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Failed to allocate Internal Tier 1 Buffer");
        engine->status = BAL_ERROR_ALLOCATION_FAILED;
        return engine->status;
    }

    const bal_error_t status = bal_tier1_compiler_init(&internal_engine_state->tier1_compiler,
                                                       internal_engine_state->tier1_buffer,
                                                       internal_engine_state->tier1_buffer_size,
                                                       logger);

    if (status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Failed to initialize Tier1 Compiler");
        allocator->free(allocator->context,
                        &internal_engine_state->tier1_buffer,
                        sizeof(internal_engine_state_t));
        allocator->free(allocator->context, internal_engine_state, sizeof(internal_engine_state_t));
        engine->status = status;
        return engine->status;
    }

    atomic_init(&internal_engine_state->thread_state.is_executing, false);
    atomic_init(&internal_engine_state->thread_state.stop_requested, false);
    atomic_init(&internal_engine_state->thread_state.clear_cache_requested, false);

    engine->cpu              = cpu;
    engine->allocator        = allocator;
    engine->memory_interface = memory_interface;
    engine->engine_state     = internal_engine_state;
    engine->logger           = logger;
    engine->status           = BAL_SUCCESS;
    engine->flags            = 0;
    return engine->status;
}

bal_error_t
bal_engine_run_thread(bal_engine_t *engine)
{
    internal_engine_state_t *internal_engine_state
        = (internal_engine_state_t *)engine->engine_state;
    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit(&internal_engine_state->thread_state.is_executing,
                                                 &expected,
                                                 false,
                                                 memory_order_acq_rel,
                                                 memory_order_acquire))
    {
        BAL_LOG_ERROR(&engine->logger, "Aborting function: Engine is already running");
        return BAL_ERROR_ENGINE_ALREADY_RUNNING;
    }

    engine->flags |= BAL_ENGINE_FLAG_RUNNING;

    while (true)
    {
        if (BAL_UNLIKELY(atomic_load_explicit(&internal_engine_state->thread_state.stop_requested,
                                              memory_order_acquire)))
        {
            BAL_LOG_INFO(&engine->logger, "Engine stop requested. Exiting execution loop.");
            break;
        }

        if (BAL_UNLIKELY(atomic_load_explicit(
                &internal_engine_state->thread_state.clear_cache_requested, memory_order_acquire)))
        {
            BAL_LOG_INFO(&engine->logger,
                         "Clear cache requested. Resetting JIT layout and block cache.");
            bal_tier1_compiler_reset(&internal_engine_state->tier1_compiler);
            (void)memset(
                internal_engine_state->block_cache, 0, sizeof(internal_engine_state->block_cache));
            atomic_store_explicit(&internal_engine_state->thread_state.clear_cache_requested,
                                  false,
                                  memory_order_release);
        }

        if (engine->flags & BAL_ENGINE_FLAG_INTERRUPT_PENDING)
        {
            BAL_LOG_INFO(&engine->logger, "Interrupt pending, yielding to host.");
            engine->flags &= ~BAL_ENGINE_FLAG_INTERRUPT_PENDING;
            break;
        }

        // TODO: Implement Block linking
        bal_guest_address_t pc = engine->cpu->pc;

        void *BAL_RESTRICT entry_point = NULL;

        if (BAL_LIKELY(!(engine->flags & BAL_ENGINE_FLAG_DISABLE_BLOCK_CACHE)))
        {
            entry_point = block_cache_lookup(internal_engine_state->block_cache, pc);
        }

        if (NULL == entry_point)
        {
            if (engine->flags & BAL_ENGINE_FLAG_LOG_BLOCKS)
            {
                BAL_LOG_INFO(
                    &engine->logger, "Fetching block at PC 0x%016llX", (unsigned long long)pc);
            }

            engine->allocator->protect_rw(engine->allocator->context,
                                          internal_engine_state->tier1_buffer,
                                          internal_engine_state->tier1_buffer_size);
            size_t max_instructions = MAX_INSTRUCTIONS;

            if (engine->flags & BAL_ENGINE_FLAG_SINGLE_STEP)
            {
                max_instructions = 1;
            }

            entry_point = bal_tier1_compiler_translate(&internal_engine_state->tier1_compiler,
                                                       engine->memory_interface,
                                                       pc,
                                                       max_instructions,
                                                       engine->flags);

            if (BAL_UNLIKELY(NULL == entry_point))
            {
                if (internal_engine_state->tier1_compiler.assembler.status
                    == BAL_ERROR_INSTRUCTION_OVERFLOW)
                {
                    BAL_LOG_INFO(&engine->logger,
                                 "Tier 1 executable buffer full. Resetting JIT layout cache");
                    bal_tier1_compiler_reset(&internal_engine_state->tier1_compiler);

                    // TODO: When the block cache is implemented, it must be cleared here.
                    entry_point
                        = bal_tier1_compiler_translate(&internal_engine_state->tier1_compiler,
                                                       engine->memory_interface,
                                                       pc,
                                                       max_instructions,
                                                       engine->flags);
                }

                if (BAL_UNLIKELY(NULL == entry_point))
                {
                    BAL_LOG_ERROR(&engine->logger,
                                  "Aborting function: Failed to compile block at PC 0x%0llx",
                                  // WARNING: C standard guarantees unsigned long long is at least
                                  // 64 bits wide.
                                  (unsigned long long)pc);
                }
            }

            if (BAL_UNLIKELY(NULL == entry_point))
            {
                BAL_LOG_ERROR(&engine->logger,
                              "Aborting function: Failed to compile block at PC 0x%0llX",
                              (unsigned long long)pc);
                engine->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
                break;
            }
        }

        engine->allocator->protect_rx(engine->allocator->context,
                                      internal_engine_state->tier1_buffer,
                                      internal_engine_state->tier1_buffer_size);

        if (BAL_LIKELY(!(engine->flags & BAL_ENGINE_FLAG_DISABLE_BLOCK_CACHE)))
        {
            block_cache_insert(internal_engine_state->block_cache, pc, entry_point);
        }

        const bal_jit_block_t compiled_block = (bal_jit_block_t)entry_point;

        if (engine->flags & BAL_ENGINE_FLAG_LOG_BLOCKS)
        {
            BAL_LOG_INFO(&engine->logger, "Executing JIT Block at %p", entry_point);
        }

        compiled_block(engine->cpu);

        if (engine->flags & BAL_ENGINE_FLAG_SINGLE_STEP)
        {
            break;
        }
    }

    engine->flags &= ~BAL_ENGINE_FLAG_RUNNING;
    atomic_store_explicit(
        &internal_engine_state->thread_state.is_executing, false, memory_order_release);
    return engine->status;
}

void
bal_engine_stop_thread(bal_engine_t *BAL_RESTRICT engine)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == engine->engine_state))
    {
        BAL_LOG_ERROR(&engine->logger, "Aborting function: engine state is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    internal_engine_state_t *BAL_RESTRICT engine_state
        = (internal_engine_state_t *)engine->engine_state;
    atomic_store_explicit(&engine_state->thread_state.stop_requested, true, memory_order_release);
}

bal_error_t
bal_engine_reset(bal_engine_t *BAL_RESTRICT engine)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    engine->status = BAL_SUCCESS;
    internal_engine_state_t *BAL_RESTRICT engine_state
        = (internal_engine_state_t *)engine->engine_state;
    (void)memset(&engine_state->tier1_buffer, 0, engine_state->tier1_buffer_size);
    bal_tier1_compiler_reset(&engine_state->tier1_compiler);
    return engine->status;
}

bool
bal_engine_is_running(bal_engine_t *engine)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return false;
    }

    if (BAL_UNLIKELY(NULL == engine->engine_state))
    {
        BAL_LOG_ERROR(&engine->logger, "Aborting function: engine state is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return false;
    }

    internal_engine_state_t *BAL_RESTRICT engine_state
        = (internal_engine_state_t *)engine->engine_state;
    return atomic_load_explicit(&engine_state->thread_state.is_executing, memory_order_acquire);
}

void
bal_engine_clear_cache(bal_engine_t *engine)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == engine->engine_state))
    {
        BAL_LOG_ERROR(&engine->logger, "Aborting function: engine state is NULL");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    internal_engine_state_t *BAL_RESTRICT engine_state
        = (internal_engine_state_t *)engine->engine_state;
    atomic_store_explicit(
        &engine_state->thread_state.clear_cache_requested, true, memory_order_release);
}

BAL_HOT void *
block_cache_lookup(const block_cache_set_t *BAL_RESTRICT cache, const bal_guest_address_t pc)
{
    const uint32_t                          set_index = (uint32_t)(pc >> 2) & BLOCK_CACHE_MASK;
    const block_cache_entry_t *BAL_RESTRICT ways      = cache[set_index].ways;

    for (uint32_t i = 0; i < BLOCK_CACHE_WAYS; ++i)
    {
        if (ways[i].guest_address == pc && ways[i].host_code != NULL)
        {
            return ways[i].host_code;
        }
    }

    return NULL;
}

BAL_HOT void
block_cache_insert(block_cache_set_t *BAL_RESTRICT cache,
                   const bal_guest_address_t       pc,
                   void *BAL_RESTRICT              host_code)
{
    const uint32_t                    set_index = (uint32_t)(pc >> 2) & BLOCK_CACHE_MASK;
    block_cache_entry_t *BAL_RESTRICT ways      = cache[set_index].ways;

    for (uint32_t i = 0; i < BLOCK_CACHE_WAYS; ++i)
    {
        if (ways[i].guest_address == pc)
        {
            ways[i].host_code = host_code;
            return;
        }
    }

    for (uint32_t i = 0; i < BLOCK_CACHE_WAYS; ++i)
    {
        if (NULL == ways[i].host_code)
        {
            ways[i].guest_address = pc;
            ways[i].host_code     = host_code;
            return;
        }
    }

    // All 4 ways are full. Use the lower bits of the PC as a cheap starting index.
    const uint32_t evict_way      = (pc >> 4) & (BLOCK_CACHE_WAYS - 1);
    ways[evict_way].guest_address = pc;
    ways[evict_way].host_code     = host_code;
}