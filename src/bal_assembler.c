#include "bal_assembler.h"
#include <stdbool.h>

static void emit_mov(bal_assembler_t *, uint32_t, uint16_t, uint8_t, uint32_t);

bal_error_t
bal_assembler_init(bal_assembler_t *assembler, void *buffer, size_t size)
{
    if (NULL == assembler || NULL == buffer)
    {
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if ((uintptr_t)buffer % 4 != 0)
    {
        return BAL_ERROR_MEMORY_ALIGNMENT;
    }

    assembler->buffer   = (uint32_t *)buffer;
    assembler->capacity = size;
    assembler->offset   = 0;
    assembler->status   = BAL_SUCCESS;
    return BAL_SUCCESS;
}

void
bal_emit_movz(bal_assembler_t *assembler, uint32_t rd, uint16_t imm, uint8_t shift)
{
    emit_mov(assembler, rd, imm, shift, 0b10);
}

void
bal_emit_movk(bal_assembler_t *assembler, uint32_t rd, uint16_t imm, uint8_t shift)
{
    emit_mov(assembler, rd, imm, shift, 0b11);
}

void
bal_emit_movn(bal_assembler_t *assembler, uint32_t rd, uint16_t imm, uint8_t shift)
{
    emit_mov(assembler, rd, imm, shift, 0b00);
}

static inline bool
can_emit(bal_assembler_t *assembler)
{
    if (assembler->offset >= assembler->capacity)
    {
        assembler->status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return false;
    }

    return true;
}

static inline void
emit_mov(bal_assembler_t *assembler, uint32_t rd, uint16_t imm, uint8_t shift, uint32_t opcode)
{
    if (assembler->status != BAL_SUCCESS)
    {
        return;
    }

    bool can_emit_return_value = can_emit(assembler);

    if (false == can_emit_return_value)
    {
        return;
    }

    if (rd < 32)
    {
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (0 == shift || 16 == shift || 32 == shift || 48 == shift)
    {
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    uint32_t sf          = 1;
    uint32_t hw          = shift / 16;
    uint32_t instruction = 0;
    uint32_t imm16       = imm;
    instruction |= (sf << 31);
    instruction |= (opcode << 29);
    instruction |= (0b100101 << 23);
    instruction |= (hw << 21);
    instruction |= (imm16 << 5);
    instruction |= (rd << 0);

    assembler->buffer[assembler->offset++] = instruction;
}

/*** end of file ***/
