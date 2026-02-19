#include "setup.h"
#include <inttypes.h>

static int
test_movz(test_context_t *context)
{
    int                        return_code = EXIT_FAILURE;
    const bal_register_index_t registers[] = {
        BAL_REGISTER_X0, BAL_REGISTER_X1, BAL_REGISTER_X15, BAL_REGISTER_X30, BAL_REGISTER_XZR,
    };
    const uint16_t immediates[] = { 0, 1, 0xFFFF, 0xAAAA, 0x5555, 0x1234 };
    const uint8_t  shifts[]     = { 0, 16, 32, 48 };

    size_t registers_count        = sizeof(registers) / sizeof(registers[0]);
    size_t immediates_count       = sizeof(immediates) / sizeof(immediates[0]);
    size_t shifts_count           = sizeof(shifts) / sizeof(shifts[0]);
    size_t assembler_buffer_index = 0;

    for (size_t r = 0; r < registers_count; ++r)
    {
        for (size_t i = 0; i < immediates_count; ++i)
        {
            for (size_t s = 0; s < shifts_count; ++s)
            {
                bal_emit_movz(&context->assembler, registers[r], immediates[i], shifts[s]);
                ++assembler_buffer_index;
            }
        }
    }

    bal_engine_translate(&context->engine,
                         &context->interface,
                         context->code_buffer,
                         context->assembler.offset * sizeof(uint32_t));
    bal_instruction_t *BAL_RESTRICT ir       = context->engine.instructions;
    size_t                          ir_index = 0;

    for (size_t r = 0; r < registers_count; ++r)
    {
        for (size_t i = 0; i < immediates_count; ++i)
        {
            for (size_t s = 0; s < shifts_count; ++s)
            {
                bal_opcode_t opcode = (ir[ir_index] >> BAL_OPCODE_SHIFT_POSITION);

                if (opcode != OPCODE_CONST)
                {
                    fprintf(stderr, "FAIL: IR Inst %zu is not CONST\n", ir_index);
                    goto end;
                }

                uint32_t pool_index
                    = (ir[ir_index] >> BAL_SOURCE1_SHIFT_POSITION) & BAL_SOURCE_MASK;
                uint64_t expected_immediate = (uint64_t)immediates[i] << shifts[s];
                uint64_t actual_immediate   = context->engine.constants[pool_index];

                if (expected_immediate != actual_immediate)
                {
                    fprintf(stderr,
                            "FAIL: ARM Inst %08X value mismatch. Expected %" PRIX64 ", Got %" PRIX64
                            "\n",
                            context->assembler.buffer[assembler_buffer_index],
                            expected_immediate,
                            actual_immediate);
                    fprintf(stderr, "   Pool Index: %u\n", pool_index);
                    goto end;
                }
                ++ir_index;
            }
        }
    }

    return_code = EXIT_SUCCESS;

end:
    return return_code;
}

BAL_TEST_MAIN(test_movz)

/*** end of file ***/
