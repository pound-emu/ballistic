/**
 * @file bal_decoder.h
 * @brief ARM Instruction Decoder Interface.
 *
 * @details
 * This module provides the interface for decoding ARM instructions.
 */

#ifndef BAL_DECODER_H
#define BAL_DECODER_H

#include "bal_attributes.h"
#include "bal_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /// Represents static metadata aasociated with a specific ARM instruction.
    BAL_ALIGNED(32) typedef struct
    {
        /// The instruction mnemonic.
        const char *name;

        /// A bitmask indicating which bits in the instruction word are
        /// significant for decoding.
        ///
        /// A value of `1` indicates a fixed bit used for identification,
        /// while `0` indicates a variable field (e.g, imm, shamt, Rn).
        uint32_t mask;

        /// The expected pattern of the instruction after applying the `mask`.
        /// `(instruction & mask) == expected`.
        uint32_t expected;

        /// The IR opcode equivalent to this instruction's mnemonic.
        bal_opcode_t ir_opcode;

        /// Padding to maintain 32 byte alignment.
        char         _pad[8];
    } bal_decoder_instruction_metadata_t;

    /// Decodes a raw ARM64 instruction.
    ///
    /// Returns a pointer to [`bal_decoder_instruction_metadata_t`] describing
    /// the instruction if a match is found, or `NULL` if the instruction is
    /// invalid.
    ///
    /// # Safety
    ///
    /// The pointer refers to static readonly memory. It is valid for the
    /// lifetime of the program and must not be freed.
    BAL_HOT const bal_decoder_instruction_metadata_t *bal_decoder_arm64_decode(
        const uint32_t instruction);

#ifdef __cplusplus
}
#endif
#endif // POUND_JIT_DECODER_ARM32_H
