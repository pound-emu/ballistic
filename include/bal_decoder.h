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
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*! @brief Represents static metadata associated with a specific ARM32
     * instruction. */
    typedef struct
    {

        /*! @brief The instruction mnemonic (e.g., "ADD", "LDR"). */
        const char *name;

        /*!
         * @brief The bitmask indicating which bits in the instruction word are
         * significant.
         *
         * @details 1 = significant bit, 0 = variable field (register,
         * immediate, etc.).
         */
        uint32_t mask;

        /*!
         * @brief The expected value of the instruction after applying the mask.
         * @details (instruction & mask) == expected.
         */
        uint32_t expected;
    } bal_decoder_instruction_metadata_t;

    /*!
     * @brief Decodes a raw ARM64 instruction.
     *
     * @details
     * Performs a hash lookup on the provided instruction word to find a
     * matching definition.
     *
     * @param[in] instruction The raw ARM64 machine code to decode.
     *
     * @return A pointer to the instruction metadata if a match is found.
     * @return `NULL` if the instruction is undefined or invalid.
     *
     * @post The returned pointer (if not null) points to static read-only
     * memory.
     */
    BAL_HOT const bal_decoder_instruction_metadata_t *bal_decoder_arm64_decode(
        const uint32_t instruction);

#ifdef __cplusplus
}
#endif
#endif // POUND_JIT_DECODER_ARM32_H
