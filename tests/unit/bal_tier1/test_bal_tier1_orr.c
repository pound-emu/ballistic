#include "bal_engine.h"
#include "bal_engine_flags.h"
#include "bal_memory.h"
#include "unity.h"
#include <string.h>

#define ARM64_RET_ENCODING     0xD65F03C0U
#define TEST_GUEST_MEMORY_SIZE 4096U

#define ORR_X8_X9_X10 0xAA0A0128U
#define ORR_X8_X9_X10_LSL4 0xAA0A1128U
#define ORR_X8_X9_X8_LSL4 0xAA081128U
#define ORR_X8_X8_X10 0xAA0A0108U
#define ORR_XZR_X9_X10 0xAA0A01E8U
#define ORR_X8_XZR_X10 0xAA0A03E8U
#define ORR_X8_X9_XZR 0xAA1F0128U
#define ORR_X8_XZR_XZR 0xAA1F03E8U
#define ORR_X8_XZR_X8_LSL4 0xAA0813E8U

static uint32_t bal_test_guest_memory[TEST_GUEST_MEMORY_SIZE / sizeof(uint32_t)];

void
setUp(void)
{
    (void)memset(bal_test_guest_memory, 0, sizeof(bal_test_guest_memory));
}

void
tearDown(void)
{
}

static void
run_single_guest_instruction(const uint32_t instruction, bal_cpu_t *BAL_RESTRICT cpu)
{
    bal_test_guest_memory[0] = instruction;
    bal_test_guest_memory[1] = ARM64_RET_ENCODING;

    bal_allocator_t allocator = {};
    bal_allocator_default_init(&allocator);

    bal_memory_interface_t memory_interface = {};
    TEST_ASSERT_EQUAL(BAL_SUCCESS,
                      bal_flat_translation_interface_init(&allocator,
                                                          &memory_interface,
                                                          bal_test_guest_memory,
                                                          sizeof(bal_test_guest_memory)));

    cpu->pc = 0ULL;

    bal_engine_t engine = {};
    TEST_ASSERT_EQUAL(BAL_SUCCESS, bal_engine_init(&engine, cpu, &allocator, &memory_interface));

    engine.flags |= BAL_ENGINE_FLAG_SINGLE_STEP;

    const bal_error_t run_status = bal_engine_run_thread(&engine);

    bal_engine_destroy(&engine);
    (void)bal_flat_translation_interface_destroy(&allocator, &memory_interface);

    TEST_ASSERT_EQUAL(BAL_SUCCESS, run_status);
}

static void
test_BalTier1Orr_CombinesBothSourceRegisters(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xAAAAAAAAAAAAAAAAULL;
    cpu.x[9]      = 0x0F0F0F0F0F0F0F0FULL;
    cpu.x[10]     = 0xF0F0F0F0F0F0F0F0ULL;

    run_single_guest_instruction(ORR_X8_X9_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFFFULL, cpu.x[8]);
}

static void
test_BalTier1Orr_AppliesLeftShiftToSecondSource(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[9]      = 0x0000000000000001ULL;
    cpu.x[10]     = 0x0000000000000001ULL;

    run_single_guest_instruction(ORR_X8_X9_X10_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000011ULL, cpu.x[8]);
}

static void
test_BalTier1Orr_DestinationMatchingSecondSourceUsesOldValue(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0x00000000000000FFULL;
    cpu.x[9]      = 0x000000000000FF00ULL;

    run_single_guest_instruction(ORR_X8_X9_X8_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x000000000000FFF0ULL, cpu.x[8]);
}

static void
test_BalTier1Orr_DestinationMatchingFirstSource(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0x0000000000000001ULL;
    cpu.x[10]     = 0x0000000000000002ULL;

    run_single_guest_instruction(ORR_X8_X8_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000003ULL, cpu.x[8]);
}

static void
test_BalTier1Orr_WriteToZeroRegisterIsDiscarded(void)
{
    bal_cpu_t cpu = {};
    cpu.x[9]  = 0x0F0F0F0F0F0F0F0FULL;
    cpu.x[10] = 0xF0F0F0F0F0F0F0F0ULL;

    run_single_guest_instruction(ORR_XZR_X9_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000000ULL, cpu.x[31]);
}

static void
test_BalTier1Orr_ZeroRegisterSourceReadsAsZeroNotSP(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[10]     = 0x0123456789ABCDEFULL;
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(ORR_X8_XZR_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0123456789ABCDEFULL, cpu.x[8]);
    TEST_ASSERT_EQUAL_HEX64(0xCAFEBABE00000000ULL, cpu.x[31]);
}

static void
test_BalTier1Orr_SecondSourceZeroRegisterKeepsFirstOperand(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[9]      = 0x0123456789ABCDEFULL;
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(ORR_X8_X9_XZR, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0123456789ABCDEFULL, cpu.x[8]);
}

static void
test_BalTier1Orr_BothSourcesZeroRegisterClearDestination(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(ORR_X8_XZR_XZR, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000000ULL, cpu.x[8]);
}

static void
test_BalTier1Orr_ZeroRegisterSourceShiftOfDestinationItself(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0x00000000000000FFULL;
    cpu.x[31]     = 0xCAFEBABE00000000ULL;

    run_single_guest_instruction(ORR_X8_XZR_X8_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000FF0ULL, cpu.x[8]);
}

int
main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_BalTier1Orr_CombinesBothSourceRegisters);
    RUN_TEST(test_BalTier1Orr_AppliesLeftShiftToSecondSource);
    RUN_TEST(test_BalTier1Orr_DestinationMatchingSecondSourceUsesOldValue);
    RUN_TEST(test_BalTier1Orr_DestinationMatchingFirstSource);
    RUN_TEST(test_BalTier1Orr_WriteToZeroRegisterIsDiscarded);
    RUN_TEST(test_BalTier1Orr_ZeroRegisterSourceReadsAsZeroNotSP);
    RUN_TEST(test_BalTier1Orr_SecondSourceZeroRegisterKeepsFirstOperand);
    RUN_TEST(test_BalTier1Orr_BothSourcesZeroRegisterClearDestination);
    RUN_TEST(test_BalTier1Orr_ZeroRegisterSourceShiftOfDestinationItself);
    return UNITY_END();
}

/*** end of file ***/
