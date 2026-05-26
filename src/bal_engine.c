#include "bal_engine.h"
#include "backend/x86/bal_x86_tier1_compiler.h"
#include "bal_decoder.h"
#include "bal_logging.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define MAX_INSTRUCTIONS 65535

// Not sure what exact value to put here.
//
#define MAX_GUEST_REGISTERS 128

/// The size of an ARM Instruction in bytes.
#define ARM_INSTRUCTION_SIZE_BYTES 4

/// Helper macro to align `x` UP to the nearest memory alignment.
#define BAL_ALIGN_UP(x, memory_alignment) \
    (((x) + ((memory_alignment) - 1)) & ~((memory_alignment) - 1))

#define SOURCE_VARIABLES_SIZE_BYTES MAX_GUEST_REGISTERS * sizeof(bal_source_variable_t)
#define SSA_BIT_WIDTHS_SIZE_BYTES   MAX_INSTRUCTIONS * sizeof(bal_bit_width_t)
#define INSTRUCTIONS_SIZE_BYTES     MAX_INSTRUCTIONS * sizeof(bal_instruction_t)
#define CONSTANTS_SIZE_BYTES        MAX_INSTRUCTIONS * sizeof(bal_instruction_t)
#define MEMORY_ALIGNMENT            64U

#define OFFSET_INSTRUCTIONS BAL_ALIGN_UP(SOURCE_VARIABLES_SIZE_BYTES, MEMORY_ALIGNMENT)
#define OFFSET_SSA_BIT_WIDTHS \
    BAL_ALIGN_UP((OFFSET_INSTRUCTIONS + INSTRUCTIONS_SIZE_BYTES), MEMORY_ALIGNMENT)
#define OFFSET_CONSTANTS \
    BAL_ALIGN_UP((OFFSET_SSA_BIT_WIDTHS + SSA_BIT_WIDTHS_SIZE_BYTES), MEMORY_ALIGNMENT)
#define ARENA_SIZE_BYTES BAL_ALIGN_UP((OFFSET_CONSTANTS + CONSTANTS_SIZE_BYTES), MEMORY_ALIGNMENT)

#define TIER1_BUFFER_SIZE_BYTES (1024 * 1024 * 16) // 16 MiB

typedef struct
{
    // Tier 1 State

    bal_tier1_compiler_t    tier1_compiler;
    bal_executable_buffer_t tier1_buffer;
    size_t                  tier1_buffer_size;

    // Tier 2 State

    void                   *ir_arena_base;
    bal_instruction_count_t instruction_count;
    bal_constant_count_t    constant_count;
} internal_engine_state_t;

typedef struct
{
    bal_instruction_t      *ir_instruction_cursor;
    bal_bit_width_t        *bit_width_cursor;
    bal_source_variable_t  *source_variables;
    bal_constant_t         *constants;
    size_t                  constants_size;
    bal_constant_count_t    constant_count;
    bal_instruction_count_t instruction_count;
    bal_error_t             status;
    bal_logger_t           *logger;
} bal_tier2_translation_context_t;

static_assert(sizeof(bal_tier2_translation_context_t) <= 64, "Context must fit in  1 cache line");

// static uint32_t extract_operand_value(uint32_t, const bal_decoder_operand_t *);
// static uint32_t intern_constant(bal_translation_context_t *, bal_constant_t);
// static void     translate_const(bal_translation_context_t *,
// const bal_decoder_instruction_metadata_t *,
// const uint32_t *);
// static void     translate_sub(bal_translation_context_t *,
// const bal_decoder_instruction_metadata_t *,
// const uint32_t *);
// static void     translate_return(bal_translation_context_t *, const uint32_t *);

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
        = allocator->allocate(allocator->handle, MEMORY_ALIGNMENT, sizeof(internal_engine_state_t));

    if (BAL_UNLIKELY(NULL == internal_engine_state))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Failed to allocate Internal Engine State");
        engine->status = BAL_ERROR_ALLOCATION_FAILED;
        return engine->status;
    }

    (void)memset(internal_engine_state, 0, sizeof(internal_engine_state_t));

    internal_engine_state->ir_arena_base
        = allocator->allocate(allocator->handle, MEMORY_ALIGNMENT, ARENA_SIZE_BYTES);

    if (BAL_UNLIKELY(NULL == internal_engine_state->ir_arena_base))
    {
        allocator->free(allocator->handle, internal_engine_state, sizeof(internal_engine_state_t));
        BAL_LOG_ERROR(&logger, "Aborting function: Failed to allocate Internal IR Arena");
        engine->status = BAL_ERROR_ALLOCATION_FAILED;
    }

    (void)memset(
        internal_engine_state->ir_arena_base, POISON_UNINITIALIZED_MEMORY, ARENA_SIZE_BYTES);
    internal_engine_state->instruction_count = 0;
    internal_engine_state->constant_count    = 0;
    internal_engine_state->tier1_buffer_size = TIER1_BUFFER_SIZE_BYTES;
    internal_engine_state->tier1_buffer      = allocator->allocate_executable(
        allocator->handle, 4096, internal_engine_state->tier1_buffer_size);

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
        allocator->free(allocator->handle,
                        &internal_engine_state->tier1_buffer,
                        sizeof(internal_engine_state_t));
        allocator->free(allocator->handle,
                        internal_engine_state->ir_arena_base,
                        sizeof(internal_engine_state_t));
        allocator->free(allocator->handle, internal_engine_state, sizeof(internal_engine_state_t));
        engine->status = status;
        return engine->status;
    }

    BAL_LOG_DEBUG(&logger, "Calculating arena layout (Alignment: %zu bytes):", MEMORY_ALIGNMENT);
    BAL_LOG_DEBUG(&logger,
                  "  [0x%08zx] source_variables (%zu bytes)",
                  (size_t)0,
                  SOURCE_VARIABLES_SIZE_BYTES);
    BAL_LOG_DEBUG(&logger,
                  "  [0x%08zx] instructions     (%zu bytes)",
                  OFFSET_INSTRUCTIONS,
                  INSTRUCTIONS_SIZE_BYTES);
    BAL_LOG_DEBUG(&logger,
                  "  [0x%08zx] ssa_bit_widths   (%zu bytes)",
                  OFFSET_SSA_BIT_WIDTHS,
                  SSA_BIT_WIDTHS_SIZE_BYTES);
    BAL_LOG_DEBUG(&logger,
                  "  [0x%08zx] constants        (%zu bytes)",
                  OFFSET_CONSTANTS,
                  CONSTANTS_SIZE_BYTES);

    BAL_LOG_INFO(&logger,
                 "Initialized engine successfully. Arena: %p (%zu KB)",
                 internal_engine_state->ir_arena_base,
                 ARENA_SIZE_BYTES / 1024);
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
bal_engine_run(bal_engine_t *engine)
{
    internal_engine_state_t *internal_engine_state = engine->engine_state;
    engine->flags |= BAL_ENGINE_FLAG_RUNNING;

    while (engine->flags & BAL_ENGINE_FLAG_RUNNING)
    {
        // TODO: Lookup PC in a Block Cache Map
        //
        bal_guest_address_t pc = engine->cpu->pc;
        engine->allocator->protect_rw(engine->allocator->handle,
                                      internal_engine_state->tier1_buffer,
                                      internal_engine_state->tier1_buffer_size);

        void *entry_point = bal_tier1_compiler_translate(
            &internal_engine_state->tier1_compiler, engine->memory_interface, pc, MAX_INSTRUCTIONS);

        if (BAL_UNLIKELY(NULL == entry_point))
        {
            BAL_LOG_ERROR(&engine->logger,
                          "Aborting function: Failed to compile block at PC 0x%0llX",
                          (unsigned long long)pc);
            engine->status = BAL_ERROR_UNKNOWN_INSTRUCTION;
            engine->flags &= (uint32_t)~BAL_ENGINE_FLAG_RUNNING;
            break;
        }

        engine->allocator->protect_rx(engine->allocator->handle,
                                      internal_engine_state->tier1_buffer,
                                      internal_engine_state->tier1_buffer_size);
        const bal_jit_block_t compiled_block = entry_point;
        BAL_LOG_INFO(&engine->logger, "Executing JIT Block at %p", entry_point);
        compiled_block(engine->cpu);
        engine->flags &= (uint32_t)~BAL_ENGINE_FLAG_RUNNING;
    }

    return engine->status;
}

#if 0
bal_error_t
bal_engine_translate_tier2(bal_engine_t *BAL_RESTRICT                 engine,
                           const bal_memory_interface_t *BAL_RESTRICT interface,
                           bal_guest_address_t                       *guest_address_start,
                           const size_t                               max_instructions)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(engine->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&engine->logger, "Engine != BAL_SUCCESS, aborting translation");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (BAL_UNLIKELY(NULL == interface))
    {
        BAL_LOG_ERROR(&engine->logger, "Interface is NULL, aborting translation");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(0 == max_instructions))
    {
        BAL_LOG_INFO(&engine->logger, "Max Instructions is 0, aborting translation");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    if (BAL_UNLIKELY(NULL == guest_address_start))
    {
        BAL_LOG_ERROR(&engine->logger, "Guest Address is NULL, aborting translation");
        engine->status = BAL_ERROR_INVALID_ARGUMENT;
        return engine->status;
    }

    const size_t max_instructions_size_bytes = max_instructions * ARM_INSTRUCTION_SIZE_BYTES;
    BAL_LOG_INFO(&engine->logger,
                 "Starting JIT unit. GVA: %p, Size: %zu bytes ",
                 (void *)*guest_address_start,
                 max_instructions_size_bytes);

    bal_translation_context_t context
        = { .ir_instruction_cursor
            = (bal_instruction_t *)((uint8_t *)engine->arena_base + OFFSET_INSTRUCTIONS),
            .bit_width_cursor = (uint8_t *)engine->arena_base + OFFSET_SSA_BIT_WIDTHS,
            .source_variables = (bal_source_variable_t *)engine->arena_base,
            .constants      = (bal_constant_t *)((uint8_t *)engine->arena_base + OFFSET_CONSTANTS),
            .constant_count = engine->constant_count,
            .instruction_count = engine->instruction_count,
            .status            = engine->status,
            .logger            = &engine->logger };

    bool     is_block_terminated                         = false;
    uint32_t arm_instruction_operands[BAL_OPERANDS_SIZE] = { 0 };

    while (false == is_block_terminated)
    {
        size_t                            max_readable_instructions_bytes = 0;
        bal_guest_address_t *BAL_RESTRICT guest_address_current           = guest_address_start;

        if (BAL_UNLIKELY((uintptr_t)guest_address_current % 4 != 0))
        {
            BAL_LOG_ERROR(context.logger,
                          "Guest Virtual Address 0x%016llX is not 4-byte aligned",
                          (unsigned long long)guest_address_current);
            context.status = BAL_ERROR_MEMORY_ALIGNMENT;
            break;
        }
        const uint32_t *host_address_base = (const uint32_t *)interface->translate(
            (void *)interface, *guest_address_current, &max_readable_instructions_bytes);

        if (BAL_UNLIKELY(NULL == host_address_base))
        {
            BAL_LOG_ERROR(context.logger,
                          "Memory translation fault at GVA 0x%016llX, aborting translation block",
                          (unsigned long long)*guest_address_start);
            context.status = BAL_ERROR_MEMORY_FAULT;
            break;
        }

        const uint32_t *BAL_RESTRICT host_address_current = host_address_base;
        const size_t max_readable_instructions = max_readable_instructions_bytes / sizeof(uint32_t);

        // Has the remaining instructions crossed a memory page boundary? This should not happen on
        // ARM64 since instructions are strictly 4 byte aligned. So throw an error if this
        // happens.
        //
        if (BAL_UNLIKELY(0 == max_readable_instructions))
        {
            BAL_LOG_ERROR(context.logger,
                          "Insufficient memory at GVA 0x%016llX. Need 4 bytes for an instruction, "
                          "but only %zu bytes are readable",
                          (unsigned long long)guest_address_current,
                          max_readable_instructions_bytes);
            context.status = BAL_ERROR_PC_ALIGNMENT;
            break;
        }

        for (size_t i = 0; i < max_readable_instructions; ++i)
        {
            if (BAL_UNLIKELY(context.instruction_count >= (MAX_INSTRUCTIONS - 128)))
            {
                BAL_LOG_WARN(context.logger,
                             "Critical buffer pressure. Inst:  %u/%d",
                             context.instruction_count,
                             MAX_INSTRUCTIONS);
            }

            const bal_decoder_instruction_metadata_t *metadata
                = bal_decode_arm64(*host_address_current);

            const size_t relative_offset
                = (uintptr_t)host_address_current - (uintptr_t)host_address_base;

            if (BAL_UNLIKELY(NULL == metadata))
            {
                BAL_LOG_ERROR(context.logger,
                              "Decode failed for GVA 0x%08x at offset +0x%zx",
                              *guest_address_start,
                              relative_offset);
                context.status      = BAL_ERROR_UNKNOWN_INSTRUCTION;
                is_block_terminated = true;
                break;
            }

            BAL_LOG_DEBUG(context.logger,
                          "  [+0x%04zx] 0x%08x: %-8s (SSA ID: %u)",
                          relative_offset,
                          *guest_address_start,
                          metadata->name,
                          context.instruction_count);

            const bal_decoder_operand_t *BAL_RESTRICT operands_cursor = metadata->operands;

            for (size_t ii = 0; ii < BAL_OPERANDS_SIZE; ++ii)
            {
                arm_instruction_operands[ii]
                    = extract_operand_value(*host_address_current, operands_cursor);
                ++operands_cursor;
            }

            switch (metadata->ir_opcode)
            {
                case OPCODE_CONST:
                    translate_const(&context, metadata, arm_instruction_operands);
                    break;
                case OPCODE_SUB:
                    translate_sub(&context, metadata, arm_instruction_operands);
                    break;
                case OPCODE_RETURN:
                    translate_return(&context, arm_instruction_operands);
                    is_block_terminated = true;
                    break;
                default:
                    BAL_LOG_DEBUG(context.logger,
                                  "  SKIPPED: Opcode %s not implemented in IR layer.",
                                  metadata->name);
                    break;
            }

            if (BAL_UNLIKELY(context.status != BAL_SUCCESS))
            {
                BAL_LOG_ERROR(context.logger, "  Status failure: %d", context.status);
                is_block_terminated = true;
                break;
            }

            if (true == is_block_terminated)
            {
                break;
            }

            ++context.ir_instruction_cursor;
            ++context.bit_width_cursor;
            *guest_address_current += 4;
            ++host_address_current;
        }
    }

    engine->instruction_count = context.instruction_count;
    engine->constant_count    = context.constant_count;
    engine->status            = context.status;

    BAL_LOG_INFO(&engine->logger,
                 "Finished. Produced %u instructions, %u constants.",
                 engine->instruction_count,
                 engine->constant_count);

    return engine->status;
}

bal_error_t
bal_engine_reset(bal_engine_t *engine)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    engine->instruction_count = 0;
    engine->status            = BAL_SUCCESS;

    (void)memset(engine->arena_base, POISON_UNINITIALIZED_MEMORY, ARENA_SIZE_BYTES);

    return engine->status;
}

void
bal_engine_destroy(const bal_allocator_t *allocator, bal_engine_t *engine)
{
    // No argument error handling. Segfault if user passes NULL.

    allocator->free(allocator->handle, engine->arena_base, ARENA_SIZE_BYTES);
    engine->arena_base = NULL;
}
const bal_instruction_t *
bal_engine_get_ir_instructions(const bal_engine_t *engine)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return NULL;
    }

    if (BAL_UNLIKELY(NULL == engine->arena_base))
    {
        BAL_LOG_ERROR(&engine->logger, "Memory Arena is NULL, aborting function");
        return NULL;
    }

    const bal_instruction_t *ir_instructions = engine->arena_base + OFFSET_INSTRUCTIONS;
    return ir_instructions;
}
const bal_constant_t *
bal_engine_get_constant(const bal_engine_t *engine, const bal_constant_t index)
{
    if (BAL_UNLIKELY(NULL == engine))
    {
        return NULL;
    }

    if (BAL_UNLIKELY(NULL == engine->arena_base))
    {
        BAL_LOG_ERROR(&engine->logger, "Engine memory arena is NULL, aborting function call");
        return NULL;
    }

    const bal_constant_t *constants = engine->arena_base + OFFSET_CONSTANTS;
    const bal_constant_t *constant  = &constants[index];
    return constant;
}

BAL_HOT static uint32_t
extract_operand_value(const uint32_t instruction, const bal_decoder_operand_t *operand)
{
    if (BAL_OPERAND_TYPE_NONE == operand->type)
    {
        return 0;
    }

    const uint32_t mask = (1U << operand->bit_width) - 1;
    const uint32_t bits = instruction >> operand->bit_position & mask;
    return bits;
}

// If `constant` exists in the constants array return its index, if not, add it and then return
// its index
BAL_HOT static uint32_t
intern_constant(bal_translation_context_t *BAL_RESTRICT context, const bal_constant_t constant)
{
    if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
    {
        return 0;
    }

    // This can be upgraded to a hash map later on if it causes performance loss.
    //
    for (uint32_t i = 0; i < context->constant_count; ++i)
    {
        if (context->constants[i] == constant)
        {
            BAL_LOG_TRACE(context->logger, "  0X%016llX -> c%u", (unsigned long long)constant, i);
            return i | BAL_IS_CONSTANT_BIT_POSITION;
        }
    }

    const uint32_t index = context->constant_count;

    if (BAL_UNLIKELY(index >= MAX_INSTRUCTIONS))
    {
        BAL_LOG_ERROR(context->logger,
                      "Constant Pool Overflow at index %u (Max %u)",
                      index,
                      context->constants_size);
        context->status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return 0;
    }

    context->constants[index] = constant;
    context->constant_count++;
    BAL_LOG_TRACE(context->logger, "  0X%016llX -> c%u", (unsigned long long)constant, index);
    return index | BAL_IS_CONSTANT_BIT_POSITION;
}

BAL_HOT static uint32_t
get_or_create_ssa_index(bal_translation_context_t *context, const uint64_t register_index)
{
    uint32_t ssa_index = context->source_variables[register_index].current_ssa_index;

    const uint32_t invalid_ssa_index = 0xFFFFFFFF;

    // Checks if the ssa index is not initialized.
    //
    if (ssa_index != invalid_ssa_index)
    {
        return ssa_index;
    }

    const bal_instruction_t instruction = (bal_instruction_t)OPCODE_GET_REGISTER
                                              << BAL_OPCODE_SHIFT_POSITION
                                          | register_index << BAL_SOURCE1_SHIFT_POSITION;

    *context->ir_instruction_cursor                             = instruction;
    ssa_index                                                   = context->instruction_count;
    context->source_variables[register_index].current_ssa_index = ssa_index;

    BAL_LOG_DEBUG(context->logger,
                  "  EMIT: v%u = GET_REGISTER X%u",
                  context->instruction_count,
                  register_index);
    BAL_LOG_TRACE(
        context->logger, "  SSA UPDATE: X%lu -> v%lu", register_index, context->instruction_count);

    context->instruction_count++;
    context->ir_instruction_cursor++;
    context->bit_width_cursor++;
    return ssa_index;
}

BAL_HOT static void
translate_const(bal_translation_context_t                *context,
                const bal_decoder_instruction_metadata_t *metadata,
                const uint32_t                           *arm_registers)
{
    const uint64_t rd    = arm_registers[0];
    const uint64_t imm16 = arm_registers[1];
    const uint64_t hw    = arm_registers[2];
    const uint64_t shift = hw * 16;

    const uint64_t mask = 0xFFFFFFFFFFFFFFFFULL;

    // Calculate the shifted immediate value.
    //
    uint64_t value = imm16 << shift & mask;

    // Check mnemonic 4th character: MOV[Z], MOV[N], MOV[K].
    //
    const char variant = metadata->name[3];

    BAL_LOG_TRACE(context->logger,
                  "  Variant='%c' Rd=%lu Imm=0x%lX Shift=%lu Mask=0x%llX",
                  variant,
                  rd,
                  imm16,
                  shift,
                  mask);

    if ('N' == variant)
    {
        value = ~value & mask;
        BAL_LOG_TRACE(context->logger, "  MOVN Inversion: New Value=0x%llX", value);
    }

    if ('K' == variant)
    {
        // MOVK:
        // mask = ~(0xFFFF << shift)
        // cleared_val = Old_Rd & mask
        // new_val = cleared_val + (imm << shift)

        uint64_t old_ssa;
        uint64_t old_ssa_with_flag;

        if (31 == rd)
        {
            BAL_LOG_TRACE(context->logger, "  MOVK Source is ZR. Interning 0.");
            old_ssa_with_flag = intern_constant(context, 0);
            old_ssa           = old_ssa_with_flag & ~BAL_IS_CONSTANT_BIT_POSITION;
        }
        else
        {
            old_ssa_with_flag = get_or_create_ssa_index(context, rd);
            old_ssa           = old_ssa_with_flag & ~BAL_IS_CONSTANT_BIT_POSITION;
            BAL_LOG_TRACE(context->logger, "  MOVK Source: Reg X%u -> SSA v%u", rd, old_ssa);
        }

        const uint64_t clear_mask = (~(0xFFFFULL << shift)) & mask;
        const uint64_t mask_index = intern_constant(context, clear_mask);

        if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
        {
            return;
        }

        *context->ir_instruction_cursor = (bal_instruction_t)OPCODE_AND << BAL_OPCODE_SHIFT_POSITION
                                          | old_ssa_with_flag << BAL_SOURCE1_SHIFT_POSITION
                                          | mask_index << BAL_SOURCE2_SHIFT_POSITION;

        BAL_LOG_DEBUG(context->logger,
                      "  EMIT: v%u = AND v%u, c%u (Mask: 0x%llX)",
                      context->instruction_count,
                      old_ssa,
                      mask_index & ~BAL_IS_CONSTANT_BIT_POSITION,
                      clear_mask);

        const uint64_t cleared_ssa = context->instruction_count;

        // Remove unused variable warning from release builds.
        //
        (void)old_ssa;
        (void)cleared_ssa;

        // Advance cursor for the AND instruction.
        //
        context->ir_instruction_cursor++;
        context->bit_width_cursor++;
        context->instruction_count++;

        const uint64_t value_index = intern_constant(context, value);

        // Source 1 is the result of the AND instruction.
        //
        const uint64_t masked_ssa       = context->instruction_count - 1;
        *context->ir_instruction_cursor = (bal_instruction_t)OPCODE_ADD << BAL_OPCODE_SHIFT_POSITION
                                          | masked_ssa << BAL_SOURCE1_SHIFT_POSITION
                                          | (bal_instruction_t)value_index
                                                << BAL_SOURCE2_SHIFT_POSITION;

        BAL_LOG_DEBUG(context->logger,
                      "  EMIT: v%u = ADD v%u, c%u (Val: 0x%llX)",
                      context->instruction_count,
                      cleared_ssa,
                      value_index & ~BAL_IS_CONSTANT_BIT_POSITION,
                      value);
    }
    else
    {
        const uint64_t constant_index = intern_constant(context, value);

        if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
        {
            return;
        }

        *context->ir_instruction_cursor
            = (bal_instruction_t)OPCODE_CONST << BAL_OPCODE_SHIFT_POSITION
              | (bal_instruction_t)constant_index << BAL_SOURCE1_SHIFT_POSITION;

        BAL_LOG_DEBUG(context->logger,
                      "  EMIT: v%u = CONST %u (0x%llX)",
                      context->instruction_count,
                      constant_index & ~BAL_IS_CONSTANT_BIT_POSITION,
                      value);
    }

    // Only update the SSA map is not writing to XZR/WZR.
    //
    if (rd != 31)
    {
        context->source_variables[rd].current_ssa_index = context->instruction_count;
        BAL_LOG_TRACE(
            (context)->logger, "  SSA UPDATE: X%u -> v%u", rd, context->instruction_count);
    }
    else
    {
        BAL_LOG_TRACE(context->logger, "    SSA NO-OP: Destination is XZR");
    }

    context->instruction_count++;
}

BAL_HOT static void
translate_return(bal_translation_context_t *context, const uint32_t *arm_registers)
{
    const uint64_t rn               = arm_registers[0];
    const uint64_t rn_ssa_index     = get_or_create_ssa_index(context, rn);
    *context->ir_instruction_cursor = (bal_instruction_t)OPCODE_RETURN << BAL_OPCODE_SHIFT_POSITION
                                      | rn_ssa_index << BAL_SOURCE1_SHIFT_POSITION;
    BAL_LOG_DEBUG(
        context->logger, "   EMIT: v%u = RET v%u", context->instruction_count, rn_ssa_index);
    context->instruction_count++;
}

BAL_HOT static void
translate_sub(bal_translation_context_t                *context,
              const bal_decoder_instruction_metadata_t *metadata,
              const uint32_t                           *arm_registers)
{
    if (BAL_UNLIKELY(BAL_OPERAND_TYPE_IMMEDIATE != metadata->operands[2].type))
    {
        BAL_LOG_DEBUG(context->logger,
                      "  SKIPPED: SUB variant '%s' not yet implemented in IR layer.",
                      metadata->name);
        return;
    }

    const uint64_t rd    = arm_registers[0];
    const uint64_t rn    = arm_registers[1];
    const uint64_t imm12 = arm_registers[2];
    const uint64_t sh    = arm_registers[3];
    const uint64_t shift = (1 == sh) ? 12 : 0;
    const uint64_t value = imm12 << shift;

    BAL_LOG_TRACE(context->logger,
                  "  Variant='Imm' Rd=%lu Rn=%lu Imm12=0x%lX Shift=%lu Value=0x%llX",
                  rd,
                  rn,
                  imm12,
                  shift,
                  (unsigned long long)value);

    const uint64_t rn_ssa_index      = get_or_create_ssa_index(context, rn);
    const uint64_t value_const_index = intern_constant(context, value);

    if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
    {
        return;
    }

    bal_bit_width_t bit_width;

    switch (metadata->operands[0].type)
    {
        case BAL_OPERAND_TYPE_REGISTER_32:
            bit_width = 32;
            break;
        case BAL_OPERAND_TYPE_REGISTER_64:
            bit_width = 64;
            break;
        default:
            BAL_LOG_ERROR(context->logger, "Unknown register type for SUB (Imm) Rd register");
            context->status = BAL_ERROR_INCORRECT_REGISTER_TYPE;
            return;
    }

    *context->ir_instruction_cursor = (bal_instruction_t)OPCODE_SUB << BAL_OPCODE_SHIFT_POSITION
                                      | rn_ssa_index << BAL_SOURCE1_SHIFT_POSITION
                                      | value_const_index << BAL_SOURCE2_SHIFT_POSITION;

    *context->bit_width_cursor = bit_width;

    BAL_LOG_DEBUG(context->logger,
                  "  EMIT: v%u = SUB v%u, c%u (%u-bit)",
                  context->instruction_count,
                  (uint32_t)rn_ssa_index,
                  (uint32_t)(value_const_index & ~BAL_IS_CONSTANT_BIT_POSITION),
                  bit_width);

    context->source_variables[rd].current_ssa_index = context->instruction_count;
    BAL_LOG_TRACE(context->logger, "  SSA UPDATE: X%lu -> v%u", rd, context->instruction_count);

    context->instruction_count++;
}
#endif