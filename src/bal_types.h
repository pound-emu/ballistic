/** @file bal_types.h
 *
 * @brief Defines types used by Ballistic.
 */     

#ifndef BALLISTIC_TYPES_H
#define BALLISTIC_TYPES_H

#include <stdint.h>

typedef uint64_t bal_instruction_t;

typedef enum
{
    OPCODE_CONST,
    OPCODE_MOV,
    OPCODE_ADD,
} bal_opcode_t;

#endif /* BALLISTIC_TYPES_H */   

/*** end of file ***/
