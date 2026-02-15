#include "bal_decoder.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INSTRUCTION_TRACKER_SIZE 2048 // Not sure what exact value to pur here.

typedef struct
{
    const char *name;
    int         counter;
} instruction_tracker_t;

int compare_trackers(const void *, const void *);

int
main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [ARM64 binary file]\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    FILE       *file     = fopen(filepath, "rb");

    if (NULL == file)
    {
        (void)fprintf(stderr, "fopen() failed to open file.");
        return EXIT_FAILURE;
    }

    uint32_t              instruction                       = 0;
    size_t                count                             = 1;
    unsigned int          unknown_instructions              = 0;
    instruction_tracker_t tracker[INSTRUCTION_TRACKER_SIZE] = { 0 };

    for (;;)
    {
        size_t bytes_read = fread(&instruction, sizeof(instruction), count, file);

        if (0 == bytes_read)
        {
            break;
        }

        bool is_end_of_file = (feof(file) != 0);

        if (true == is_end_of_file)
        {
            break;
        }

        bool error_reading_file = (ferror(file) != 0);

        if (true == error_reading_file)
        {
            (void)fprintf(stderr, "Error reading binary file.");
        }

        const bal_decoder_instruction_metadata_t *metadata = bal_decode_arm64(instruction);

        if (NULL == metadata)
        {
            ++unknown_instructions;
        }
        else
        {
            for (size_t i = 0; i < INSTRUCTION_TRACKER_SIZE; ++i)
            {
                if (NULL == tracker[i].name)
                {
                    tracker[i].name = metadata->name;
                    ++tracker[i].counter;
                    break;
                }

                bool is_names_equivalent = (0 == strcmp(tracker[i].name, metadata->name));

                if (true == is_names_equivalent)
                {
                    ++tracker[i].counter;
                    break;
                }
            }
        }
    }

    qsort(tracker, INSTRUCTION_TRACKER_SIZE, sizeof(instruction_tracker_t), compare_trackers);

    (void)printf("Top 20 most common instructions:\n");

    for (size_t i = 0; i < 20; ++i)
    {
        (void)printf("Mnemonic: %s\n", tracker[i].name);

        if (NULL == tracker[i].name)
        {
            break;
        }
    }

    (void)printf("Unknowm instructions %u\n", unknown_instructions);
}

int
compare_trackers(const void *a, const void *b)
{
    return ((const instruction_tracker_t *)b)->counter
           - ((const instruction_tracker_t *)a)->counter;
    ;
}
