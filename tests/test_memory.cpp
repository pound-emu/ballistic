#include "bal_logging.h"
#include "bal_memory.h"
#include <gtest/gtest.h>

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
}

static void
test_teardown(test_context_t *context)
{
    bal_flat_translation_interface_destroy(&context->allocator, &context->interface);
    context->allocator.free(
        context->allocator.context, context->code_buffer, TEST_BUFFER_SIZE_BYTES);
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

static void *empty_allocate(bal_allocator_handle_t allocator, size_t alignment, size_t size);

TEST(MemoryDefaultAllocate, InvalidSizeReturnsNullptr)
{
    test_context_t context = {};
    test_setup(&context);
    constexpr size_t size             = 0;
    constexpr size_t memory_alignment = 8;
    context.code_buffer               = static_cast<uint32_t *>(
        context.allocator.allocate(context.allocator.context, memory_alignment, size));

    ASSERT_EQ(context.code_buffer, nullptr);

    test_teardown(&context);
}

TEST(MemoryFlatTranslation, InitSuccess)
{
    test_context_t context = {};
    test_setup(&context);
    constexpr size_t memory_alignment = 16U;
    context.code_buffer               = static_cast<uint32_t *>(context.allocator.allocate(
        context.allocator.context, memory_alignment, TEST_BUFFER_SIZE_BYTES));

    ASSERT_NE(context.code_buffer, nullptr);

    const bal_error_t error = bal_flat_translation_interface_init(&context.allocator,
                                                                  &context.interface,
                                                                  context.code_buffer,
                                                                  TEST_BUFFER_SIZE_BYTES,
                                                                  context.logger);
    ASSERT_EQ(error, BAL_SUCCESS);

    test_teardown(&context);
}

TEST(MemoryFlatTranslation, InitInvalidArgumentsReturnsError)
{
    test_context_t context = {};
    test_setup(&context);
    bal_memory_interface_t valid_interface         = {};
    constexpr size_t       valid_memory_alignment  = 16;
    constexpr uint32_t     valid_buffer_size_bytes = TEST_BUFFER_SIZE_BYTES;
    auto                  *valid_code_buffer   = static_cast<uint32_t *>(context.allocator.allocate(
        context.allocator.context, valid_memory_alignment, valid_buffer_size_bytes));
    uint32_t              *invalid_code_buffer = nullptr;

    bal_logger_t logger = {};
    bal_logger_init_default(&logger);

    // Disable logging because Ballistic will output error logs (which is a good thing since
    // we're testing errors).
    //
    logger.min_level                 = BAL_LOG_LEVEL_NONE;
    bal_allocator_t *valid_allocator = &context.allocator;

    bal_allocator_t        *invalid_allocator         = nullptr;
    bal_memory_interface_t *invalid_interface         = nullptr;
    constexpr uint32_t      invalid_buffer_size_bytes = 0;

    bal_error_t error = bal_flat_translation_interface_init(
        invalid_allocator, &valid_interface, valid_code_buffer, valid_buffer_size_bytes, logger);

    ASSERT_EQ(error, BAL_ERROR_INVALID_ARGUMENT);

    error = bal_flat_translation_interface_init(
        valid_allocator, invalid_interface, valid_code_buffer, valid_buffer_size_bytes, logger);

    ASSERT_EQ(error, BAL_ERROR_INVALID_ARGUMENT);

    error = bal_flat_translation_interface_init(
        valid_allocator, &valid_interface, invalid_code_buffer, valid_buffer_size_bytes, logger);

    ASSERT_EQ(error, BAL_ERROR_INVALID_ARGUMENT);

    error = bal_flat_translation_interface_init(
        valid_allocator, &valid_interface, valid_code_buffer, invalid_buffer_size_bytes, logger);

    ASSERT_EQ(error, BAL_ERROR_INVALID_ARGUMENT);

    invalid_code_buffer = static_cast<uint32_t *>(context.allocator.allocate(
        context.allocator.context, valid_memory_alignment, valid_buffer_size_bytes));

    ASSERT_NE(invalid_code_buffer, nullptr);

    invalid_code_buffer += 1;
    valid_allocator->allocate = empty_allocate;
    error                     = bal_flat_translation_interface_init(
        valid_allocator, &valid_interface, invalid_code_buffer, valid_buffer_size_bytes, logger);
    invalid_code_buffer -= 1;

    ASSERT_EQ(error, BAL_ERROR_MEMORY_ALIGNMENT);

    context.allocator.free(context.allocator.context, invalid_code_buffer, valid_buffer_size_bytes);
    context.allocator.free(context.allocator.context, valid_code_buffer, valid_buffer_size_bytes);
    test_teardown(&context);
}

TEST(MemoryFlatTranslation, InitFailedInterfaceAllocationReturnsError)
{
    test_context_t context = {};
    test_setup(&context);
    bal_logger_t logger = {};
    bal_logger_init_default(&logger);

    constexpr size_t memory_alignment = 16;
    context.code_buffer               = static_cast<uint32_t *>(context.allocator.allocate(
        context.allocator.context, memory_alignment, TEST_BUFFER_SIZE_BYTES));
    context.allocator.allocate        = empty_allocate;
    const bal_error_t error           = bal_flat_translation_interface_init(
        &context.allocator, &context.interface, context.code_buffer, memory_alignment, logger);

    ASSERT_EQ(error, BAL_ERROR_ALLOCATION_FAILED);

    test_teardown(&context);
}

TEST(MemoryFlatTranslation, DestroyInvalidArgumentReturnsError)
{
    test_context_t context = {};
    test_setup(&context);
    bal_allocator_t        *valid_allocator   = &context.allocator;
    bal_memory_interface_t  valid_interface   = {};
    bal_allocator_t        *invalid_allocator = nullptr;
    bal_memory_interface_t *invalid_interface = nullptr;
    bal_error_t error = bal_flat_translation_interface_destroy(valid_allocator, invalid_interface);

    ASSERT_EQ(error, BAL_ERROR_INVALID_ARGUMENT);

    error = bal_flat_translation_interface_destroy(invalid_allocator, &valid_interface);

    ASSERT_EQ(error, BAL_ERROR_INVALID_ARGUMENT);

    test_teardown(&context);
}

TEST(MemoryFlatTranslation, TranslateInvalidArgumentsReturnsError)
{
    test_context_t context = {};
    test_setup(&context);
    constexpr size_t memory_alignment = 16;
    context.code_buffer               = static_cast<uint32_t *>(
        context.allocator.allocate(context.allocator.context, memory_alignment, TEST_BUFFER_SIZE));
    bal_flat_translation_interface_init(&context.allocator,
                                        &context.interface,
                                        context.code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context.logger);

    bal_memory_interface_t       *valid_interface        = &context.interface;
    size_t                        valid_max_size_bytes   = 0;
    bal_memory_interface_t       *invalid_interface      = nullptr;
    size_t                       *invalid_max_size_bytes = nullptr;
    constexpr bal_guest_address_t guest_address          = 0x0;

    const uint8_t *host_address
        = context.interface.translate(invalid_interface, guest_address, &valid_max_size_bytes);

    ASSERT_EQ(host_address, nullptr);

    host_address
        = context.interface.translate(valid_interface, guest_address, invalid_max_size_bytes);

    ASSERT_EQ(host_address, nullptr);

    test_teardown(&context);
}

TEST(MemoryFlatTranslation, TranslateReturnsValidPointerWithinBounds)
{
    test_context_t   context          = {};
    constexpr size_t memory_alignment = 16U;
    test_setup(&context);
    context.code_buffer = static_cast<uint32_t *>(
        context.allocator.allocate(context.allocator.context, memory_alignment, TEST_BUFFER_SIZE));
    bal_flat_translation_interface_init(&context.allocator,
                                        &context.interface,
                                        context.code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context.logger);
    constexpr bal_guest_address_t valid_gvas[4]           = { 0x0, 0xF, 0x100, 0xFFC };
    size_t                        max_readable_size_bytes = 0;

    for (const unsigned long valid_gva : valid_gvas)
    {
        const uint8_t *host_address
            = context.interface.translate(&context.interface, valid_gva, &max_readable_size_bytes);

        ASSERT_NE(host_address, nullptr);
    }

    test_teardown(&context);
}

TEST(MemoryFlatTranslation, OutOfBoundsReturnsNull)
{
    test_context_t context = {};
    test_setup(&context);
    constexpr size_t memory_alignment = 16U;
    context.code_buffer               = static_cast<uint32_t *>(
        context.allocator.allocate(context.allocator.context, memory_alignment, TEST_BUFFER_SIZE));
    bal_flat_translation_interface_init(&context.allocator,
                                        &context.interface,
                                        context.code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context.logger);
    constexpr bal_guest_address_t invalid_gvas[3]
        = { TEST_BUFFER_SIZE, TEST_BUFFER_SIZE + 0x1000, 0xFFFFFFFFFFFFFFFF };
    size_t max_readable_size_bytes = 0;

    for (const unsigned long invalid_gva : invalid_gvas)
    {
        const uint8_t *host_address = context.interface.translate(
            &context.interface, invalid_gva, &max_readable_size_bytes);

        ASSERT_EQ(host_address, nullptr);
    }

    test_teardown(&context);
}

TEST(MemoryFlatTranslation, UnalignedGuestAddressReturnsCorrectOffset)
{
    test_context_t context = {};
    test_setup(&context);
    constexpr size_t memory_alignment = 16U;
    context.code_buffer               = static_cast<uint32_t *>(
        context.allocator.allocate(context.allocator.context, memory_alignment, TEST_BUFFER_SIZE));
    bal_flat_translation_interface_init(&context.allocator,
                                        &context.interface,
                                        context.code_buffer,
                                        TEST_BUFFER_SIZE,
                                        context.logger);
    constexpr bal_guest_address_t valid_gvas[3]           = { 0x1, 0x2, 0x3 };
    size_t                        max_readable_size_bytes = 0;

    for (const unsigned long valid_gva : valid_gvas)
    {
        const uint8_t *host_address
            = context.interface.translate(&context.interface, valid_gva, &max_readable_size_bytes);
        const uint8_t *expected_host_address
            = reinterpret_cast<uint8_t *>(context.code_buffer) + valid_gva;

        ASSERT_EQ(host_address, expected_host_address);
    }

    test_teardown(&context);
}

void *
empty_allocate(bal_allocator_handle_t allocator, const size_t alignment, const size_t size)
{
    (void)allocator;
    (void)alignment;
    (void)size;
    return nullptr;
}

/*** end of file ***/
