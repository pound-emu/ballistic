/** @file bal_engine.h
 *
 * @brief Manages resources while translating ARM blocks to Itermediate
 * Representation.
 */     

#ifndef BALLISTIC_ENGINE_H
#define BALLISTIC_ENGINE_H  

typedef struct
{
    instruction_t *instructions;
    uint8_t       *ssa_bit_widths;
    uint16_t       instruction_count;
} bal_engine_t;

#endif /* BALLISTIC_ENGINE_H */   

/*** end of file ***/
