#include "backend/x86/bal_x86_assembler.h"
#include "bal_assert.h"
#include <stdbool.h>
#include <string.h>

BAL_HOT static bool is_valid_register(bal_x86_register_t reg);
BAL_HOT static bool can_emit(bal_x86_assembler_t *assembler, size_t size);
BAL_HOT static void emit8(uint8_t *buffer, size_t *offset, uint8_t value);
BAL_HOT static void emit32(uint8_t *buffer, size_t *offset, uint32_t value);
BAL_HOT static void emit64(uint8_t *buffer, size_t *offset, uint64_t value);

/// Emits the REX prefix.
/// w = 1 for 64-bit operands.
/// r = extension for the ModR/W `reg` field.
/// b = extension for the ModR/W `r/m` field or opcode register.
BAL_HOT static void emit_rex(uint8_t *buffer, size_t *offset, uint8_t w, uint8_t r, uint8_t b);

/// Emits the ModR/M byte for Register-to-Register operations.
BAL_HOT static void emit_modrm_register(uint8_t           *buffer,
                                        size_t            *offset,
                                        bal_x86_register_t reg,
                                        bal_x86_register_t rm);

/// Emits the ModR/M byte for Memory Addressing: [RBP + disp32].
BAL_HOT static void emit_modrm_memory_disp32_rbp(uint8_t           *buffer,
                                                 size_t            *offset,
                                                 bal_x86_register_t reg);

bal_error_t
bal_x86_assembler_init(bal_x86_assembler_t    *assembler,
                       bal_executable_buffer_t executable_buffer,
                       const size_t            size,
                       const bal_logger_t      logger)
{
    const bal_error_t error = BAL_ERROR_INVALID_ARGUMENT;

    if (BAL_UNLIKELY(NULL == assembler))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Assembler is NULL");
        return error;
    }

    if (BAL_UNLIKELY(NULL == executable_buffer.rx_pointer)
        || BAL_UNLIKELY(NULL == executable_buffer.rw_pointer))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Buffer is NULL");
        return error;
    }

    if (BAL_UNLIKELY(0 == size))
    {
        BAL_LOG_ERROR(&logger, "Aborting function: Size is 0");
        return error;
    }

    assembler->buffer    = (uint8_t *)executable_buffer.rw_pointer;
    assembler->rx_buffer = (uint8_t *)executable_buffer.rx_pointer;
    assembler->capacity  = size;
    assembler->offset    = 0;
    assembler->logger    = logger;
    assembler->status    = BAL_SUCCESS;

    BAL_LOG_INFO(&logger,
                 "x86 Assembler initialized. RW Buffer: %p, RX Buffer: %p, Capacity: %zu bytes",
                 executable_buffer.rw_pointer,
                 executable_buffer.rx_pointer,
                 size);
    return BAL_SUCCESS;
}

void
bal_x86_emit_and_r64_r64(bal_x86_assembler_t     *assembler,
                         const bal_x86_register_t destination,
                         const bal_x86_register_t source)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        return;
    }

    bool is_valid_register_result = is_valid_register(destination);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid destination register: %d", destination);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    is_valid_register_result = is_valid_register(source);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid source register: %d", destination);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const size_t instruction_size_bytes = 3;

    const bool can_emit_status = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(
        &assembler->logger, "[0x%04zx] and r%d, r%d", assembler->offset, destination, source);

    const uint8_t w          = 1;
    const uint8_t r          = (uint8_t)destination >> 3;
    const uint8_t b          = (uint8_t)source >> 3;
    const uint8_t opcode     = 0x23;
    const size_t  old_offset = assembler->offset;
    emit_rex(assembler->buffer, &assembler->offset, w, r, b);
    emit8(assembler->buffer, &assembler->offset, opcode);
    emit_modrm_register(assembler->buffer, &assembler->offset, destination, source);
    const size_t bytes_emitted = assembler->offset - old_offset;
    BAL_ASSERT_MSG(bytes_emitted == instruction_size_bytes,
                   "Bytes emitted %d does not match instruction size %d",
                   bytes_emitted,
                   instruction_size_bytes);
}

void
bal_x86_emit_load_r64_rbp_offset(bal_x86_assembler_t     *assembler,
                                 const bal_x86_register_t destination,
                                 const int32_t            offset)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler status != BAL_SUCCESS");
        return;
    }

    const bool is_valid_register_result = is_valid_register(destination);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid destination register: %d", destination);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const size_t instruction_size_bytes = 7;
    const bool   can_emit_status        = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(&assembler->logger,
                  "[0x%04zx] mov r%d, [rbp + 0x%X]",
                  assembler->offset,
                  destination,
                  offset);

    const uint8_t w          = 1;
    const uint8_t r          = (uint8_t)destination >> 3;
    const uint8_t b          = 0;
    const uint8_t opcode     = 0X8B;
    const size_t  old_offset = assembler->offset;
    emit_rex(assembler->buffer, &assembler->offset, w, r, b);
    emit8(assembler->buffer, &assembler->offset, opcode);
    emit_modrm_memory_disp32_rbp(assembler->buffer, &assembler->offset, destination);
    emit32(assembler->buffer, &assembler->offset, (uint32_t)offset);
    const size_t bytes_emitted = assembler->offset - old_offset;
    BAL_ASSERT_MSG(bytes_emitted == instruction_size_bytes,
                   "Bytes emitted %d does not match instruction size %d",
                   bytes_emitted,
                   instruction_size_bytes);
}

void
bal_x86_emit_store_r64_rbp_offset(bal_x86_assembler_t     *assembler,
                                  const bal_x86_register_t source,
                                  const int32_t            offset)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: assembler status != BAL_SUCCESS");
        return;
    }

    const bool is_valid_register_result = is_valid_register(source);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid source register: %d", source);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const size_t instruction_size_bytes = 7;
    const bool   can_emit_status        = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(
        &assembler->logger, "[0x%04zx] mov[rbp + 0x%X], r%d", assembler->offset, offset, source);
    const uint8_t w          = 1;
    const uint8_t r          = (uint8_t)source >> 3;
    const uint8_t b          = 0;
    const uint8_t opcode     = 0X89;
    const size_t  old_offset = assembler->offset;
    emit_rex(assembler->buffer, &assembler->offset, w, r, b);
    emit8(assembler->buffer, &assembler->offset, opcode);
    emit_modrm_memory_disp32_rbp(assembler->buffer, &assembler->offset, source);
    emit32(assembler->buffer, &assembler->offset, (uint32_t)offset);
    const size_t bytes_emitted = assembler->offset - old_offset;
    BAL_ASSERT_MSG(bytes_emitted == instruction_size_bytes,
                   "Bytes emitted %d does not match instruction size %d",
                   bytes_emitted,
                   instruction_size_bytes);
}

void
bal_x86_emit_mov_r64_r64(bal_x86_assembler_t     *assembler,
                         const bal_x86_register_t destination,
                         const bal_x86_register_t source)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Assembler status != BAL_SUCCESS, aborting function");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    bool is_valid_register_result = is_valid_register(source);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid source register: %d", source);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    is_valid_register_result = is_valid_register(destination);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid destination register: %d", destination);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const size_t instruction_size_bytes = 3;
    const bool   can_emit_status        = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(
        &assembler->logger, "[+0x%04zx] mov r%d, r%d", assembler->offset, destination, source);
    const uint8_t w          = 1;
    const uint8_t r          = (uint8_t)destination >> 3;
    const uint8_t b          = (uint8_t)source >> 3;
    const uint8_t opcode     = 0x8BU;
    const size_t  old_offset = assembler->offset;
    emit_rex(assembler->buffer, &assembler->offset, w, r, b);
    emit8(assembler->buffer, &assembler->offset, opcode);
    emit_modrm_register(assembler->buffer, &assembler->offset, destination, source);
    const size_t bytes_emitted = assembler->offset - old_offset;
    BAL_ASSERT_MSG(bytes_emitted == instruction_size_bytes,
                   "Bytes emitted %d does not match instruction size %d",
                   bytes_emitted,
                   instruction_size_bytes);
}

void
bal_x86_emit_mov_r64_imm64(bal_x86_assembler_t     *assembler,
                           const bal_x86_register_t destination,
                           const uint64_t           immediate)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Assembler status != BAL_SUCCESS, aborting function");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const bool is_valid_register_result = is_valid_register(destination);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid destination register: %d", destination);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const size_t instruction_size_bytes = 10;
    const bool   can_emit_status        = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(&assembler->logger,
                  "[+0x%04zx] mov r%d, 0x%llX",
                  assembler->offset,
                  destination,
                  (unsigned long long)immediate);
    const uint8_t w          = 1;
    const uint8_t r          = 0;
    const uint8_t b          = (uint8_t)destination >> 3;
    const size_t  old_offset = assembler->offset;
    emit_rex(assembler->buffer, &assembler->offset, w, r, b);
    emit8(assembler->buffer, &assembler->offset, 0xB8 + (destination & 7));
    emit64(assembler->buffer, &assembler->offset, immediate);
    const size_t bytes_emitted = assembler->offset - old_offset;
    BAL_ASSERT_MSG(bytes_emitted == instruction_size_bytes,
                   "Bytes emitted %d does not match instruction size %d",
                   bytes_emitted,
                   instruction_size_bytes);
}

void
bal_x86_emit_ret(bal_x86_assembler_t *assembler)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Assembler status != BAL_SUCCESS, aborting function");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const size_t instruction_size_bytes = 1;
    const bool   can_emit_status        = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(&assembler->logger, "[+0x%04zx] ret", assembler->offset);
    emit8(assembler->buffer, &assembler->offset, 0XC3);
}

void
bal_x86_emit_push_r64(bal_x86_assembler_t *assembler, const bal_x86_register_t reg)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Assembler status != BAL_SUCCESS, aborting function");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const bool is_valid_register_result = is_valid_register(reg);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid register: %d", reg);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    size_t     instruction_size_bytes = 1;
    const bool can_emit_status        = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(&assembler->logger, "[+0x%04zx] push r%d", assembler->offset, reg);
    const size_t old_offset = assembler->offset;

    if (reg > 7)
    {
        const uint8_t w = 0;
        const uint8_t r = 0;
        const uint8_t b = (uint8_t)reg >> 3;
        emit_rex(assembler->buffer, &assembler->offset, w, r, b);
        ++instruction_size_bytes;
    }

    const uint8_t opcode = 0x50;
    emit8(assembler->buffer, &assembler->offset, opcode + (reg & 7));
    const size_t bytes_emitted = assembler->offset - old_offset;
    BAL_ASSERT_MSG(bytes_emitted <= instruction_size_bytes,
                   "Bytes emitted %d does not match instruction size %d",
                   bytes_emitted,
                   instruction_size_bytes);
}

void
bal_x86_emit_pop_r64(bal_x86_assembler_t *assembler, const bal_x86_register_t reg)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Assembler status != BAL_SUCCESS, aborting function");
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    const bool is_valid_register_result = is_valid_register(reg);

    if (BAL_UNLIKELY(false == is_valid_register_result))
    {
        BAL_LOG_ERROR(&assembler->logger, "Invalid register: %d", reg);
        assembler->status = BAL_ERROR_INVALID_ARGUMENT;
        return;
    }

    size_t     instruction_size_bytes = 1;
    const bool can_emit_status        = can_emit(assembler, instruction_size_bytes);

    if (BAL_UNLIKELY(false == can_emit_status))
    {
        return;
    }

    BAL_LOG_DEBUG(&assembler->logger, "[+0x%04zx] pop r%d", assembler->offset, reg);
    const size_t old_offset = assembler->offset;

    if (reg > 7)
    {
        const uint8_t w = 0;
        const uint8_t r = 0;
        const uint8_t b = (uint8_t)reg >> 3;
        emit_rex(assembler->buffer, &assembler->offset, w, r, b);
        ++instruction_size_bytes;
    }

    const uint8_t opcode = 0x58;
    emit8(assembler->buffer, &assembler->offset, opcode + (reg & 7));
    const size_t bytes_emitted = assembler->offset - old_offset;
    BAL_ASSERT_MSG(bytes_emitted == instruction_size_bytes,
                   "Bytes emitted %d does not match instruction size %d",
                   bytes_emitted,
                   instruction_size_bytes);
}

bool
is_valid_register(const bal_x86_register_t reg)
{
    return reg >= BAL_X86_RAX && reg <= BAL_X86_R15;
}

static bool
can_emit(bal_x86_assembler_t *assembler, const size_t size)
{
    if (BAL_UNLIKELY(NULL == assembler))
    {
        return false;
    }

    if (BAL_UNLIKELY(assembler->status != BAL_SUCCESS))
    {
        BAL_LOG_ERROR(&assembler->logger, "Aborting function: Assembler status != BAL_SUCCESS");
        return false;
    }

    if (BAL_UNLIKELY(SIZE_MAX - assembler->offset < size))
    {
        BAL_LOG_ERROR(&assembler->logger,
                      "x86 Assembler Integer Overflow. Current offset: %zu, Requested size: %zu",
                      assembler->offset,
                      size);
        assembler->status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return false;
    }

    const size_t assembler_size = assembler->offset + size;

    if (assembler_size > assembler->capacity)
    {
        BAL_LOG_ERROR(&assembler->logger,
                      "x86 Assembler Overflow. Capacity %zu reached",
                      assembler->capacity);
        assembler->status = BAL_ERROR_INSTRUCTION_OVERFLOW;
        return false;
    }

    return true;
}

static void
emit8(uint8_t *buffer, size_t *offset, uint8_t const value)
{
    if (BAL_UNLIKELY(NULL == buffer))
    {
        return;
    }

    buffer += *offset;
    *buffer = value;
    *offset += 1;
}

static void
emit32(uint8_t *buffer, size_t *offset, const uint32_t value)
{
    if (BAL_UNLIKELY(NULL == buffer))
    {
        return;
    }

    buffer += *offset;
    memcpy(buffer, &value, sizeof(uint32_t));
    *offset += sizeof(uint32_t);
}

static void
emit64(uint8_t *buffer, size_t *offset, uint64_t const value)
{
    if (BAL_UNLIKELY(NULL == buffer))
    {
        return;
    }

    buffer += *offset;
    memcpy(buffer, &value, sizeof(uint64_t));
    *offset += sizeof(uint64_t);
}

static void
emit_rex(uint8_t *buffer, size_t *offset, const uint8_t w, const uint8_t r, const uint8_t b)
{
    const uint8_t safe_w = w & 1U;
    const uint8_t safe_r = r & 1U;
    const uint8_t safe_b = b & 1U;
    const uint8_t rex = (uint8_t)(0x40U | (unsigned)safe_w << 3U | (unsigned)safe_r << 2U | safe_b);
    emit8(buffer, offset, rex);
}

void
emit_modrm_register(uint8_t                 *buffer,
                    size_t                  *offset,
                    const bal_x86_register_t reg,
                    const bal_x86_register_t rm)
{
    const uint8_t safe_reg = (uint8_t)reg & 7;
    const uint8_t safe_rm  = (uint8_t)rm & 7U;
    const uint8_t modrm    = (uint8_t)(0xC0U | (unsigned)safe_reg << 3U | safe_rm);
    emit8(buffer, offset, modrm);
}

void
emit_modrm_memory_disp32_rbp(uint8_t *buffer, size_t *offset, const bal_x86_register_t reg)
{
    const uint8_t safe_reg = (uint8_t)reg & 7;
    uint8_t const modrm    = (uint8_t)(0x80U | (unsigned)safe_reg << 3U | 0x05U);
    emit8(buffer, offset, modrm);
}

/*** end of file ***/
