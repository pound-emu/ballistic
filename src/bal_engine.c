#include "bal_engine.h"
#include "bal_decoder.h"
#include "bal_logging.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MAX_INSTRUCTIONS 65536

// Not sure what exact value to put here.
//
#define MAX_GUEST_REGISTERS 128

/// Helper macro to align `x` UP to the nearest memory alignment.
#define BAL_ALIGN_UP(x, memory_alignment) \
    (((x) + ((memory_alignment) - 1)) & ~((memory_alignment) - 1))

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
} bal_translation_context_t;

static uint32_t    extract_operand_value(const uint32_t, const bal_decoder_operand_t *);
static uint32_t    intern_constant(bal_translation_context_t *, const bal_constant_t);
static inline void translate_const(bal_translation_context_t *,
                                   const bal_decoder_instruction_metadata_t *,
                                   uint32_t *,
                                   const bal_decoder_operand_t *);
BAL_COLD bal_error_t
bal_engine_init(bal_allocator_t *allocator, bal_engine_t *engine, bal_logger_t logger)
{
    if (NULL == allocator || NULL == engine)
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    size_t source_variables_size = MAX_GUEST_REGISTERS * sizeof(bal_source_variable_t);
    size_t ssa_bit_widths_size   = MAX_INSTRUCTIONS * sizeof(bal_bit_width_t);
    size_t instructions_size     = MAX_INSTRUCTIONS * sizeof(bal_instruction_t);
    size_t constants_size        = MAX_INSTRUCTIONS * sizeof(bal_instruction_t);

    // Calculate amount of memory needed for all arrays in engine.
    //
    size_t memory_alignment    = 64U;
    size_t offset_instructions = BAL_ALIGN_UP(source_variables_size, memory_alignment);

    size_t offset_ssa_bit_widths
        = BAL_ALIGN_UP((offset_instructions + instructions_size), memory_alignment);

    size_t offset_constants
        = BAL_ALIGN_UP((offset_ssa_bit_widths + ssa_bit_widths_size), memory_alignment);

    size_t total_size_with_padding
        = BAL_ALIGN_UP((offset_constants + constants_size), memory_alignment);

    uint8_t *data = (uint8_t *)allocator->allocate(
        allocator->handle, memory_alignment, total_size_with_padding);

    BAL_LOG_DEBUG(&logger, "Calculating arena layout (Alignment: %zu bytes):", memory_alignment);
    BAL_LOG_DEBUG(
        &logger, "  [0x%08zx] source_variables (%zu bytes)", (size_t)0, source_variables_size);
    BAL_LOG_DEBUG(&logger,
                  "  [0x%08zx] instructions     (%zu bytes)",
                  offset_instructions,
                  instructions_size);
    BAL_LOG_DEBUG(&logger,
                  "  [0x%08zx] ssa_bit_widths   (%zu bytes)",
                  offset_ssa_bit_widths,
                  ssa_bit_widths_size);
    BAL_LOG_DEBUG(
        &logger, "  [0x%08zx] constants        (%zu bytes)", offset_constants, constants_size);

    if (NULL == data)
    {
        BAL_LOG_ERROR(&logger, "Allocation of %zu bytes failed.", total_size_with_padding);
        engine->status = BAL_ERROR_ALLOCATION_FAILED;
        return engine->status;
    }

    engine->source_variables      = (bal_source_variable_t *)data;
    engine->instructions          = (bal_instruction_t *)(data + offset_instructions);
    engine->ssa_bit_widths        = (bal_bit_width_t *)(data + offset_ssa_bit_widths);
    engine->constants             = (bal_constant_t *)(data + offset_constants);
    engine->source_variables_size = source_variables_size / sizeof(bal_source_variable_t);
    engine->instructions_size     = instructions_size / sizeof(bal_instruction_t);
    engine->constants_size        = constants_size / sizeof(bal_constant_t);
    engine->constant_count        = 0;
    engine->instruction_count     = 0;
    engine->status                = BAL_SUCCESS;
    engine->arena_base            = (void *)data;
    engine->arena_size            = total_size_with_padding;
    engine->logger                = logger;

    BAL_LOG_INFO(&logger,
                 "Initialized engine successfully. Arena: %p (%zu KB)",
                 engine->arena_base,
                 total_size_with_padding / 1024);

    (void)memset(engine->source_variables, POISON_UNINITIALIZED_MEMORY, source_variables_size);
    (void)memset(engine->instructions, POISON_UNINITIALIZED_MEMORY, instructions_size);
    (void)memset(engine->ssa_bit_widths, POISON_UNINITIALIZED_MEMORY, ssa_bit_widths_size);
    (void)memset(engine->constants, POISON_UNINITIALIZED_MEMORY, constants_size);

    return engine->status;
}

bal_error_t
bal_engine_translate(bal_engine_t *BAL_RESTRICT           engine,
                     bal_memory_interface_t *BAL_RESTRICT interface,
                     const uint32_t *BAL_RESTRICT         arm_instruction_cursor,
                     size_t                               arm_size_bytes)
{
    (void)interface;

    if (BAL_UNLIKELY(NULL == engine || NULL == arm_instruction_cursor))
    {
        return BAL_ERROR_ENGINE_STATE_INVALID;
    }

    BAL_LOG_INFO(&(engine->logger),
                 "Starting JIT unit. GVA: %p, Size: %zu bytes ",
                 (void *)arm_instruction_cursor,
                 arm_size_bytes);

    bal_translation_context_t context
        = { .ir_instruction_cursor = engine->instructions + engine->instruction_count,
            .bit_width_cursor      = engine->ssa_bit_widths + engine->instruction_count,
            .source_variables      = engine->source_variables,
            .constants             = engine->constants,
            .constants_size        = engine->constants_size,
            .constant_count        = engine->constant_count,
            .instruction_count     = engine->instruction_count,
            .status                = engine->status,
            .logger                = &engine->logger };

    const bal_instruction_t *BAL_RESTRICT ir_instruction_end
        = engine->instructions + engine->instructions_size;
    const uint32_t *arm_start = arm_instruction_cursor;
    const uint32_t *arm_end   = arm_instruction_cursor + (arm_size_bytes / sizeof(uint32_t));
    uint32_t        arm_registers[BAL_OPERANDS_SIZE] = { 0 };

    while ((context.ir_instruction_cursor < ir_instruction_end)
           && (arm_instruction_cursor < arm_end))
    {
        if (BAL_UNLIKELY(context.instruction_count >= (MAX_INSTRUCTIONS - 128)))
        {
            BAL_LOG_WARN(context.logger,
                         "Critical buffer pressure. Inst:  %u/%d",
                         context.instruction_count,
                         MAX_INSTRUCTIONS);
        }

        const bal_decoder_instruction_metadata_t *metadata
            = bal_decode_arm64(*arm_instruction_cursor);

        size_t relative_offset = (size_t)((uintptr_t)arm_instruction_cursor - (uintptr_t)arm_start);
        if (BAL_UNLIKELY(NULL == metadata))
        {

            BAL_LOG_ERROR(context.logger,
                          "Decode failed for opcode 0x%08x at offset +0x%zx",
                          arm_instruction_cursor,
                          relative_offset);
            context.status = BAL_ERROR_UNKNOWN_INSTRUCTION;
            break;
        }

        BAL_LOG_TRACE(context.logger,
                      "  [+0x%04zx] 0x%08x: %-8s (SSA ID: %u)",
                      relative_offset,
                      arm_instruction_cursor,
                      metadata->name,
                      context.instruction_count);

        const bal_decoder_operand_t *BAL_RESTRICT operands_cursor = metadata->operands;

        for (size_t i = 0; i < BAL_OPERANDS_SIZE; ++i)
        {
            arm_registers[i] = extract_operand_value(*arm_instruction_cursor, operands_cursor);
            ++operands_cursor;
        }

        operands_cursor = metadata->operands;

        switch (metadata->ir_opcode)
        {
            case OPCODE_CONST:
                translate_const(&context, metadata, arm_registers, operands_cursor);
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
            break;
        }

        ++context.ir_instruction_cursor;
        ++context.bit_width_cursor;
        ++arm_instruction_cursor;
    }

    engine->instruction_count = context.instruction_count;
    engine->constant_count    = context.constant_count;
    engine->status            = context.status;

    BAL_LOG_INFO(&(engine->logger),
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

    (void)memset(
        engine->source_variables, POISON_UNINITIALIZED_MEMORY, engine->source_variables_size);

    (void)memset(engine->constants, POISON_UNINITIALIZED_MEMORY, engine->constants_size);

    return engine->status;
}

void
bal_engine_destroy(bal_allocator_t *allocator, bal_engine_t *engine)
{
    // No argument error handling. Segfault if user passes NULL.

    allocator->free(allocator->handle, engine->arena_base, engine->arena_size);
    engine->arena_base       = NULL;
    engine->source_variables = NULL;
    engine->instructions     = NULL;
    engine->ssa_bit_widths   = NULL;
}

BAL_HOT static uint32_t
extract_operand_value(const uint32_t instruction, const bal_decoder_operand_t *operand)
{
    if (BAL_OPERAND_TYPE_NONE == operand->type)
    {
        return 0;
    }

    uint32_t mask = (1U << operand->bit_width) - 1;
    uint32_t bits = (instruction >> operand->bit_position) & mask;
    return bits;
}

BAL_HOT static uint32_t
intern_constant(bal_translation_context_t *BAL_RESTRICT context, bal_constant_t constant)
{
    if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
    {
        return 0;
    }

    uint32_t index = context->constant_count;

    if (BAL_UNLIKELY(index >= context->constants_size))
    {
        BAL_LOG_ERROR(context->logger, "Constant pool overflow.");
        context->status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return 0;
    }

    context->constants[index] = constant;
    context->constant_count++;
    BAL_LOG_DEBUG(context->logger, "  %zu -> Pool Index %u", constant, index);
    return index | BAL_IS_CONSTANT_BIT_POSITION;
}

BAL_HOT static inline uint32_t
get_or_create_ssa_index(bal_translation_context_t *context, uint64_t register_index)
{
    uint32_t ssa_index = context->source_variables[register_index].current_ssa_index;

    const uint32_t invalid_ssa_index = 0xFFFFFFFF;

    if (ssa_index != invalid_ssa_index)
    {
        return ssa_index;
    }

    bal_instruction_t instruction
        = ((bal_instruction_t)OPCODE_GET_REGISTER << BAL_OPCODE_SHIFT_POSITION)
          | ((bal_instruction_t)register_index << BAL_SOURCE1_SHIFT_POSITION);

    *context->ir_instruction_cursor                             = instruction;
    ssa_index                                                   = context->instruction_count;
    context->source_variables[register_index].current_ssa_index = ssa_index;

    BAL_LOG_DEBUG(context->logger,
                  "  EMIT: v%lu = GET_REGISTER X%lu",
                  context->instruction_count,
                  register_index);

    context->instruction_count++;
    context->ir_instruction_cursor++;
    context->bit_width_cursor++;
    return ssa_index;
}

BAL_HOT static inline void
translate_const(bal_translation_context_t *BAL_RESTRICT                context,
                const bal_decoder_instruction_metadata_t *BAL_RESTRICT metadata,
                uint32_t *BAL_RESTRICT                                 arm_registers,
                const bal_decoder_operand_t *BAL_RESTRICT              operands)
{
    uint64_t rd    = arm_registers[0];
    uint64_t imm16 = arm_registers[1];
    uint64_t hw    = arm_registers[2];
    uint64_t shift = hw * 16;

    uint64_t mask = (32 == operands[0].bit_width) ? 0xFFFFFFFFULL : 0xFFFFFFFFFFFFFFFFULL;

    // Calculate the shifted immediate value.
    //
    uint64_t value = (imm16 << shift) & mask;

    // Check mneminic 4th character: MOV[Z], MOV[N], MOV[K].
    //
    char variant = metadata->name[3];

    BAL_LOG_TRACE(context->logger,
                  "  Variant='%c' Rd=%lu Imm=0x%lX Shift=%lu Mask=0x%llX",
                  variant,
                  rd,
                  imm16,
                  shift,
                  mask);

    if ('N' == variant)
    {
        value = (~value) & mask;
        BAL_LOG_TRACE(context->logger, "  MOVN Inversion: New Value=0x%llX", value);
    }

    if ('K' == variant)
    {
        // MOVK:
        // mask = ~(0xFFFF << shift)
        // cleared_val = Old_Rd & mask
        // new_val = cleared_val + (imm << shift)

        uint64_t old_ssa;

        if (31 == rd)
        {
            BAL_LOG_TRACE(context->logger, "  MOVK Source is ZR. Interning 0.");
            old_ssa = intern_constant(context, 0);
        }
        else
        {
            old_ssa = get_or_create_ssa_index(context, rd);
            BAL_LOG_TRACE(context->logger, "  MOVK Source: Reg X%lu -> SSA v%lu", rd, old_ssa);
        }

        uint64_t clear_mask = (~(0xFFFFULL << shift)) & mask;
        uint64_t mask_index = intern_constant(context, clear_mask);

        if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
        {
            return;
        }

        *context->ir_instruction_cursor
            = ((bal_instruction_t)OPCODE_AND << BAL_OPCODE_SHIFT_POSITION)
              | ((bal_instruction_t)old_ssa << BAL_SOURCE1_SHIFT_POSITION)
              | ((bal_instruction_t)mask_index << BAL_SOURCE2_SHIFT_POSITION);

        BAL_LOG_DEBUG(context->logger,
                      "  EMIT: v%lu = AND v%lu, c%lu (Mask: 0x%llX)",
                      context->instruction_count,
                      old_ssa,
                      mask_index & ~BAL_IS_CONSTANT_BIT_POSITION,
                      clear_mask);

        uint64_t cleared_ssa = context->instruction_count;
        (void)cleared_ssa; // Remove unused variable warning from release builds.

        // Advance cursor for the AND instruction.
        //
        context->ir_instruction_cursor++;
        context->bit_width_cursor++;
        context->instruction_count++;

        uint64_t value_index = intern_constant(context, value);

        // Source 1 is the result of the AND instruction.
        //
        uint64_t masked_ssa = context->instruction_count - 1;
        *context->ir_instruction_cursor
            = ((bal_instruction_t)OPCODE_ADD << BAL_OPCODE_SHIFT_POSITION)
              | ((bal_instruction_t)masked_ssa << BAL_SOURCE1_SHIFT_POSITION)
              | ((bal_instruction_t)value_index << BAL_SOURCE2_SHIFT_POSITION);

        BAL_LOG_DEBUG(context->logger,
                      "  EMIT: v%lu = ADD v%lu, c%lu (Val: 0x%llX)",
                      context->instruction_count,
                      cleared_ssa,
                      value_index & ~BAL_IS_CONSTANT_BIT_POSITION,
                      value);
    }
    else
    {
        uint64_t constant_index = intern_constant(context, value);

        if (BAL_UNLIKELY(context->status != BAL_SUCCESS))
        {
            return;
        }

        *context->ir_instruction_cursor
            = ((bal_instruction_t)OPCODE_CONST << BAL_OPCODE_SHIFT_POSITION)
              | ((bal_instruction_t)constant_index << BAL_SOURCE1_SHIFT_POSITION);

        BAL_LOG_DEBUG(context->logger,
                      "  EMIT: v%lu = CONST %lu (0x%llX)",
                      context->instruction_count,
                      constant_index & ~BAL_IS_CONSTANT_BIT_POSITION,
                      value);
    }

    // Only update the SSA map is not writing to XZR/WZR.
    //
    if (rd != 31)
    {
        context->source_variables[rd].current_ssa_index = context->instruction_count;
        BAL_LOG_DEBUG(
            (context)->logger, "  SSA UPDATE: X%lu -> v%lu", rd, context->instruction_count);
    }
    else
    {
        BAL_LOG_TRACE(context->logger, "    SSA NO-OP: Destination is XZR");
    }

    context->instruction_count++;
}
