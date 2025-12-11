/*
 *GENERATED FILE - DO NOT EDIT
 *Generated with tools/generate_a64_table.py
 */

#include "decoder.h"
#include <stdint.h>
#include <stddef.h>

#define BAL_DECODER_ARM64_INSTRUCTIONS_SIZE 5614

#define BAL_DECODER_ARM64_HASH_TABLE_BUCKET_SIZE 512U

typedef struct {
    const bal_decoder_instruction_metadata_t *instructions[512];
    size_t count;
} decoder_bucket_t;

extern const decoder_bucket_t g_decoder_lookup_table[4096];

extern const bal_decoder_instruction_metadata_t g_bal_decoder_arm64_instructions[BAL_DECODER_ARM64_INSTRUCTIONS_SIZE];
