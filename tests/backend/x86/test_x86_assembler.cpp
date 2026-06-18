#include "gtest/gtest.h"

extern "C"
{
#include "../src/backend/x86/bal_x86_assembler.c"
}

class Backendx86Assembler : public testing::Test
{
protected:
    uint8_t                 buffer[1024]      = {};
    bal_executable_buffer_t executable_buffer = { buffer, buffer };
    bal_logger_t            logger            = {};
    bal_x86_assembler_t     assembler         = {};

    void SetUp() override
    {
        memset(&buffer, 0, sizeof(executable_buffer));
        memset(&logger, 0, sizeof(bal_logger_t));
        memset(&assembler, 0, sizeof(bal_x86_assembler_t));
        bal_logger_init_default(&logger);
        const bal_error_t error
            = bal_x86_assembler_init(&assembler, executable_buffer, sizeof(buffer), logger);
        ASSERT_EQ(error, error);
    }
};

TEST_F(Backendx86Assembler, Init_NullAssembler)
{
    EXPECT_EQ(bal_x86_assembler_init(nullptr, executable_buffer, 100, logger),
              BAL_ERROR_INVALID_ARGUMENT);
}

TEST_F(Backendx86Assembler, Init_NullBuffer)
{
    bal_x86_assembler_t local_assembler;
    executable_buffer.rw_pointer = nullptr;
    EXPECT_EQ(bal_x86_assembler_init(&local_assembler, executable_buffer, 100, logger),
              BAL_ERROR_INVALID_ARGUMENT);
    executable_buffer.rw_pointer = buffer;
    executable_buffer.rx_pointer = nullptr;
    EXPECT_EQ(bal_x86_assembler_init(&local_assembler, executable_buffer, 100, logger),
              BAL_ERROR_INVALID_ARGUMENT);
}

TEST_F(Backendx86Assembler, Init_ZeroSize)
{
    bal_x86_assembler_t local_assembler;
    EXPECT_EQ(bal_x86_assembler_init(&local_assembler, executable_buffer, 0, logger),
              BAL_ERROR_INVALID_ARGUMENT);
}

TEST_F(Backendx86Assembler, Init_Success)
{
    bal_x86_assembler_t local_assembler;
    EXPECT_EQ(bal_x86_assembler_init(&local_assembler, executable_buffer, 100, logger),
              BAL_SUCCESS);
    EXPECT_EQ(local_assembler.capacity, 100);
    EXPECT_EQ(local_assembler.offset, 0);
    EXPECT_EQ(local_assembler.status, BAL_SUCCESS);
    EXPECT_EQ(local_assembler.buffer, buffer);
}

TEST_F(Backendx86Assembler, Reset_NullAssembler)
{
    assembler.capacity = 100;
    assembler.offset   = 20;
    assembler.status   = BAL_ERROR_ALLOCATION_FAILED;
    bal_x86_assembler_reset(nullptr);
    EXPECT_EQ(assembler.capacity, 100);
    EXPECT_EQ(assembler.offset, 20);
    EXPECT_EQ(assembler.status, BAL_ERROR_ALLOCATION_FAILED);
}

TEST_F(Backendx86Assembler, Reset_Success)
{
    assembler.offset = 20;
    assembler.status = BAL_ERROR_ALLOCATION_FAILED;
    bal_x86_assembler_reset(&assembler);
    EXPECT_EQ(assembler.offset, 0);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
}

TEST_F(Backendx86Assembler, Internal_IsValidRegister)
{
    EXPECT_TRUE(is_valid_register(BAL_X86_RAX));
    EXPECT_TRUE(is_valid_register(BAL_X86_R15));
    EXPECT_FALSE(is_valid_register((bal_x86_register_t)(BAL_X86_RAX - 1)));
    EXPECT_FALSE(is_valid_register((bal_x86_register_t)(BAL_X86_R15 + 1)));
    EXPECT_FALSE(is_valid_register((bal_x86_register_t)999));
}

TEST_F(Backendx86Assembler, Internal_CanEmit_NullCheck)
{
    EXPECT_FALSE(can_emit(nullptr, 1));
}

TEST_F(Backendx86Assembler, Internal_CanEmit_BadStatus)
{
    assembler.status = BAL_ERROR_INVALID_ARGUMENT;
    EXPECT_FALSE(can_emit(&assembler, 1));
}

TEST_F(Backendx86Assembler, Internal_CanEmit_IntegerOverflow)
{
    assembler.offset = SIZE_MAX;
    EXPECT_FALSE(can_emit(&assembler, 1)); // Would wrap to 0
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
}

TEST_F(Backendx86Assembler, Internal_CanEmit_BufferExhausted)
{
    assembler.offset   = 1000;
    assembler.capacity = 1000;
    EXPECT_FALSE(can_emit(&assembler, 1));
}

TEST_F(Backendx86Assembler, Internal_Emit8_NullBuffer)
{
    emit8(nullptr, 0, 0xFF);
    EXPECT_EQ(buffer[0], 0);
}

TEST_F(Backendx86Assembler, Internal_Emit32_NullBuffer)
{
    emit32(nullptr, 0, 0xFFFFFFFF);
    EXPECT_EQ(buffer[0], 0);
}

TEST_F(Backendx86Assembler, Internal_Emit64_NullBuffer)
{
    emit64(nullptr, 0, 0xFFFFFFFFFFFFFFFF);
    EXPECT_EQ(buffer[0], 0);
}

// Test pass incorrect data to modrm/rex generators.
TEST_F(Backendx86Assembler, Internal_BitwiseMasking)
{
    emit_rex(assembler.buffer, &assembler.offset, 0xFF, 0xFF, 0xFF);

    // w=1, r=1, b=1 -> 0x40 | 8 | 4 | 1 = 0x4D
    //
    EXPECT_EQ(buffer[0], 0x4D);
    assembler.offset = 0;
    emit_modrm_register(
        assembler.buffer, &assembler.offset, (bal_x86_register_t)0xFF, (bal_x86_register_t)0xFF);

    // reg=7, rm=7 -> 0xC0 | 7 << 3 | 7 = 0xC0 | 0x38 | 0x07 = 0xFF
    //
    EXPECT_EQ(buffer[0], 0xFF);
    assembler.offset = 0;
    emit_modrm_memory_disp32_rbp(assembler.buffer, &assembler.offset, (bal_x86_register_t)0xFF);

    // reg=7 -> 0x8D | 7 >> 3 | 0x05 = 0x80 | 0x38 | 0x05 = 0xBD
    //
    EXPECT_EQ(buffer[0], 0xBD);
}

TEST_F(Backendx86Assembler, Public_NullContext_NoCrash)
{
    bal_x86_emit_add_mem64_rbp_offset_imm(nullptr, 0, 0);
    bal_x86_emit_add_r64_imm32(nullptr, BAL_X86_RAX, 0);
    bal_x86_emit_add_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RBX);
    bal_x86_emit_sub_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RBX);
    bal_x86_emit_and_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RAX);
    bal_x86_emit_jcc_rel32(nullptr, BAL_X86_COND_A, 0);
    bal_x86_emit_jmp_r64(nullptr, BAL_X86_RAX);
    bal_x86_emit_jmp_rel32(nullptr, 0);
    bal_x86_emit_load_r64_rbp_offset(nullptr, BAL_X86_RAX, 0);
    bal_x86_emit_store_r64_rbp_offset(nullptr, BAL_X86_RAX, 0);
    bal_x86_emit_mov_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RAX);
    bal_x86_emit_mov_r64_imm64(nullptr, BAL_X86_RAX, 0);
    bal_x86_emit_or_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RAX);
    bal_x86_emit_ret(nullptr);
    bal_x86_emit_setcc_mem8_rbp_offset(nullptr, BAL_X86_COND_E, 0);
    bal_x86_emit_push_r64(nullptr, BAL_X86_RAX);
    bal_x86_emit_pop_r64(nullptr, BAL_X86_RAX);
    SUCCEED();
}

TEST_F(Backendx86Assembler, Public_BadStatus_NoEmit)
{
    assembler.status = BAL_ERROR_INVALID_ARGUMENT;

    bal_x86_emit_add_mem64_rbp_offset_imm(&assembler, 0, 0);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_add_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_add_r64_imm32(&assembler, BAL_X86_RAX, 0);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_and_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_jcc_rel32(&assembler, BAL_X86_COND_A, 0);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_jmp_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_jmp_rel32(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_load_r64_rbp_offset(&assembler, BAL_X86_RAX, 0);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_mov_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_mov_r64_imm64(&assembler, BAL_X86_RAX, 0);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_or_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_pop_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_push_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_setcc_mem8_rbp_offset(&assembler, BAL_X86_COND_E, 0);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_store_r64_rbp_offset(&assembler, BAL_X86_RAX, 0);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_sub_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.offset, 0);

    bal_x86_emit_ret(&assembler);
    EXPECT_EQ(assembler.offset, 0);
}

TEST_F(Backendx86Assembler, Public_InvalidRegister_NoEmit)
{
    const auto bad_register = (bal_x86_register_t)99;
    bal_x86_emit_and_r64_r64(&assembler, bad_register, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_add_r64_imm32(&assembler, bad_register, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_add_r64_r64(&assembler, bad_register, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_add_r64_r64(&assembler, BAL_X86_RAX, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_sub_r64_r64(&assembler, bad_register, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_sub_r64_r64(&assembler, BAL_X86_RAX, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_and_r64_r64(&assembler, BAL_X86_RAX, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_jmp_r64(&assembler, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_load_r64_rbp_offset(&assembler, bad_register, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_mov_r64_imm64(&assembler, bad_register, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_store_r64_rbp_offset(&assembler, bad_register, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_mov_r64_r64(&assembler, bad_register, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_mov_r64_r64(&assembler, BAL_X86_RAX, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_mov_r64_imm64(&assembler, bad_register, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_or_r64_r64(&assembler, bad_register, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_or_r64_r64(&assembler, BAL_X86_RAX, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_push_r64(&assembler, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_pop_r64(&assembler, bad_register);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);
}

TEST_F(Backendx86Assembler, Public_FullBuffer_NoEmit)
{
    assembler.offset = sizeof(buffer);

    bal_x86_emit_add_mem64_rbp_offset_imm(&assembler, 0, 0x1);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_add_r64_imm32(&assembler, BAL_X86_RAX, 0x1);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_add_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_and_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_jcc_rel32(&assembler, BAL_X86_COND_A, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_jmp_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_jmp_rel32(&assembler, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_load_r64_rbp_offset(&assembler, BAL_X86_RAX, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_mov_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_mov_r64_imm64(&assembler, BAL_X86_RAX, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_or_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBP);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_pop_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_push_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_setcc_mem8_rbp_offset(&assembler, BAL_X86_COND_A, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_store_r64_rbp_offset(&assembler, BAL_X86_RAX, 0);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_sub_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_ret(&assembler);
    EXPECT_EQ(assembler.status, BAL_ERROR_INSTRUCTION_OVERFLOW);
    EXPECT_EQ(assembler.offset, sizeof(buffer));
}

TEST_F(Backendx86Assembler, Encode_Ret)
{
    bal_x86_emit_ret(&assembler);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 1);
    EXPECT_EQ(assembler.buffer[0], 0xC3);
}

TEST_F(Backendx86Assembler, Encode_Push_LowReg)
{
    bal_x86_emit_push_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 1);
    EXPECT_EQ(assembler.buffer[0], 0x50);
}

TEST_F(Backendx86Assembler, Encode_Push_HighReg)
{
    bal_x86_emit_push_r64(&assembler, BAL_X86_R15);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.buffer[0], 0x41); // REX.B
    EXPECT_EQ(assembler.buffer[1], 0x57); // 0x50 + 7
}

TEST_F(Backendx86Assembler, Encode_Pop_LowReg)
{
    bal_x86_emit_pop_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 1);
    EXPECT_EQ(assembler.buffer[0], 0x58);
}

TEST_F(Backendx86Assembler, Encode_Pop_HighReg)
{
    bal_x86_emit_pop_r64(&assembler, BAL_X86_R15);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 2);
    EXPECT_EQ(assembler.buffer[0], 0x41); // REX.B
    EXPECT_EQ(assembler.buffer[1], 0x5F); // 0x58 + 7
}

TEST_F(Backendx86Assembler, Encode_AddRegImm_LowReg_8Bit)
{
    bal_x86_emit_add_r64_imm32(&assembler, BAL_X86_RAX, 0x10);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 4);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x83); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xC0); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0x10); // Imm8
}

TEST_F(Backendx86Assembler, Encode_AddRegImm_HighReg_32Bit)
{
    bal_x86_emit_add_r64_imm32(&assembler, BAL_X86_R15, 0x11223344);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 7);
    EXPECT_EQ(assembler.buffer[0], 0x49); // REX.W | REX.B
    EXPECT_EQ(assembler.buffer[1], 0x81); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xC7); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0x44); // Imm32 LSB
    EXPECT_EQ(assembler.buffer[4], 0x33);
    EXPECT_EQ(assembler.buffer[5], 0x22);
    EXPECT_EQ(assembler.buffer[6], 0x11); // Imm32 MSB
}

TEST_F(Backendx86Assembler, Encode_AddMemRbp_8BitImm)
{
    bal_x86_emit_add_mem64_rbp_offset_imm(&assembler, 0x20, 10);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 8);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x83); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0x20); // Displacement LSB
    EXPECT_EQ(assembler.buffer[4], 0x00);
    EXPECT_EQ(assembler.buffer[5], 0x00);
    EXPECT_EQ(assembler.buffer[6], 0x00); // Displacement MSB
    EXPECT_EQ(assembler.buffer[7], 0x0A); // Imm8
}

TEST_F(Backendx86Assembler, Encode_AddMemRbp_32BitImm)
{
    bal_x86_emit_add_mem64_rbp_offset_imm(&assembler, 0x20, 500);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 11);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x81); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0x20); // Displacement LSB
    EXPECT_EQ(assembler.buffer[4], 0x00);
    EXPECT_EQ(assembler.buffer[5], 0x00);
    EXPECT_EQ(assembler.buffer[6], 0x00); // Displacement MSB
    EXPECT_EQ(assembler.buffer[7], 0xF4); // Imm32 LSB
    EXPECT_EQ(assembler.buffer[8], 0x01);
    EXPECT_EQ(assembler.buffer[9], 0x00);
    EXPECT_EQ(assembler.buffer[10], 0x00); // Imm32 MSB
}

TEST_F(Backendx86Assembler, Encode_AddMemRbp_NegativeOffset_SignExtension)
{
    bal_x86_emit_add_mem64_rbp_offset_imm(&assembler, -32, 10);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 8);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x83); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0xE0); // Displacement LSB
    EXPECT_EQ(assembler.buffer[4], 0xFF);
    EXPECT_EQ(assembler.buffer[5], 0xFF);
    EXPECT_EQ(assembler.buffer[6], 0xFF); // Displacement MSB
    EXPECT_EQ(assembler.buffer[7], 0x0A); // Imm8
}

TEST_F(Backendx86Assembler, Encode_AddRegToReg_LowLow)
{
    bal_x86_emit_add_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x03); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xC3); // ModRM
}

TEST_F(Backendx86Assembler, Encode_AddRegToReg_HighHigh)
{
    bal_x86_emit_add_r64_r64(&assembler, BAL_X86_R15, BAL_X86_R14);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x4D); // REX.W | REX.R | REX.B
    EXPECT_EQ(assembler.buffer[1], 0x03); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xFE); // ModRM
}

TEST_F(Backendx86Assembler, Encode_SubRegToReg_LowLow)
{
    bal_x86_emit_sub_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x2B); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xC3); // ModRM
}

TEST_F(Backendx86Assembler, Encode_SubRegToReg_HighHigh)
{
    bal_x86_emit_sub_r64_r64(&assembler, BAL_X86_R15, BAL_X86_R14);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x4D); // REX.W | REX.R | REX.B
    EXPECT_EQ(assembler.buffer[1], 0x2B); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xFE); // ModRM
}

TEST_F(Backendx86Assembler, Encode_AndRegToReg_LowLow)
{
    bal_x86_emit_and_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x23); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xC3); // ModRM: 0xC0 | 0 << 3 | 3
}

TEST_F(Backendx86Assembler, Encode_AndRegToReg_HighLow)
{
    bal_x86_emit_and_r64_r64(&assembler, BAL_X86_R15, BAL_X86_R14);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x4D); // REX.W | REX.R | REX.B
    EXPECT_EQ(assembler.buffer[1], 0x23); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xFE); // ModRM: 0xC0 | 7 << 3 | 6
}

TEST_F(Backendx86Assembler, Encode_Jcc_PositiveOffset)
{
    // Test JNE (Jump if Not Equal / Not Zero) -> Condition code 0x5
    bal_x86_emit_jcc_rel32(&assembler, BAL_X86_COND_NE, 0x11223344);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 6);
    EXPECT_EQ(assembler.buffer[0], 0x0F); // Prefix
    EXPECT_EQ(assembler.buffer[1], 0x85); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0x44); // Rel32 LSB
    EXPECT_EQ(assembler.buffer[3], 0x33);
    EXPECT_EQ(assembler.buffer[4], 0x22);
    EXPECT_EQ(assembler.buffer[5], 0x11); // Rel32 MSB
}

TEST_F(Backendx86Assembler, Encode_Jcc_NegativeOffset_SignExtension)
{
    // Test JL (Jump if Less) -> Condition  code 0xC
    bal_x86_emit_jcc_rel32(&assembler, BAL_X86_COND_L, -32);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 6);
    EXPECT_EQ(assembler.buffer[0], 0x0F); // Prefix
    EXPECT_EQ(assembler.buffer[1], 0x8C); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xE0); // Rel32 LSB
    EXPECT_EQ(assembler.buffer[3], 0xFF);
    EXPECT_EQ(assembler.buffer[4], 0xFF);
    EXPECT_EQ(assembler.buffer[5], 0xFF); // Rel32 MSB
}

TEST_F(Backendx86Assembler, Encode_Jmp_LowReg)
{
    bal_x86_emit_jmp_r64(&assembler, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 2);
    EXPECT_EQ(assembler.buffer[0], 0xFF);
    EXPECT_EQ(assembler.buffer[1], 0xE0); // 0xC0 | 4 << 3 | 0
}

TEST_F(Backendx86Assembler, Encode_Jmp_HighReg)
{
    bal_x86_emit_jmp_r64(&assembler, BAL_X86_R15);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x41); // REX.B
    EXPECT_EQ(assembler.buffer[1], 0xFF);
    EXPECT_EQ(assembler.buffer[2], 0xE7); // 0xC0 | 4 << 3 | 7
}

TEST_F(Backendx86Assembler, Encode_Jmp_Rel32)
{
    bal_x86_emit_jmp_rel32(&assembler, 0x11223344);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 5);
    EXPECT_EQ(assembler.buffer[0], 0xE9);
    EXPECT_EQ(assembler.buffer[1], 0x44);
    EXPECT_EQ(assembler.buffer[2], 0x33);
    EXPECT_EQ(assembler.buffer[3], 0x22);
    EXPECT_EQ(assembler.buffer[4], 0x11);
}

TEST_F(Backendx86Assembler, Encode_MovRegToReg_LowLow)
{
    bal_x86_emit_mov_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x8B); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xC3); // ModRM
}

TEST_F(Backendx86Assembler, Encode_MovRegToReg_HighHigh)
{
    bal_x86_emit_mov_r64_r64(&assembler, BAL_X86_R15, BAL_X86_R14);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x4D); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x8B); // OPCODE
    EXPECT_EQ(assembler.buffer[2], 0xFE); // ModRM
}

TEST_F(Backendx86Assembler, Encode_MovRegImm_LowReg)
{
    bal_x86_emit_mov_r64_imm64(&assembler, BAL_X86_RAX, 0x1122334455667788);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 10);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0xB8); // Opcode + 0
    EXPECT_EQ(assembler.buffer[2], 0x88); // Little Endian Imm64
    EXPECT_EQ(assembler.buffer[9], 0x11);
}

TEST_F(Backendx86Assembler, Encode_MovRegImm_HighReg)
{
    bal_x86_emit_mov_r64_imm64(&assembler, BAL_X86_R15, 0x1122334455667788);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 10);
    EXPECT_EQ(assembler.buffer[0], 0x49); // REX.W | REX.B
    EXPECT_EQ(assembler.buffer[1], 0xBF); // Opcode 0xB8 + 7
}

TEST_F(Backendx86Assembler, Encode_MovRegImm_LowReg_32Bit)
{
    bal_x86_emit_mov_r64_imm64(&assembler, BAL_X86_RAX, 0X11223344);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 5);
    EXPECT_EQ(assembler.buffer[0], 0xB8); // Opcode
    EXPECT_EQ(assembler.buffer[1], 0X44);
    EXPECT_EQ(assembler.buffer[2], 0x33);
    EXPECT_EQ(assembler.buffer[3], 0x22);
    EXPECT_EQ(assembler.buffer[4], 0x11);
}

TEST_F(Backendx86Assembler, Encode_MovRegImm_HighReg_32Bit)
{
    bal_x86_emit_mov_r64_imm64(&assembler, BAL_X86_R15, 0X11223344);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 6);
    EXPECT_EQ(assembler.buffer[0], 0x41); // REX.B
    EXPECT_EQ(assembler.buffer[1], 0xBF); // Opcode 0xB8 + 7
    EXPECT_EQ(assembler.buffer[2], 0X44);
    EXPECT_EQ(assembler.buffer[3], 0x33);
    EXPECT_EQ(assembler.buffer[4], 0x22);
    EXPECT_EQ(assembler.buffer[5], 0x11);
}

TEST_F(Backendx86Assembler, Encode_OrRegToReg_LowLow)
{
    bal_x86_emit_or_r64_r64(&assembler, BAL_X86_RAX, BAL_X86_RBX);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x0B); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xC3); // ModRM: 0xC0 | 0 << 3 | 3
}

TEST_F(Backendx86Assembler, Encode_OrRegToReg_HighHigh)
{
    bal_x86_emit_or_r64_r64(&assembler, BAL_X86_R15, BAL_X86_R14);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 3);
    EXPECT_EQ(assembler.buffer[0], 0x4D); // REX.W | REX.R | REX.B
    EXPECT_EQ(assembler.buffer[1], 0x0B); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0xFE); // ModRM: 0xC0 | 7 << 3 | 3
}

TEST_F(Backendx86Assembler, Encode_LoadRdp_PositiveOffset)
{
    bal_x86_emit_load_r64_rbp_offset(&assembler, BAL_X86_RAX, 0x10);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 7);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x8B); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM (Reg RAX, RM RBP, Disp32)
    EXPECT_EQ(assembler.buffer[3], 0x10); // Disp32 Little Endian
    EXPECT_EQ(assembler.buffer[4], 0x00);
    EXPECT_EQ(assembler.buffer[5], 0x00);
    EXPECT_EQ(assembler.buffer[6], 0x00);
}

TEST_F(Backendx86Assembler, Encode_LoadRdp_NegativeOffset_SignExtension)
{
    // Verify two's compliment correctly writes 0xFFFFFFF0.
    //
    bal_x86_emit_load_r64_rbp_offset(&assembler, BAL_X86_RAX, -16);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.buffer[3], 0xF0);
    EXPECT_EQ(assembler.buffer[4], 0xFF);
    EXPECT_EQ(assembler.buffer[5], 0xFF);
    EXPECT_EQ(assembler.buffer[6], 0xFF);
}

TEST_F(Backendx86Assembler, Encode_Setcc_PositiveOffset)
{
    // Test SETE (Set if Equal / Zero) -> Condition code 0x4.
    bal_x86_emit_setcc_mem8_rbp_offset(&assembler, BAL_X86_COND_E, 0x20);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 7);
    EXPECT_EQ(assembler.buffer[0], 0x0F); // Prefix
    EXPECT_EQ(assembler.buffer[1], 0x94); // SETE: 0x90 + 0x4
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0x20); // Disp32 LSB
    EXPECT_EQ(assembler.buffer[4], 0x00);
    EXPECT_EQ(assembler.buffer[5], 0x00);
    EXPECT_EQ(assembler.buffer[6], 0x00); // Disp32 MSB
}

TEST_F(Backendx86Assembler, Encode_Setcc_NegativeOffset_SignExtension)
{
    // Test SETB (Set if Below / Carry) -> Condition code 0x2.
    bal_x86_emit_setcc_mem8_rbp_offset(&assembler, BAL_X86_COND_B, -16);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 7);
    EXPECT_EQ(assembler.buffer[0], 0x0F); // Prefix
    EXPECT_EQ(assembler.buffer[1], 0x92); // SETB: 0x90 + 0x2
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0xF0); // Offset LSB
    EXPECT_EQ(assembler.buffer[4], 0xFF);
    EXPECT_EQ(assembler.buffer[5], 0xFF);
    EXPECT_EQ(assembler.buffer[6], 0xFF); // Offset MSB
}

TEST_F(Backendx86Assembler, Encode_StoreRdp_PositiveOffset)
{
    bal_x86_emit_store_r64_rbp_offset(&assembler, BAL_X86_RAX, 0x20);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 7);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x89); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM
    EXPECT_EQ(assembler.buffer[3], 0x20);
    EXPECT_EQ(assembler.buffer[4], 0x00);
    EXPECT_EQ(assembler.buffer[5], 0x00);
    EXPECT_EQ(assembler.buffer[6], 0x00);
}

TEST_F(Backendx86Assembler, Encode_StoreRdp_NegativeOffset_SignExtension)
{
    // Verify two's compliment correctly writes 0xFFFFFFE0.
    //
    bal_x86_emit_store_r64_rbp_offset(&assembler, BAL_X86_RAX, -32);
    EXPECT_EQ(assembler.status, BAL_SUCCESS);
    EXPECT_EQ(assembler.offset, 7);
    EXPECT_EQ(assembler.buffer[0], 0x48); // REX.W
    EXPECT_EQ(assembler.buffer[1], 0x89); // Opcode
    EXPECT_EQ(assembler.buffer[2], 0x85); // ModRM (Reg RAX, RM RBP, Disp32)
    EXPECT_EQ(assembler.buffer[3], 0xE0); // Little Endian LSB
    EXPECT_EQ(assembler.buffer[4], 0xFF);
    EXPECT_EQ(assembler.buffer[5], 0xFF);
    EXPECT_EQ(assembler.buffer[6], 0xFF); // Little Endian MSB
}

/*** end of file ***/
