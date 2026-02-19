#include "setup.h"
#include <inttypes.h>

static int
test_movk(test_context_t *context)
{
    int                        return_code = EXIT_FAILURE;
    const bal_register_index_t registers[] = {
        BAL_REGISTER_X0, BAL_REGISTER_X1, BAL_REGISTER_X15, BAL_REGISTER_X30, BAL_REGISTER_XZR,
    };
    const uint16_t immediates[] = { 0, 1, 0xFFFF, 0xAAAA, 0x5555, 0x1234 };
    const uint8_t  shifts[]     = { 0, 16, 32, 48 };

    size_t registers_count  = sizeof(registers) / sizeof(registers[0]);
    size_t immediates_count = sizeof(immediates) / sizeof(immediates[0]);
    size_t shifts_count     = sizeof(shifts) / sizeof(shifts[0]);

    for (size_t r = 0; r < registers_count; ++r)
    {
        for (size_t i = 0; i < immediates_count; ++i)
        {
            for (size_t s = 0; s < shifts_count; ++s)
            {
                bal_emit_movk(&context->assembler, registers[r], immediates[i], shifts[s]);
            }
        }
    }

    bal_engine_translate(&context->engine,
                         &context->interface,
                         context->code_buffer,
                         context->assembler.offset * sizeof(uint32_t));
    bal_instruction_t *ir_start  = context->engine.instructions;
    bal_instruction_t *ir_cursor = context->engine.instructions;

    for (size_t r = 0; r < registers_count; ++r)
    {
        for (size_t i = 0; i < immediates_count; ++i)
        {
            for (size_t s = 0; s < shifts_count; ++s)
            {
                // MOVK emits AND then ADD.

                bal_opcode_t opcode1 = (*ir_cursor >> BAL_OPCODE_SHIFT_POSITION);

                // The register is uninitialized so skip the instruction that loads it from memory.
                //
                if (OPCODE_GET_REGISTER == opcode1)
                {
                    ++ir_cursor;
                    opcode1 = (*ir_cursor >> BAL_OPCODE_SHIFT_POSITION);
                }

                if (opcode1 != OPCODE_AND)
                {
                    size_t ir_instruction_offset
                        = (size_t)((uintptr_t)ir_cursor - (uintptr_t)ir_start);
                    fprintf(stderr,
                            "FAIL: [+0x%04zx] %08llx Expected OPCODE_AND for MOVK mask\n",
                            ir_instruction_offset,
                            (unsigned long long)*ir_cursor);
                    goto end;
                }

                uint32_t ssa_index
                    = (*ir_cursor >> BAL_SOURCE2_SHIFT_POSITION) & BAL_SOURCE_MASK_WITH_FLAG;

                if (!(ssa_index & BAL_IS_CONSTANT_BIT_POSITION))
                {
                    size_t ir_instruction_offset
                        = (size_t)((uintptr_t)ir_cursor - (uintptr_t)ir_start);
                    fprintf(stderr,
                            "FAIL: [+0x%04zx] %08llx AND mask operand is not a constant.\n",
                            ir_instruction_offset,
                            (unsigned long long)*ir_cursor);
                    goto end;
                }

                uint64_t actual_mask
                    = context->engine.constants[ssa_index & ~BAL_IS_CONSTANT_BIT_POSITION];
                uint64_t expected_mask = ~(0xFFFFULL << shifts[s]);

                if (actual_mask != expected_mask)
                {
                    size_t ir_instruction_offset
                        = (size_t)((uintptr_t)ir_cursor - (uintptr_t)ir_start);
                    fprintf(stderr,
                            "FAIL:  [+0x%04zx] %08llx Shift: %d, Expected Mask: %" PRIX64
                            ", Actual Mask: %" PRIX64 "\n",
                            ir_instruction_offset,
                            (unsigned long long)*ir_cursor,
                            shifts[s],
                            expected_mask,
                            actual_mask);
                    goto end;
                }

                ++ir_cursor;

                // Verify ADD instruction.

                bal_opcode_t opcode2 = (*ir_cursor >> BAL_OPCODE_SHIFT_POSITION);

                if (opcode2 != OPCODE_ADD)
                {
                    size_t ir_instruction_offset
                        = (size_t)((uintptr_t)ir_cursor - (uintptr_t)ir_start);
                    fprintf(stderr,
                            "FAIL: [+0x%04zx] %08llx Expected OPCODE_ADD for MOVK value.\n",
                            ir_instruction_offset,
                            (unsigned long long)*ir_cursor);
                    goto end;
                }

                uint32_t pool_index = (*ir_cursor >> BAL_SOURCE2_SHIFT_POSITION) & BAL_SOURCE_MASK;
                uint64_t expected_immediate = (uint64_t)immediates[i] << shifts[s];
                uint64_t actual_immediate   = context->engine.constants[pool_index];

                if (expected_immediate != actual_immediate)
                {
                    size_t ir_instruction_offset
                        = (size_t)((uintptr_t)ir_cursor - (uintptr_t)ir_start);
                    fprintf(stderr,
                            "FAIL: [+0x%04zx] %08llx value mismatch. Expected %" PRIX64
                            ", Got %" PRIX64 "\n",
                            ir_instruction_offset,
                            (unsigned long long)*ir_cursor,
                            expected_immediate,
                            actual_immediate);
                    fprintf(stderr, "   Pool Index: %u\n", pool_index);
                    goto end;
                }
                ++ir_cursor;
            }
        }
    }

    return_code = EXIT_SUCCESS;

end:
    return return_code;
}

BAL_TEST_MAIN(test_movk)

/*** end of file ***/
