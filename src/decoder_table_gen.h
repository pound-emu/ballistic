/*
 *GENERATED FILE - DO NOT EDIT
 *Generated with tools/generate_a64_table.py
 */

#include "bal_decoder.h"
#include <stdint.h>
#include <stddef.h>

#define BAL_DECODER_ARM64_INSTRUCTIONS_SIZE 3945

typedef struct {
    const bal_decoder_instruction_metadata_t *const *instructions;
    size_t count;
} decoder_bucket_t;

extern const decoder_bucket_t g_decoder_lookup_table[2048];

extern const bal_decoder_instruction_metadata_t g_bal_decoder_arm64_instructions[BAL_DECODER_ARM64_INSTRUCTIONS_SIZE];
