#include "bal_attributes.h"
#include "bal_engine.h"
#include "bal_logging.h"
#include "bal_memory.h"
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

int
main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s [ARM64 binary file]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filepath = argv[1];
    FILE       *file     = fopen(filepath, "rb");

    if (NULL == file)
    {
        (void)fprintf(stderr, "fopen() failed to open file.\n");
        return EXIT_FAILURE;
    }

    bal_allocator_t allocator = { 0 };
    bal_allocator_default_init(&allocator);
    bal_logger_t logger = { 0 };
    bal_logger_init_default(&logger);
    bal_memory_interface_t   interface           = { 0 };
    BAL_ALIGNED(16) uint32_t buffer[BUFFER_SIZE] = { 0 };
    bal_error_t              error
        = bal_flat_translation_interface_init(&allocator, &interface, buffer, BUFFER_SIZE, logger);

    if (error != BAL_SUCCESS)
    {
        (void)fprintf(stderr, "bal_flat_translation_interface_init() failed.\n");
        return EXIT_FAILURE;
    }

    bal_engine_t engine = { 0 };
    error               = bal_engine_init(&allocator, &engine, logger);

    if (error != BAL_SUCCESS)
    {
        (void)fprintf(stderr, "bal_engine_init() failed.\n");
        return EXIT_FAILURE;
    }

    for (;;)
    {
        const size_t              count       = 1;
        const bal_guest_address_t entry_point = 0x0;
        const size_t              bytes_read  = fread(buffer, sizeof(buffer), count, file);

        if (0 == bytes_read)
        {
            break;
        }

        const bool is_end_of_file = (feof(file) != 0);

        if (true == is_end_of_file)
        {
            break;
        }

        const bool error_reading_file = (ferror(file) != 0);

        if (true == error_reading_file)
        {
            (void)fprintf(stderr, "Error reading binary file.\n");
        }

        bal_guest_address_t guest_address_cursor = entry_point;

        while (guest_address_cursor < entry_point + bytes_read)
        {
            error = bal_engine_translate_tier2(
                &engine, &interface, &guest_address_cursor, BUFFER_SIZE);

            if (error != BAL_SUCCESS)
            {
                (void)fprintf(stderr, "bal_engine_translate_tier2() failed.\n");
                return EXIT_FAILURE;
            }

            bal_engine_reset(&engine);
        }
    }

    return EXIT_SUCCESS;
}
