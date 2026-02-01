#include "bal_assert.h"
#include "bal_engine.h"
#include "bal_errors.h"
#include "bal_memory.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

typedef struct
{
    bal_opcode_t opcode;
    uint32_t     source1;
    uint32_t     source2;
    uint32_t     source3;
    uint32_t     constant_value;
    bool         is_source1_constant;
    bool         is_source2_constant;
    bool         is_source3_constant;
} ir_instruction_t;

int                     tests_test_translation(void);
static ir_instruction_t unpack_ir_instruction(bal_instruction_t);
static inline bool      is_constant(uint32_t);

int
tests_test_translation(void)
{
    bal_allocator_t allocator = { 0 };
    bal_get_default_allocator(&allocator);
    bal_memory_interface_t interface = { 0 };

    // MOV X0, #42
    // MOV X0, #0
    //
    uint32_t    buffer[BUFFER_SIZE] = { 0xD2800540, 0xD2800000 };
    bal_error_t error = bal_memory_init_flat(&allocator, &interface, buffer, BUFFER_SIZE);

    if (error != BAL_SUCCESS)
    {
        (void)fprintf(
            stderr, "bal_memory_init_flat() failed (reason: %s).\n", bal_error_to_string(error));
        return EXIT_FAILURE;
    }

    bal_engine_t engine = { 0 };
    error               = bal_engine_init(&allocator, &engine);

    if (error != BAL_SUCCESS)
    {
        (void)fprintf(
            stderr, "bal_engine_init() failed (reason: %s).\n", bal_error_to_string(error));
        return EXIT_FAILURE;
    }

    error = bal_engine_translate(&engine, &interface, buffer, BUFFER_SIZE);

    if (error != BAL_SUCCESS)
    {
        (void)fprintf(
                stderr, "bal_engine_translate() failed (reason: %s).", bal_error_to_string(error));
        return EXIT_FAILURE;
    }

    ir_instruction_t expected[BUFFER_SIZE] = { {
                                                   .opcode              = OPCODE_CONST,
                                                   .source1             = 0,
                                                   .constant_value      = 42,
                                                   .is_source1_constant = true,
                                               },
                                               {
                                                   .opcode              = OPCODE_CONST,
                                                   .source1             = 1,
                                                   .constant_value      = 0,
                                                   .is_source1_constant = true,
                                               } };

    // Test IR emitter logic.
    //
    for (size_t i = 0; i < engine.instruction_count; ++i)
    {
        BAL_ASSERT_MSG(engine.instructions[i] != POISON_UNINITIALIZED_MEMORY,
                       "Inst %d: Reading uninitialized memory.",
                       i);

        ir_instruction_t actual = unpack_ir_instruction(engine.instructions[i]);

        BAL_ASSERT_MSG(actual.opcode == expected[i].opcode,
                       "Inst %d: Decoded and expected opcoded do not match.",
                       i);

        if (true == expected[i].is_source1_constant)
        {
            BAL_ASSERT_MSG(engine.constants[actual.source1] == expected[i].constant_value,
                           "Inst %d: Got constant %d, expected  %d",
                           i,
                           engine.constants[actual.source1],
                           expected[i].constant_value);
        }
        else
        {
            BAL_ASSERT_MSG(actual.source1 == expected[i].source1,
                           "Inst %d: %d != %d - Actual and expected source1 do not match.",
                           i,
                           actual.source1,
                           expected[i].source1);
        }
    }

    // TODO Test SSA State.

    return EXIT_SUCCESS;
}

static ir_instruction_t
unpack_ir_instruction(bal_instruction_t instruction)
{
    uint32_t opcode      = (instruction >> BAL_OPCODE_SHIFT_POSITION) & (BAL_OPCODE_SIZE - 1U);
    uint32_t source_mask = (BAL_SOURCE_SIZE - 1U) | BAL_IS_CONSTANT_BIT_POSITION;
    uint32_t source1     = (instruction >> BAL_SOURCE1_SHIFT_POSITION) & source_mask;
    uint32_t source2     = (instruction >> BAL_SOURCE2_SHIFT_POSITION) & source_mask;
    uint32_t source3     = instruction & source_mask;

    ir_instruction_t decoded = {
        .opcode = opcode,

        // Clears the IS_CONSTANT flag.
        //
        .source1 = source1 & ~(BAL_IS_CONSTANT_BIT_POSITION),
        .source2 = source2 & ~(BAL_IS_CONSTANT_BIT_POSITION),
        .source3 = source3 & ~(BAL_IS_CONSTANT_BIT_POSITION),

        .is_source1_constant = is_constant(source1),
        .is_source2_constant = is_constant(source2),
        .is_source3_constant = is_constant(source3),
    };

    return decoded;
}

static inline bool
is_constant(uint32_t source)
{
    if (0 == (source & BAL_IS_CONSTANT_BIT_POSITION))
    {
        return false;
    }

    return true;
}
