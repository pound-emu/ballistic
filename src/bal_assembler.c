#include "bal_assembler.h"
#include <stdbool.h>
#include <string.h>

static bool can_emit(bal_assembler_t *assembler);
static void emit_mov(bal_assembler_t *, const char *, uint32_t, uint16_t, uint8_t, uint32_t);

bal_error_t
bal_assembler_init(bal_assembler_t   *assembler,
                   void              *buffer,
                   const size_t       size,
                   const bal_logger_t logger)
{
    if (NULL == assembler)
    {
        BAL_LOG_ERROR(&logger, "Assembler struct is NULL.");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if (NULL == buffer)
    {
        BAL_LOG_ERROR(&logger, "Buffer is NULL.");
        return BAL_ERROR_INVALID_ARGUMENT;
    }

    if ((uintptr_t)buffer % 4 != 0)
    {
        BAL_LOG_ERROR(&logger, "Buffer %p is not 4-byte aligned.", buffer);
        return BAL_ERROR_MEMORY_ALIGNMENT;
    }

    assembler->buffer   = (uint32_t *)buffer;
    assembler->capacity = size;
    assembler->offset   = 0;
    assembler->logger   = logger;
    assembler->status   = BAL_SUCCESS;

    BAL_LOG_INFO(
        &logger, "Assembler initialized. Buffer: %p, Capacity: %zu instructions.", buffer, size);
    return BAL_SUCCESS;
}

void
bal_assembler_reset(bal_assembler_t *assembler)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    assembler->offset = 0;
    assembler->status = BAL_SUCCESS;
    (void)memset(assembler->buffer, 0, assembler->capacity * sizeof(uint32_t));
}

void
bal_emit_add_immediate(bal_assembler_t           *assembler,
                       const bal_register_index_t rd,
                       const uint8_t              rn,
                       const uint16_t             imm12,
                       const uint8_t              shift)
{
    if (NULL == assembler)
    {
        return;
    }

    if (assembler->status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->status != BAL_SUCCESS");
        return;
    }

    const bool can_emit_return_value = can_emit(assembler);

    if (false == can_emit_return_value)
    {
        return;
    }

    if (rd > 31)
    {
        BAL_LOG_ERROR(&assembler->logger, "Rd X%u out of range (0-31).", rd);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (shift != 0 && shift != 1)
    {
        BAL_LOG_ERROR(&assembler->logger, "%u is not a valid shift amount (0-1).", shift);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const uint32_t imm12_mask      = 0xFFF;
    const uint32_t rn_mask         = 0x1F;
    const uint32_t imm12_uint32    = imm12 & imm12_mask;
    const uint32_t rn_uint32       = rn & rn_mask;
    const uint32_t hard_coded_bits = 0x22U;
    const uint32_t shift_uint32    = shift;
    const uint32_t sf              = 1U;

    uint32_t instruction = 0;
    instruction |= sf << 31;
    instruction |= hard_coded_bits << 23;
    instruction |= shift_uint32 << 22;
    instruction |= imm12_uint32 << 10;
    instruction |= rn_uint32 << 5;
    instruction |= rd;

    const char *mnemonic = "ADD (Imm)";
    BAL_LOG_TRACE(&assembler->logger,
                  "[+0x%04zx] %08x %s X%u, #0x%04x, LSL #%u",
                  assembler->offset * sizeof(uint32_t),
                  instruction,
                  mnemonic,
                  rd,
                  imm12_uint32,
                  shift_uint32);

    (void)mnemonic;
    assembler->buffer[assembler->offset++] = instruction;
}

void
bal_emit_add_shifted_register(bal_assembler_t           *assembler,
                              const bal_register_index_t rd,
                              const bal_register_index_t rn,
                              const bal_register_index_t rm,
                              const uint8_t              shift,
                              const uint8_t              shift_type)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == assembler->buffer))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->buffer is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->status != BAL_SUCCESS");
        return;
    }

    const bool can_emit_return_value = can_emit(assembler);

    if (BAL_UNLIKELY(false == can_emit_return_value))
    {
        return;
    }

    if (BAL_UNLIKELY(rd > 31))
    {
        BAL_LOG_ERROR(&assembler->logger, "Rd X%u out of range (0-31).", rd);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(rn > 31))
    {
        BAL_LOG_ERROR(&assembler->logger, "Rn X%u out of range (0-31).", rn);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(rm > 31))
    {
        BAL_LOG_ERROR(&assembler->logger, "Rm X%u out of range (0-31).", rm);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(shift > 63))
    {
        BAL_LOG_ERROR(&assembler->logger, "%u is not a valid shift amount (0-63).", shift);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(shift_type > 2))
    {
        BAL_LOG_ERROR(&assembler->logger, "%u is not a valid shift type (0-2).", shift_type);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const uint32_t register_mask     = 0x1F;
    const uint32_t shift_mask        = 0x3F;
    const uint32_t shift_type_mask   = 0x2;
    const uint32_t rm_uint32         = rm & register_mask;
    const uint32_t rn_uint32         = rn & register_mask;
    const uint32_t rd_uint32         = rd & register_mask;
    const uint32_t shift_uint32      = shift & shift_mask;
    const uint32_t shift_type_uint32 = shift_type & shift_type_mask;

    const uint32_t sf          = 1U;
    const uint32_t opcode      = 0x0BU;
    uint32_t       instruction = 0;
    instruction |= sf << 31;
    instruction |= opcode << 24;
    instruction |= shift_type_uint32 << 22;
    instruction |= rm_uint32 << 16;
    instruction |= shift_uint32 << 10;
    instruction |= rn_uint32 << 5;
    instruction |= rd_uint32;

    const char *mnemonic = "ADD (Shifted Register)";
    BAL_LOG_TRACE(&assembler->logger,
                  "[+0X%04zx] %08x %s X%u, X%u, X%u, shift %u",
                  assembler->offset * sizeof(uint32_t),
                  instruction,
                  mnemonic,
                  rd,
                  rn,
                  rm,
                  shift);
    (void)mnemonic;
    assembler->buffer[assembler->offset++] = instruction;
}

void
bal_emit_b(bal_assembler_t *assembler, const int32_t offset)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == assembler->buffer))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->buffer is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const bool can_emit_return_value = can_emit(assembler);

    if (false == can_emit_return_value)
    {
        return;
    }

    if (assembler->status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->status != BAL_SUCCESS");
        return;
    }

    const uint32_t hard_coded_bits = 0x14000000;

    // WARNING: Cast to uint32_t safely applies bitwise masking to negative values.
    const uint32_t imm26       = (uint32_t)(offset / 4) & 0x03FFFFFF;
    const uint32_t instruction = hard_coded_bits | imm26;

    const char *mnemonic = "B";
    BAL_LOG_TRACE(&assembler->logger,
                  "[+0x%04zx] %08x %s #%d",
                  assembler->offset * sizeof(uint32_t),
                  instruction,
                  mnemonic,
                  offset);

    // WARNING: Prevents unsued local variable compiler warning.
    (void)mnemonic;

    assembler->buffer[assembler->offset++] = instruction;
}

void
bal_emit_br(bal_assembler_t *BAL_RESTRICT assembler, const bal_register_index_t rn)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == assembler->buffer))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->buffer is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const bool can_emit_return_status = can_emit(assembler);

    if (BAL_UNLIKELY(false == can_emit_return_status))
    {
        return;
    }

    if (BAL_UNLIKELY(rn > 31))
    {
        BAL_LOG_ERROR(&assembler->logger, "X%u out of range (0-31)", rn);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (assembler->status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->status != BAL_SUCCESS");
        return;
    }

    const uint32_t           hard_coded_bits = 0xD61F0000;
    const uint32_t           rn_shift        = 5;
    const uint32_t           instruction     = hard_coded_bits | rn << rn_shift;
    const char *BAL_RESTRICT mnemonic        = "BR";
    BAL_LOG_TRACE(&assembler->logger,
                  "[+0x%04zx] %08x %s X%u",
                  assembler->offset * sizeof(uint32_t),
                  instruction,
                  mnemonic,
                  rn);

    // WARNING: Prevents unsued local variable compiler warning.
    (void)mnemonic;

    assembler->buffer[assembler->offset++] = instruction;
}

void
bal_emit_sub_immediate(bal_assembler_t           *assembler,
                       const bal_register_index_t rd,
                       const uint8_t              rn,
                       const uint16_t             imm12,
                       const uint8_t              shift)
{
    if (NULL == assembler)
    {
        return;
    }

    if (assembler->status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->status != BAL_SUCCESS");
        return;
    }

    const bool can_emit_return_value = can_emit(assembler);

    if (false == can_emit_return_value)
    {
        return;
    }

    if (rd > 31)
    {
        BAL_LOG_ERROR(&assembler->logger, "Rd X%u out of range (0-31).", rd);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (shift != 0 && shift != 1)
    {
        BAL_LOG_ERROR(&assembler->logger, "%u is not a valid shift amount (0-1).", shift);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const uint32_t imm12_mask      = 0xFFF;
    const uint32_t rn_mask         = 0x1F;
    const uint32_t imm12_uint32    = imm12 & imm12_mask;
    const uint32_t rn_uint32       = rn & rn_mask;
    const uint32_t hard_coded_bits = 0xA2U;
    const uint32_t shift_uint32    = shift;
    const uint32_t sf              = 1U;

    uint32_t instruction = 0;
    instruction |= sf << 31;
    instruction |= hard_coded_bits << 23;
    instruction |= shift_uint32 << 22;
    instruction |= imm12_uint32 << 10;
    instruction |= rn_uint32 << 5;
    instruction |= rd;

    const char *mnemonic = "SUB (Imm)";
    BAL_LOG_TRACE(&assembler->logger,
                  "[+0x%04zx] %08x %s X%u, #0x%04x, LSL #%u",
                  assembler->offset * sizeof(uint32_t),
                  instruction,
                  mnemonic,
                  rd,
                  imm12_uint32,
                  shift_uint32);

    (void)mnemonic;
    assembler->buffer[assembler->offset++] = instruction;
}

void
bal_emit_movz(bal_assembler_t           *assembler,
              const bal_register_index_t rd,
              const uint16_t             imm,
              const uint8_t              shift)
{
    emit_mov(assembler, "MOVZ", rd, imm, shift, 0x2);
}

void
bal_emit_movk(bal_assembler_t           *assembler,
              const bal_register_index_t rd,
              const uint16_t             imm,
              const uint8_t              shift)
{
    emit_mov(assembler, "MOVK", rd, imm, shift, 0x3);
}

void
bal_emit_movn(bal_assembler_t           *assembler,
              const bal_register_index_t rd,
              const uint16_t             imm,
              const uint8_t              shift)
{
    emit_mov(assembler, "MOVN", rd, imm, shift, 0x0);
}

void
bal_emit_ret(bal_assembler_t *assembler, const bal_register_index_t rn)
{
    if (NULL == assembler)
    {
        return;
    }

    if (NULL == assembler->buffer)
    {
        BAL_LOG_ERROR(&assembler->logger, "assembler->buffer is NULL, aborting emission");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const bool can_emit_return_value = can_emit(assembler);

    if (false == can_emit_return_value)
    {
        return;
    }

    if (rn > 31)
    {
        BAL_LOG_ERROR(&assembler->logger, "X%u out of range (0-31).", rn);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (assembler->status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&assembler->logger, "assembler->status != BAL_SUCCESS, aborting emission");
        return;
    }

    const uint32_t hard_coded_bits = 0xD65F0000;
    const uint32_t rn_shift        = 5;
    const uint32_t instruction     = hard_coded_bits | rn << rn_shift;

    const char *mnemonic = "RET";
    BAL_LOG_TRACE(&assembler->logger,
                  "[+0x%04zx] %08x %s X%u",
                  assembler->offset * sizeof(uint32_t),
                  instruction,
                  mnemonic,
                  rn);

    // This function argument isn't used in the log trace above on release builds because the log
    // trace is optimized out, making the compiler mark this variable as unused.
    (void)mnemonic;

    assembler->buffer[assembler->offset++] = instruction;
}

static bool
can_emit(bal_assembler_t *assembler)
{
    if (assembler->offset >= assembler->capacity)
    {
        BAL_LOG_ERROR(&assembler->logger,
                      "Aborting function: Assembler Overflow. Capacity %zu reached.",
                      assembler->capacity);
        assembler->status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return false;
    }

    return true;
}

static void
emit_mov(bal_assembler_t *BAL_RESTRICT assembler,
         const char *BAL_RESTRICT      mnemonic,
         const uint32_t                rd,
         const uint16_t                imm,
         const uint8_t                 shift,
         const uint32_t                opcode)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(NULL == assembler->buffer))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->buffer is NULL");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler->status != BAL_SUCCESS");
        return;
    }

    const bool can_emit_return_value = can_emit(assembler);

    if (BAL_UNLIKELY(false == can_emit_return_value))
    {
        return;
    }

    if (BAL_UNLIKELY(rd > 31))
    {
        BAL_LOG_ERROR(&assembler->logger, "Rd X%u out of range (0-31).", rd);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    if (BAL_UNLIKELY(shift != 0 && shift != 16 && shift != 32 && shift != 48))
    {
        BAL_LOG_ERROR(&assembler->logger, "%u is not a valid shift amount (0, 16, 32, 48).", shift);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const uint32_t sf          = 1;
    const uint32_t hw          = shift / 16;
    uint32_t       instruction = 0;
    const uint32_t imm16       = imm;
    instruction |= sf << 31;
    instruction |= opcode << 29;
    instruction |= 0x25 << 23; // 0b100101
    instruction |= hw << 21;
    instruction |= imm16 << 5;
    instruction |= rd << 0;

    BAL_LOG_TRACE(&assembler->logger,
                  "[+0x%04zx] %08x %s X%u, #0x%04x, LSL #%u",
                  assembler->offset * sizeof(uint32_t),
                  instruction,
                  mnemonic,
                  rd,
                  imm,
                  shift);

    // This function argument isn't used in the log trace above on release builds because the log
    // trace is optimized out, making the compiler mark this variable as unused.
    (void)mnemonic;

    assembler->buffer[assembler->offset++] = instruction;
}

/*** end of file ***/
