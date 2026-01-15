/** @file bal_types.h
 *
 * @brief Defines types used by Ballistic.
 */     

#ifndef BALLISTIC_TYPES_H
#define BALLISTIC_TYPES_H

#include <stdint.h>


typedef uint64_t bal_instruction_t;
typedef uint64_t bal_guest_address_t;
typedef uint16_t bal_instruction_count_t;
typedef uint16_t bal_ssa_id_t;
typedef uint8_t  bal_bit_width_t;

typedef enum
{
    OPCODE_CONST,
    OPCODE_MOV,
    OPCODE_ADD,
    OPCODE_SUB,
    OPCODE_MUL,
    OPCODE_DIV,
    OPCODE_AND,
    OPCODE_XOR,
    OPCODE_OR_NOT,
    OPCODE_SHIFT,
    OPCODE_LOAD,
    OPCODE_STORE,
    OPCODE_JUMP,
    OPCODE_CALL,
    OPCODE_RETURN,
    OPCODE_BRANCH_ZERO,
    OPCODE_BRANCH_NOT_ZERO,
    OPCODE_TEST_BIT_ZERO,
    OPCODE_CMP,
    OPCODE_CMP_COND,
    OPCODE_TRAP,
    OPCODE_EMUM_END = 0x7FF, // Force enum to 2 bytes.
} bal_opcode_t;

#endif /* BALLISTIC_TYPES_H */   

/*** end of file ***/
