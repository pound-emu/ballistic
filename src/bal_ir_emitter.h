/** @file bal_ir_emitter.h
 *
 * @brief Responsible for encoding the IR into the memory arena.
 */

#ifndef BALLISTIC_IR_EMITTER_H
#define BALLISTIC_IR_EMITTER_H

#include "bal_attributes.h"
#include "bal_types.h"
#include "bal_errors.h"
#include "bal_engine.h"

/*!
 * @brief The maximum value for an Opcode.
 */
#define BAL_OPCODE_SIZE (1U << 11U)

/*!
 * @brief The maximum value for an Operand Index.
 * @note Bit 17 is reserved for the "Is Constant" flag.
 */
#define BAL_SOURCE_SIZE (1U << 16U)

/*!
 * @brief Appends a new instruction to the linear instruction stream.
 *
 * @param[in,out] engine    The JIT engine containing the instruction buffer.
 * @param[in]     opcode    The operation to perform (see @ref bal_opcode_t).
 * @param[in]     source1   SSA ID or Constant Pool Index for operand 1.
 * @param[in]     source2   SSA ID or Constant Pool Index for operand 2.
 * @param[in]     source3   SSA ID or Constant Pool Index for operand 3.
 * @param[in]     bit_width The bit width of the variable defined by this
 *                          instruction.
 *
 * @note Increments engine->instruction_count when called.
 *
 * @return BAL_SUCCESS on success.
 * @return BAL_ERROR_INSTRUCTION_OVERFLOW if the block limit is reached.
 * @return BAL_ERROR_ENGINE_STATE_INVALID if engine->status != BAL_SUCCESS.
 */
BAL_HOT bal_error_t emit_instruction(bal_engine_t   *engine,
                                     uint32_t        opcode,
                                     uint32_t        source1,
                                     uint32_t        source2,
                                     uint32_t        source3,
                                     bal_bit_width_t bit_width);

#endif /* BALLISTIC_IR_EMITTER_H */

/*** end of file ***/
