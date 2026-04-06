#include "bal_logging.h"
#include "bal_memory.h"
#include <stdlib.h>

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

#define TEST_BUFFER_SIZE       4096
#define TEST_BUFFER_SIZE_BYTES (TEST_BUFFER_SIZE * sizeof(uint32_t))

typedef struct
{
    bal_allocator_t        allocator;
    bal_memory_interface_t interface;
    bal_logger_t           logger;
    uint32_t              *code_buffer;
} test_context_t;

static void
test_setup(test_context_t *context)
{
    bal_allocator_default_init(&context->allocator);
    bal_logger_init_default(&context->logger);
    context->logger.min_level = BAL_LOG_LEVEL_ERROR;
}

static void
test_teardown(test_context_t *context)
{
    bal_flat_translation_interface_destroy(&context->allocator, &context->interface);
    context->allocator.free(
        context->allocator.handle, context->code_buffer, TEST_BUFFER_SIZE_BYTES);
}

#define BAL_TEST_FUNCTION(test_function_name)          \
    do                                                 \
    {                                                  \
        test_setup(&context);                          \
        return_value = (test_function_name(&context)); \
        test_teardown(&context);                       \
        context.code_buffer = NULL;                    \
                                                       \
        if (return_value != EXIT_SUCCESS)              \
        {                                              \
            return return_value;                       \
        }                                              \
    } while (0)

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static int test_memory__default_allocate__invalid_size_returns_nullptr(test_context_t *context);
static int test_memory__default_flat_translation_init__success(test_context_t *context);
static int test_memory__default_flat_translation_init__invalid_arguments_returns_error(
    test_context_t *context);
static int test_memory__flat_translation_interface_init__failed_interface_allocation_returns_error(
    test_context_t *context);
static int tests_memory__default_flat_translation_interface_destroy__invalid_argument_returns_error(
    test_context_t *context);
static int test_memory__flat_translation_interface_translate__invalid_arguments_returns_error(
    test_context_t *);
static int test_memory__flat_translation_interface_translate__returns_valid_pointer_within_bounds(
    test_context_t *);
static int test_memory__flat_translation_interface__out_of_bounds_returns_null(test_context_t *);
static int test_memory__flat_translation_interface__unaligned_guest_address_returns_correct_offset(
    test_context_t *);
static void *empty_allocate(bal_allocator_handle_t allocator, size_t alignment, size_t size);

int
main(void)
{
    test_context_t context      = { 0 };
    int            return_value = EXIT_SUCCESS;
    BAL_TEST_FUNCTION(test_memory__default_allocate__invalid_size_returns_nullptr);
    BAL_TEST_FUNCTION(test_memory__default_flat_translation_init__success);
    BAL_TEST_FUNCTION(test_memory__default_flat_translation_init__invalid_arguments_returns_error);
    BAL_TEST_FUNCTION(
        test_memory__flat_translation_interface_init__failed_interface_allocation_returns_error);
    BAL_TEST_FUNCTION(
        tests_memory__default_flat_translation_interface_destroy__invalid_argument_returns_error);
    BAL_TEST_FUNCTION(
        test_memory__flat_translation_interface_translate__invalid_arguments_returns_error);
    BAL_TEST_FUNCTION(
        test_memory__flat_translation_interface_translate__returns_valid_pointer_within_bounds);
    BAL_TEST_FUNCTION(test_memory__flat_translation_interface__out_of_bounds_returns_null);
    BAL_TEST_FUNCTION(
        test_memory__flat_translation_interface__unaligned_guest_address_returns_correct_offset);
    return return_value;
}

static int
test_memory__default_allocate__invalid_size_returns_nullptr(test_context_t *context)
{
    int          return_code      = EXIT_SUCCESS;
    const size_t size             = 0;
    const size_t memory_alignment = 8;
    context->code_buffer
        = context->allocator.allocate(context->allocator.handle, memory_alignment, size);

    if (context->code_buffer != NULL)
    {
        BAL_LOG_ERROR(&context->logger, "Expected buffer == NULL");
        return_code = EXIT_FAILURE;
    }

    return return_code;
}

static int
test_memory__default_flat_translation_init__success(test_context_t *context)
{
    const size_t memory_alignment = 16U;
    context->code_buffer          = context->allocator.allocate(
        context->allocator.handle, memory_alignment, TEST_BUFFER_SIZE_BYTES);

    if (context->code_buffer == NULL)
    {
        BAL_LOG_ERROR(&context->logger, "Expected code buffer != NULL");
        return EXIT_FAILURE;
    }

    const bal_error_t error = bal_flat_translation_interface_init(&context->allocator,
                                                                  &context->interface,
                                                                  context->code_buffer,
                                                                  TEST_BUFFER_SIZE_BYTES,
                                                                  context->logger);
    context->allocator.free(
        context->allocator.handle, context->code_buffer, TEST_BUFFER_SIZE_BYTES);
    context->code_buffer = NULL;

    if (error != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&context->logger, "Expected BAL_SUCCESS");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int
test_memory__default_flat_translation_init__invalid_arguments_returns_error(test_context_t *context)
{
    int return_code = EXIT_SUCCESS;
    for (int i = 0; i < 1; ++i)
    {
        bal_logger_t logger = { 0 };
        bal_logger_init_default(&logger);

        // Disable logging because Ballistic will output error logs (which is a good thing since
        // we're testing errors).
        //
        logger.min_level = BAL_LOG_LEVEL_NONE;

        bal_allocator_t       *valid_allocator         = &context->allocator;
        bal_memory_interface_t valid_interface         = { 0 };
        const size_t           valid_memory_alignment  = 16;
        const uint32_t         valid_buffer_size_bytes = TEST_BUFFER_SIZE_BYTES;
        uint32_t              *valid_code_buffer       = context->allocator.allocate(
            context->allocator.handle, valid_memory_alignment, valid_buffer_size_bytes);

        bal_allocator_t        *invalid_allocator         = NULL;
        bal_memory_interface_t *invalid_interface         = NULL;
        uint32_t               *invalid_code_buffer       = NULL;
        const uint32_t          invalid_buffer_size_bytes = 0;

        bal_error_t error = bal_flat_translation_interface_init(invalid_allocator,
                                                                &valid_interface,
                                                                valid_code_buffer,
                                                                valid_buffer_size_bytes,
                                                                logger);

        if (error != BAL_ERROR_INVALID_ARGUMENT)
        {
            logger.min_level = BAL_LOG_LEVEL_ERROR;
            BAL_LOG_ERROR(&logger, "Expected BAL_ERROR_INVALID_ARGUMENT");
            return_code = EXIT_FAILURE;
            break;
        }

        error = bal_flat_translation_interface_init(
            valid_allocator, invalid_interface, valid_code_buffer, valid_buffer_size_bytes, logger);

        if (error != BAL_ERROR_INVALID_ARGUMENT)
        {
            logger.min_level = BAL_LOG_LEVEL_ERROR;
            BAL_LOG_ERROR(&logger, "Expected BAL_ERROR_INVALID_ARGUMENT");
            return_code = EXIT_FAILURE;
            break;
        }

        error = bal_flat_translation_interface_init(valid_allocator,
                                                    &valid_interface,
                                                    invalid_code_buffer,
                                                    valid_buffer_size_bytes,
                                                    logger);

        if (error != BAL_ERROR_INVALID_ARGUMENT)
        {
            logger.min_level = BAL_LOG_LEVEL_ERROR;
            BAL_LOG_ERROR(&logger, "Expected BAL_ERROR_INVALID_ARGUMENT");
            return_code = EXIT_FAILURE;
            break;
        }
        error = bal_flat_translation_interface_init(valid_allocator,
                                                    &valid_interface,
                                                    valid_code_buffer,
                                                    invalid_buffer_size_bytes,
                                                    logger);

        if (error != BAL_ERROR_INVALID_ARGUMENT)
        {
            logger.min_level = BAL_LOG_LEVEL_ERROR;
            BAL_LOG_ERROR(&logger, "Expected BAL_ERROR_INVALID_ARGUMENT");
            return_code = EXIT_FAILURE;
            break;
        }

        invalid_code_buffer = context->allocator.allocate(
            context->allocator.handle, valid_memory_alignment, valid_buffer_size_bytes);

        if (invalid_code_buffer == NULL)
        {
            BAL_LOG_ERROR(&context->logger, "Failed to allocate code buffer");
            return_code = EXIT_FAILURE;
            break;
        }

        invalid_code_buffer += 1;
        valid_allocator->allocate = empty_allocate;
        error                     = bal_flat_translation_interface_init(valid_allocator,
                                                    &valid_interface,
                                                    invalid_code_buffer,
                                                    valid_buffer_size_bytes,
                                                    logger);

        if (error != BAL_ERROR_MEMORY_ALIGNMENT)
        {
            BAL_LOG_ERROR(&logger, "Expected BAL_ERROR_MEMORY_ALIGNMENT");
            return_code = EXIT_FAILURE;
            break;
        }
    }

    return return_code;
}

static int
test_memory__flat_translation_interface_init__failed_interface_allocation_returns_error(
    test_context_t *context)
{
    int          return_code = EXIT_SUCCESS;
    bal_logger_t logger      = { 0 };
    bal_logger_init_default(&logger);
    logger.min_level = BAL_LOG_LEVEL_NONE;

    const size_t memory_alignment = 16;
    context->code_buffer          = context->allocator.allocate(
        context->allocator.handle, memory_alignment, TEST_BUFFER_SIZE_BYTES);
    context->allocator.allocate = empty_allocate;
    const bal_error_t error     = bal_flat_translation_interface_init(
        &context->allocator, &context->interface, context->code_buffer, memory_alignment, logger);

    if (error != BAL_ERROR_ALLOCATION_FAILED)
    {
        logger.min_level = BAL_LOG_LEVEL_ERROR;
        BAL_LOG_ERROR(&logger, "Expected BAL_ERROR_ALLOCATION_FAILED");
        return_code = EXIT_FAILURE;
    }

    return return_code;
}

static int
tests_memory__default_flat_translation_interface_destroy__invalid_argument_returns_error(
    test_context_t *context)
{
    int return_code = EXIT_SUCCESS;
    for (int i = 0; i < 1; ++i)
    {
        bal_allocator_t        *valid_allocator   = &context->allocator;
        bal_memory_interface_t  valid_interface   = { 0 };
        bal_allocator_t        *invalid_allocator = NULL;
        bal_memory_interface_t *invalid_interface = NULL;
        bal_error_t             error
            = bal_flat_translation_interface_destroy(valid_allocator, invalid_interface);
        if (error != BAL_ERROR_INVALID_ARGUMENT)
        {
            BAL_LOG_ERROR(&context->logger, "Expected BAL_ERROR_INVALID_ARGUMENT");
            return_code = EXIT_FAILURE;
            break;
        }

        error = bal_flat_translation_interface_destroy(invalid_allocator, &valid_interface);

        if (error != BAL_ERROR_INVALID_ARGUMENT)
        {
            BAL_LOG_ERROR(&context->logger, "Expected BAL_ERROR_INVALID_ARGUMENT");
            return_code = EXIT_FAILURE;
            break;
        }
    }

    return return_code;
}

static int
test_memory__flat_translation_interface_translate__invalid_arguments_returns_error(
    test_context_t *context)
{
    int return_code = EXIT_SUCCESS;
    bal_flat_translation_interface_init(&context->allocator,
                                        &context->interface,
                                        &context->code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context->logger);

    for (size_t i = 0; i < 1; ++i)
    {
        bal_memory_interface_t   *valid_interface        = &context->interface;
        size_t                    valid_max_size_bytes   = 0;
        bal_memory_interface_t   *invalid_interface      = NULL;
        size_t                   *invalid_max_size_bytes = NULL;
        const bal_guest_address_t guest_address          = 0x0;

        const uint8_t *host_address
            = context->interface.translate(invalid_interface, guest_address, &valid_max_size_bytes);
        if (host_address != NULL)
        {
            BAL_LOG_ERROR(&context->logger,
                          "Expected translated GVA 0x%llX to be NULL",
                          (unsigned long long)guest_address);
            return_code = EXIT_FAILURE;
            break;
        }

        host_address
            = context->interface.translate(valid_interface, guest_address, invalid_max_size_bytes);

        if (host_address != NULL)
        {
            BAL_LOG_ERROR(&context->logger,
                          "Expected translated GVA 0x%llX to be NULL",
                          (unsigned long long)guest_address);
            return_code = EXIT_FAILURE;
        }
    }

    return return_code;
}

static int
test_memory__flat_translation_interface_translate__returns_valid_pointer_within_bounds(
    test_context_t *context)
{
    int return_code = EXIT_SUCCESS;
    bal_flat_translation_interface_init(&context->allocator,
                                        &context->interface,
                                        &context->code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context->logger);
    const bal_guest_address_t valid_gvas[4]           = { 0x0, 0xF, 0x100, 0xFFC };
    size_t                    max_readable_size_bytes = 0;

    for (size_t i = 0; i < 4; ++i)
    {
        const uint8_t *host_address = context->interface.translate(
            &context->interface, valid_gvas[i], &max_readable_size_bytes);

        if (NULL == host_address)
        {
            BAL_LOG_ERROR(&context->logger,
                          "Expected translated GVA 0x%llX to return valid host address valid "
                          "host address.",
                          (unsigned long long)valid_gvas[i]);
            return_code = EXIT_FAILURE;
            break;
        }
    }

    return return_code;
}

static int
test_memory__flat_translation_interface__out_of_bounds_returns_null(test_context_t *context)
{
    int return_code = EXIT_SUCCESS;
    bal_flat_translation_interface_init(&context->allocator,
                                        &context->interface,
                                        &context->code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context->logger);
    const bal_guest_address_t invalid_gvas[3]
        = { TEST_BUFFER_SIZE, TEST_BUFFER_SIZE + 0x1000, 0xFFFFFFFFFFFFFFFF };
    size_t max_readable_size_bytes = 0;

    for (size_t i = 0; i < 3; ++i)
    {
        const uint8_t *host_address = context->interface.translate(
            &context->interface, invalid_gvas[i], &max_readable_size_bytes);

        if (host_address != NULL)
        {
            BAL_LOG_ERROR(&context->logger,
                          "Expected translated GVA 0x%llX to be NULL",
                          (unsigned long long)invalid_gvas[i]);
            return_code = EXIT_FAILURE;
            break;
        }
    }

    return return_code;
}

static int
test_memory__flat_translation_interface__unaligned_guest_address_returns_correct_offset(
    test_context_t *context)
{
    int return_code = EXIT_SUCCESS;
    bal_flat_translation_interface_init(&context->allocator,
                                        &context->interface,
                                        &context->code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context->logger);
    const bal_guest_address_t valid_gvas[3]           = { 0x1, 0x2, 0x3 };
    size_t                    max_readable_size_bytes = 0;

    for (size_t i = 0; i < 3; ++i)
    {
        const uint8_t *host_address = context->interface.translate(
            &context->interface, valid_gvas[i], &max_readable_size_bytes);
        const uint8_t *expected_host_address = (uint8_t *)&context->code_buffer + valid_gvas[i];

        if (host_address != expected_host_address)
        {
            BAL_LOG_ERROR(&context->logger,
                          "Expected Host Address 0x%llX, Got 0x%llX",
                          expected_host_address,
                          host_address);
            return_code = EXIT_FAILURE;
            break;
        }
    }

    return return_code;
}

void *
empty_allocate(bal_allocator_handle_t allocator, size_t alignment, size_t size)
{
    (void)allocator;
    (void)alignment;
    (void)size;
    return NULL;
}

/*** end of file ***/