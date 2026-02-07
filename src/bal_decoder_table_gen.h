/*
 * GENERATED FILE - DO NOT EDIT
 * Generated with tools/generate_a64_table.py
 */

#include "bal_decoder.h"
#include <stdint.h>
#include <stddef.h>

#define BAL_DECODER_ARM64_INSTRUCTIONS_SIZE 3608

typedef struct {
   uint16_t index;
    uint8_t  count;
} decoder_bucket_t;

extern const bal_decoder_instruction_metadata_t g_bal_decoder_arm64_instructions[BAL_DECODER_ARM64_INSTRUCTIONS_SIZE];
extern const decoder_bucket_t g_decoder_lookup_table[2048];

extern const bal_decoder_instruction_metadata_t *const g_decoder_hash_candidates[];

