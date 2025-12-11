#include "decoder.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s [hex_instruction]\n", argv[0]);
        return 1;
    }

    const char *input_string = argv[1];
    char       *end          = NULL;
    errno                    = 0;

    int           base  = 16;
    unsigned long value = strtoul(input_string, &end, base);
    if (ERANGE == errno)
    {
        fprintf(stderr, "Error: input value out of range.\n");
        return 1;
    }

    if (input_string == end)
    {
        fprintf(stderr, "Error: No hex digits found in input.\n");
        return 1;
    }

    if (*end != '\0')
    {
        fprintf(
            stderr, "Error: Trailing garbage characters found: '%s'\n", end);
        return 1;
    }

    if (value > UINT32_MAX)
    {
        fprintf(stderr,
                "Error: Value 0x%lX exceeds 32-bit instruction size.\n",
                value);
        return 1;
    }

    uint32_t instruction = (uint32_t)value;

    const bal_decoder_instruction_metadata_t* metadata
        = bal_decoder_arm64_decode(instruction);
    if (NULL == metadata)
    {
        printf("UNDEFINED\n");
    }
    else
    {
        printf("Mnemonic: %s - Mask: 0x%08X - Expected: 0x%08X\n", metadata->name, metadata->mask, metadata->expected);
    }
    return 0;
}
