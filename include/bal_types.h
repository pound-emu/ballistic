/** @file bal_types.h
 *
 * @brief Defines types used by Ballistic.
 */     

#ifndef BALLISTIC_TYPES_H
#define BALLISTIC_TYPES_H

#include <stdint.h>

typedef uint64_t bal_instruction_t;
typedef uint16_t bal_instruction_count_t;
typedef uint16_t bal_ssa_id_t;
typedef uint8_t  bal_bit_width_t;

typedef enum
{
    OPCODE_CONST,
    OPCODE_MOV,
    OPCODE_ADD,
    OPCODE_EMUM_END = 0x7FF, // Force enum to 2 bytes.
} bal_opcode_t;

#endif /* BALLISTIC_TYPES_H */   

/*** end of file ***/
