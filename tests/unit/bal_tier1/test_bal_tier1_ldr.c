#include "bal_engine.h"
#include "bal_engine_flags.h"
#include "bal_memory.h"
#include "unity.h"
#include <string.h>

#define ARM64_RET_ENCODING     0xD65F03C0U
#define TEST_GUEST_MEMORY_SIZE 4096U

#define LDR_X1_X0_P255_PRE 0xF84FFC01U
#define LDR_X1_X0_P255_POST 0xF84FF401U
#define LDR_X1_X0_M8_PRE 0xF85F8C01U
#define LDR_W1_X0_M4_POST 0xB85FC401U
#define LDR_W1_X0_P252_PRE 0xB84FCC01U
#define LDRAA_X1_X0_PRE 0xF82FFC01U
#define LDRAB_X1_X0_PRE 0xF8AFFC01U
#define LDRAA_X1_X0_POST 0xF82FF401U
#define LDRAA_X1_X0_ZERO_PRE 0xF8200C01U
#define LDRB_W1_X0_P255_POST 0x384FF401U
#define LDRH_W1_X0_P248_PRE 0x784F8C01U
#define LDRSW_X1_X0_P255_POST 0xB88FF401U
#define LDR_X0_X0_P255_PRE 0xF84FFC00U
#define LDRB_W1_X0_U1023 0x395FFC01U
#define LDRH_W1_X0_U1020 0x794FF801U
#define LDR_X1_X0_U8 0xF9400401U
#define LDR_W1_X0_U4 0xB9400401U

static uint32_t bal_test_guest_memory[TEST_GUEST_MEMORY_SIZE / sizeof(uint32_t)];

static void
set_u64(const uint32_t byte_address, const uint64_t value)
{
    bal_test_guest_memory[byte_address / 4U]      = (uint32_t)value;
    bal_test_guest_memory[byte_address / 4U + 1U] = (uint32_t)(value >> 32);
}

static void
set_u32(const uint32_t byte_address, const uint32_t value)
{
    bal_test_guest_memory[byte_address / 4U] = value;
}

void
setUp(void)
{
    (void)memset(bal_test_guest_memory, 0, sizeof(bal_test_guest_memory));

    set_u64(0x3F8U, 0x3333333333333333ULL);
    set_u64(0x400U, 0x8765432112345678ULL);
    set_u64(0x408U, 0x5555555555555555ULL);
    set_u64(0x4F8U, 0x6666666666666666ULL);
    set_u32(0x4FCU, 0x2468ACE1U);
    set_u64(0x500U, 0x0000000012345678ULL);
    set_u64(0xBF8U, 0xF300F300F300F300ULL);
    set_u32(0x8FCU, 0xAB112233U);
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
test_BalTier1Ldr_PostIndexLoadsAtBaseThenUpdatesIt(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;
    cpu.x[31]      = 0xCAFE000000000000ULL;

    run_single_guest_instruction(LDR_X1_X0_P255_POST, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x8765432112345678ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x4FFULL, cpu.x[0]);
    TEST_ASSERT_EQUAL_HEX64(0xCAFE000000000000ULL, cpu.x[31]);
}

static void
test_BalTier1Ldr_PreIndexUpdatesItAndLoadsAtNewAddress(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDR_X1_X0_P255_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000001234567824ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x4FFULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_PreIndexSignExtendsNegativeImmediate(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDR_X1_X0_M8_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x3333333333333333ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x3F8ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_PostIndexNegativeImmediateStillWritesBackBaseMinusOffset(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDR_W1_X0_M4_POST, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x12345678ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x3FCULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_PreIndex32BitLoadsAtBasePlusOffset(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDR_W1_X0_P252_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x2468ACE1ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x4FCULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_LdraaPreIndexScalesByEightAndWritesBack(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDRAA_X1_X0_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0xF300F300F300F300ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0xBF8ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_LdrabPreIndexScalesByEightAndWritesBack(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDRAB_X1_X0_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0xF300F300F300F300ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0xBF8ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_LdraaPostIndexLeavesBaseRegisterUntouched(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDRAA_X1_X0_POST, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0xF300F300F300F300ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x400ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_LdraaPreIndexZeroImmediateWritesBackBase(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDRAA_X1_X0_ZERO_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x8765432112345678ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x400ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_UnscaledBytePostIndexWritesBackSignedOffset(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDRB_W1_X0_P255_POST, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x78ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x4FFULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_UnscaledHalfwordPreIndexWritesBackSignedOffset(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDRH_W1_X0_P248_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x6666ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x4F8ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_UnscaledSignedWordPostIndexWritesBackSignedOffset(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x500ULL;

    run_single_guest_instruction(LDRSW_X1_X0_P255_POST, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x0000000012345678ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x5FFULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_WriteBackWinsWhenDestinationMatchesBase(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDR_X0_X0_P255_PRE, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x4FFULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_ByteUnsignedOffsetScalesByOne(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x100ULL;

    run_single_guest_instruction(LDRB_W1_X0_U1023, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0xABULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x100ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_HalfwordUnsignedOffsetScalesByTwo(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x100ULL;

    run_single_guest_instruction(LDRH_W1_X0_U1020, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x2233ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x100ULL, cpu.x[0]);
}

static void
test_BalTier1Ldr_WordUnsignedOffsetScalesByAccessSize(void)
{
    bal_cpu_t cpu  = {};
    cpu.x[0]       = 0x400ULL;

    run_single_guest_instruction(LDR_X1_X0_U8, &cpu);

    TEST_ASSERT_EQUAL_HEX64(0x5555555555555555ULL, cpu.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x400ULL, cpu.x[0]);

    bal_cpu_t cpu32 = {};
    cpu32.x[0]      = 0x400ULL;

    run_single_guest_instruction(LDR_W1_X0_U4, &cpu32);

    TEST_ASSERT_EQUAL_HEX64(0x87654321ULL, cpu32.x[1]);
    TEST_ASSERT_EQUAL_HEX64(0x400ULL, cpu32.x[0]);
}

int
main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_BalTier1Ldr_PostIndexLoadsAtBaseThenUpdatesIt);
    RUN_TEST(test_BalTier1Ldr_PreIndexUpdatesItAndLoadsAtNewAddress);
    RUN_TEST(test_BalTier1Ldr_PreIndexSignExtendsNegativeImmediate);
    RUN_TEST(test_BalTier1Ldr_PostIndexNegativeImmediateStillWritesBackBaseMinusOffset);
    RUN_TEST(test_BalTier1Ldr_PreIndex32BitLoadsAtBasePlusOffset);
    RUN_TEST(test_BalTier1Ldr_LdraaPreIndexScalesByEightAndWritesBack);
    RUN_TEST(test_BalTier1Ldr_LdrabPreIndexScalesByEightAndWritesBack);
    RUN_TEST(test_BalTier1Ldr_LdraaPostIndexLeavesBaseRegisterUntouched);
    RUN_TEST(test_BalTier1Ldr_LdraaPreIndexZeroImmediateWritesBackBase);
    RUN_TEST(test_BalTier1Ldr_UnscaledBytePostIndexWritesBackSignedOffset);
    RUN_TEST(test_BalTier1Ldr_UnscaledHalfwordPreIndexWritesBackSignedOffset);
    RUN_TEST(test_BalTier1Ldr_UnscaledSignedWordPostIndexWritesBackSignedOffset);
    RUN_TEST(test_BalTier1Ldr_WriteBackWinsWhenDestinationMatchesBase);
    RUN_TEST(test_BalTier1Ldr_ByteUnsignedOffsetScalesByOne);
    RUN_TEST(test_BalTier1Ldr_HalfwordUnsignedOffsetScalesByTwo);
    RUN_TEST(test_BalTier1Ldr_WordUnsignedOffsetScalesByAccessSize);

    return UNITY_END();
}
