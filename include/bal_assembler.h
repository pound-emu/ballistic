#ifndef BALLISTIC_ASSEMBLER_H
#define BALLISTIC_ASSEMBLER_H

#include "bal_errors.h"
#include <stddef.h>
#include <stdint.h>

#define BAL_REGISTER_X0  0
#define BAL_REGISTER_X1  1
#define BAL_REGISTER_X2  2
#define BAL_REGISTER_X3  3
#define BAL_REGISTER_X4  4
#define BAL_REGISTER_X5  5
#define BAL_REGISTER_X6  6
#define BAL_REGISTER_X7  7
#define BAL_REGISTER_X8  8
#define BAL_REGISTER_X9  9
#define BAL_REGISTER_X10 10
#define BAL_REGISTER_X11 11
#define BAL_REGISTER_X12 12
#define BAL_REGISTER_X13 13
#define BAL_REGISTER_X14 14
#define BAL_REGISTER_X15 15
#define BAL_REGISTER_X16 16
#define BAL_REGISTER_X17 17
#define BAL_REGISTER_X18 18
#define BAL_REGISTER_X19 19
#define BAL_REGISTER_X20 20
#define BAL_REGISTER_X21 21
#define BAL_REGISTER_X22 22
#define BAL_REGISTER_X23 23
#define BAL_REGISTER_X24 24
#define BAL_REGISTER_X25 25
#define BAL_REGISTER_X26 26
#define BAL_REGISTER_X27 27
#define BAL_REGISTER_X28 28
#define BAL_REGISTER_X29 29
#define BAL_REGISTER_X30 30
#define BAL_REGISTER_XZR 31

typedef struct
{
    uint32_t   *buffer;
    size_t      capacity;
    size_t      offset;
    bal_error_t status;
} bal_assembler_t;

bal_error_t bal_assembler_init(bal_assembler_t *assembler, void *buffer, size_t size);

void bal_emit_movz(bal_assembler_t *assembler, uint32_t rd, uint16_t imm, uint8_t shift);

void bal_emit_movk(bal_assembler_t *assembler, uint32_t rd, uint16_t imm, uint8_t shift);

void bal_emit_movn(bal_assembler_t *assembler, uint32_t rd, uint16_t imm, uint8_t shift);

#endif /* BALLISTIC_ASSEMBLER_H */

/*** end of file ***/
