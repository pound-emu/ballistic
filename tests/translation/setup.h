#ifndef TEST_SETUP_H
#define TEST_SETUP_H

#include "bal_assembler.h"
#include "bal_attributes.h"
#include "bal_engine.h"
#include "bal_memory.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_BUFFER_SIZE 4096

typedef struct
{
    bal_allocator_t        allocator;
    bal_memory_interface_t interface;
    bal_assembler_t        assembler;
    bal_engine_t           engine;
    bal_logger_t           logger;
    uint32_t              *code_buffer;
} test_context_t;

static inline void
test_setup(test_context_t *context)
{
    bal_get_default_allocator(&context->allocator);
    bal_logger_init_default(&context->logger);
    context->logger.min_level = BAL_LOG_LEVEL_WARN;

    size_t memory_alignment = 16;
    context->code_buffer    = context->allocator.allocate(
        context->allocator.handle, memory_alignment, TEST_BUFFER_SIZE * sizeof(uint32_t));
    bal_memory_init_flat(&context->allocator,
                         &context->interface,
                         context->code_buffer,
                         TEST_BUFFER_SIZE * sizeof(uint32_t),
                         context->logger);
    bal_engine_init(&context->allocator, &context->engine, context->logger);
    bal_assembler_init(
        &context->assembler, context->code_buffer, TEST_BUFFER_SIZE, context->logger);
}

static inline void
test_teardown(test_context_t *context)
{
    bal_engine_destroy(&context->allocator, &context->engine);
    bal_memory_destroy_flat(&context->allocator, &context->interface);
    context->allocator.free(
        context->allocator.handle, context->code_buffer, TEST_BUFFER_SIZE * sizeof(uint32_t));
}

#define BAL_TEST_MAIN(test_function_name)        \
    int main(void)                               \
    {                                            \
        test_context_t context;                  \
        test_setup(&context);                    \
        int code = test_function_name(&context); \
        test_teardown(&context);                 \
        return code;                             \
    }

#endif // TEST_SETUP_H
