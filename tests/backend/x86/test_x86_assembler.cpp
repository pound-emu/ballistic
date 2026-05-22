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
    bal_x86_emit_and_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RAX);
    bal_x86_emit_load_r64_rbp_offset(nullptr, BAL_X86_RAX, 0);
    bal_x86_emit_store_r64_rbp_offset(nullptr, BAL_X86_RAX, 0);
    bal_x86_emit_mov_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RAX);
    bal_x86_emit_mov_r64_imm64(nullptr, BAL_X86_RAX, 0);
    bal_x86_emit_or_r64_r64(nullptr, BAL_X86_RAX, BAL_X86_RAX);
    bal_x86_emit_ret(nullptr);
    bal_x86_emit_push_r64(nullptr, BAL_X86_RAX);
    bal_x86_emit_pop_r64(nullptr, BAL_X86_RAX);
    SUCCEED();
}

TEST_F(Backendx86Assembler, Public_BadStatus_NoEmit)
{
    assembler.status = BAL_ERROR_INVALID_ARGUMENT;
    bal_x86_emit_ret(&assembler);
    EXPECT_EQ(assembler.offset, 0);
}

TEST_F(Backendx86Assembler, Public_InvalidRegister_NoEmit)
{
    const auto bad_register = (bal_x86_register_t)99;
    bal_x86_emit_and_r64_r64(&assembler, bad_register, BAL_X86_RAX);
    EXPECT_EQ(assembler.status, BAL_ERROR_INVALID_ARGUMENT);

    assembler.status = BAL_SUCCESS;
    bal_x86_emit_and_r64_r64(&assembler, BAL_X86_RAX, bad_register);
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
