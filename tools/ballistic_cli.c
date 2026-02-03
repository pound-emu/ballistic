#include "bal_engine.h"
#include "bal_memory.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 4096

int main (int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s [ARM64 binary file]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    if(argc == 2 && strcmp(argv[1], "--help")==0)
    fprintf(stderr, "Usage: %s [ARM64 binary file]\n", argv[0]);
    return EXIT_FAILURE;

    
    const char *filepath = argv[1];
    FILE       *file     = fopen(filepath, "rb");

    if (NULL == file) {
        (void)fprintf(stderr, "fopen() failed to open file.\n");
        return EXIT_FAILURE;
    }

    bal_allocator_t allocator = { 0 };
    bal_get_default_allocator(&allocator);
    bal_memory_interface_t interface           = { 0 };
    uint32_t               buffer[BUFFER_SIZE] = { 0 };
    bal_error_t            error
        = bal_memory_init_flat(&allocator, &interface, buffer, BUFFER_SIZE);

    if (error != BAL_SUCCESS) {
        (void)fprintf(stderr, "bal_memory_init_flat() failed.\n");
        return EXIT_FAILURE;
    }

    bal_engine_t engine = { 0 };
    error = bal_engine_init(&allocator, &engine);

    if (error != BAL_SUCCESS) {
        (void)fprintf(stderr, "bal_engine_init() failed.\n");
        return EXIT_FAILURE;
    }

    size_t count = 1;

    for (;;) {
        size_t bytes_read = fread(buffer, sizeof(buffer), count, file);

        if (0 == bytes_read) {
            break;
        }

        bool is_end_of_file = (feof(file) != 0);

        if (true == is_end_of_file) {
            break;
        }

        bool error_reading_file = (ferror(file) != 0);

        if (true == error_reading_file) {
            (void)fprintf(stderr, "Error reading binary file.\n");
        }
        
        error = bal_engine_translate(&engine, &interface, buffer);

        if (error != BAL_SUCCESS) {
            (void)fprintf(stderr, "bal_engine_translate() failed.\n");
            return EXIT_FAILURE;
        }

        bal_engine_reset(&engine);
        
    }

    return EXIT_SUCCESS;
}
