#include "bal_engine.h"
#include "bal_engine_flags.h"
#include "bal_memory.h"
#include "unity.h"
#include <string.h>

#define ARM64_RET_ENCODING     0xD65F03C0U
#define TEST_GUEST_MEMORY_SIZE 4096U

#define ADD_X8_X9_X10_LSL4 0x8B0A1128U
#define ADD_X8_X9_X10_LSR4 0x8B4A1128U
#define ADD_X8_X9_X10_ASR4 0x8B8A1128U
#define ADD_X8_X9_X10_LSL63 0x8B0AFD28U
#define ADD_X8_X9_X8_LSL4 0x8B081128U
#define ADD_X8_X8_X10_LSL4 0x8B0A1108U
#define ADD_X8_X9_X10 0x8B0A0128U
#define SUB_X8_X9_X10_LSL4 0xCB0A1128U
#define SUB_X8_X9_X8_LSL4 0xCB081128U
#define CMP_X8_X9_LSL4 0xEB0A113FU
#define CMP_X8_X9 0xEB0A013FU
#define CMN_X8_X9_ASR4 0xAB8A113FU

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
test_BalTier1AddSub_AddsLeftShiftedSecondSource(void)
{
    bal_cpu_t cpu      = {};
    cpu.x[8]           = 0x0000000000000000ULL;
    cpu.x[9]           = 0x0000000000001000ULL;
    cpu.x[10]          = 0x00000000100000FFULL;

    run_single_guest_instruction(ADD_X8_X9_X10_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000100001FF0ULL, cpu.x[8]);
    TEST_ASSERT_EQUAL_HEX64(0x00000000100000FFULL, cpu.x[10]);
}

static void
test_BalTier1AddSub_AddsLogicalRightShiftedSecondSource(void)
{
    bal_cpu_t cpu = {};
    cpu.x[9]      = 0x0000000000000001ULL;
    cpu.x[10]     = 0x0000000000000FF0ULL;

    run_single_guest_instruction(ADD_X8_X9_X10_LSR4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000100ULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_AddsArithmeticShiftedSecondSource(void)
{
    bal_cpu_t cpu = {};
    cpu.x[9]      = 0x0000000000000100ULL;
    cpu.x[10]     = 0xFFFFFFFFFFFFFFF0ULL;

    run_single_guest_instruction(ADD_X8_X9_X10_ASR4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x00000000000000FFULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_AddsLeftShiftedBySixtyThree(void)
{
    bal_cpu_t cpu = {};
    cpu.x[9]      = 0x0000000000000001ULL;
    cpu.x[10]     = 0x0000000000000001ULL;

    run_single_guest_instruction(ADD_X8_X9_X10_LSL63, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x8000000000000001ULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_DestinationMatchingSecondSourceUsesOldValue(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0x0000000000000001ULL;
    cpu.x[9]      = 0x0000000000000010ULL;

    run_single_guest_instruction(ADD_X8_X9_X8_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000020ULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_DestinationMatchingFirstSource(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0x0000000000000010ULL;
    cpu.x[10]     = 0x0000000000000001ULL;

    run_single_guest_instruction(ADD_X8_X8_X10_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000020ULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_SubtractsLeftShiftedSecondSource(void)
{
    bal_cpu_t cpu = {};
    cpu.x[9]      = 0x0000000000001000ULL;
    cpu.x[10]     = 0x00000000000000FFULL;

    run_single_guest_instruction(SUB_X8_X9_X10_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000010ULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_SubtractionDestinationMatchingSecondSourceUsesOldValue(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0x0000000000000010ULL;
    cpu.x[9]      = 0x0000000000000120ULL;

    run_single_guest_instruction(SUB_X8_X9_X8_LSL4, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000020ULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_CompareSetsFlagsFromShiftedOperand(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[9]      = 0x0000000000000010ULL;
    cpu.x[10]     = 0x0000000000000001ULL;
    cpu.x[31]     = 0xCAFE000000000000ULL;

    run_single_guest_instruction(CMP_X8_X9_LSL4, &cpu);

    TEST_ASSERT_EQUAL_UINT8(0U, cpu.flag_n);
    TEST_ASSERT_EQUAL_UINT8(1U, cpu.flag_z);
    TEST_ASSERT_EQUAL_UINT8(1U, cpu.flag_c);
    TEST_ASSERT_EQUAL_UINT8(0U, cpu.flag_v);

    TEST_ASSERT_EQUAL_HEX64(0xDEADBEEFDEADBEEFULL, cpu.x[8]);
    TEST_ASSERT_EQUAL_HEX64(0x0000000000000010ULL, cpu.x[9]);
    TEST_ASSERT_EQUAL_HEX64(0x0000000000000001ULL, cpu.x[10]);
    TEST_ASSERT_EQUAL_HEX64(0xCAFE000000000000ULL, cpu.x[31]);
}

static void
test_BalTier1AddSub_CompareNegateSetsFlagsFromShiftedOperand(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[9]      = 0xFFFFFFFFFFFFFFF0ULL;
    cpu.x[10]     = 0xFFFFFFFFFFFFFFF0ULL;
    cpu.x[31]     = 0xCAFE000000000000ULL;

    run_single_guest_instruction(CMN_X8_X9_ASR4, &cpu);

    TEST_ASSERT_EQUAL_UINT8(1U, cpu.flag_n);
    TEST_ASSERT_EQUAL_UINT8(0U, cpu.flag_z);
    TEST_ASSERT_EQUAL_UINT8(1U, cpu.flag_c);
    TEST_ASSERT_EQUAL_UINT8(0U, cpu.flag_v);

    TEST_ASSERT_EQUAL_HEX64(0xDEADBEEFDEADBEEFULL, cpu.x[8]);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFF0ULL, cpu.x[9]);
    TEST_ASSERT_EQUAL_HEX64(0xFFFFFFFFFFFFFFF0ULL, cpu.x[10]);
    TEST_ASSERT_EQUAL_HEX64(0xCAFE000000000000ULL, cpu.x[31]);
}

static void
test_BalTier1AddSub_UnshiftedAddStillWorks(void)
{
    bal_cpu_t cpu = {};
    cpu.x[9]      = 0x0000000000000010ULL;
    cpu.x[10]     = 0x0000000000000002ULL;

    run_single_guest_instruction(ADD_X8_X9_X10, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000000000012ULL, cpu.x[8]);
}

static void
test_BalTier1AddSub_UnshiftedCompareStillWorks(void)
{
    bal_cpu_t cpu = {};
    cpu.x[8]      = 0xDEADBEEFDEADBEEFULL;
    cpu.x[9]      = 0x0000000000000010ULL;
    cpu.x[10]     = 0x0000000000000020ULL;
    cpu.x[31]     = 0xCAFE000000000000ULL;

    run_single_guest_instruction(CMP_X8_X9, &cpu);

    TEST_ASSERT_EQUAL_UINT8(1U, cpu.flag_n);
    TEST_ASSERT_EQUAL_UINT8(0U, cpu.flag_z);
    TEST_ASSERT_EQUAL_UINT8(0U, cpu.flag_c);
    TEST_ASSERT_EQUAL_UINT8(0U, cpu.flag_v);

    TEST_ASSERT_EQUAL_HEX64(0xDEADBEEFDEADBEEFULL, cpu.x[8]);
    TEST_ASSERT_EQUAL_HEX64(0x0000000000000010ULL, cpu.x[9]);
    TEST_ASSERT_EQUAL_HEX64(0xCAFE000000000000ULL, cpu.x[31]);
}

int
main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_BalTier1AddSub_AddsLeftShiftedSecondSource);
    RUN_TEST(test_BalTier1AddSub_AddsLogicalRightShiftedSecondSource);
    RUN_TEST(test_BalTier1AddSub_AddsArithmeticShiftedSecondSource);
    RUN_TEST(test_BalTier1AddSub_AddsLeftShiftedBySixtyThree);
    RUN_TEST(test_BalTier1AddSub_DestinationMatchingSecondSourceUsesOldValue);
    RUN_TEST(test_BalTier1AddSub_DestinationMatchingFirstSource);
    RUN_TEST(test_BalTier1AddSub_SubtractsLeftShiftedSecondSource);
    RUN_TEST(test_BalTier1AddSub_SubtractionDestinationMatchingSecondSourceUsesOldValue);
    RUN_TEST(test_BalTier1AddSub_CompareSetsFlagsFromShiftedOperand);
    RUN_TEST(test_BalTier1AddSub_CompareNegateSetsFlagsFromShiftedOperand);
    RUN_TEST(test_BalTier1AddSub_UnshiftedAddStillWorks);
    RUN_TEST(test_BalTier1AddSub_UnshiftedCompareStillWorks);

    return UNITY_END();
}
