#include "bal_fuzzer_state.h"
#include <string.h>

bal_fuzzer_comparison_result_t
bal_fuzzer_state_compare(const bal_fuzzer_cpu_snapshot_t *expected,
                         const bal_fuzzer_cpu_snapshot_t *actual)
{
    bal_fuzzer_comparison_result_t result = {};

    if (expected == actual)
    {
        result.match = true;
        return result;
    }

    const uint64_t *BAL_RESTRICT expected_register_cursor = expected->x;
    const uint64_t *BAL_RESTRICT actual_register_cursor   = actual->x;
    uint32_t                     divergence_count         = 0U;

    for (uint32_t i = 0U; i < 32U; ++i)
    {
        if (*expected_register_cursor != *actual_register_cursor)
        {
            if (divergence_count < BAL_FUZZER_MAX_DIVERGENCES)
            {
                bal_fuzzer_divergence_t *BAL_RESTRICT divergence
                    = &result.divergences[divergence_count];
                divergence->field          = BAL_FUZZER_FIELD_REGISTER;
                divergence->register_index = i;
                divergence->expected_value = *expected_register_cursor;
                divergence->actual_value   = *actual_register_cursor;
            }

            ++divergence_count;
        }

        ++expected_register_cursor;
        ++actual_register_cursor;
    }

    if (expected->pc != actual->pc)
    {
        if (divergence_count < BAL_FUZZER_MAX_DIVERGENCES)
        {
            bal_fuzzer_divergence_t *BAL_RESTRICT divergence
                = &result.divergences[divergence_count];
            divergence->field          = BAL_FUZZER_FIELD_PC;
            divergence->register_index = 0U;
            divergence->expected_value = expected->pc;
            divergence->actual_value   = actual->pc;
            ++divergence_count;
        }
    }

    if (expected->flag_c != actual->flag_c)
    {
        if (divergence_count < BAL_FUZZER_MAX_DIVERGENCES)
        {
            bal_fuzzer_divergence_t *BAL_RESTRICT divergence
                = &result.divergences[divergence_count];
            divergence->field          = BAL_FUZZER_FIELD_FLAG_C;
            divergence->register_index = 0U;
            divergence->expected_value = expected->flag_c;
            divergence->actual_value   = actual->flag_c;
            ++divergence_count;
        }
    }

    if (expected->flag_z != actual->flag_z)
    {
        if (divergence_count < BAL_FUZZER_MAX_DIVERGENCES)
        {
            bal_fuzzer_divergence_t *BAL_RESTRICT divergence
                = &result.divergences[divergence_count];
            divergence->field          = BAL_FUZZER_FIELD_FLAG_Z;
            divergence->register_index = 0U;
            divergence->expected_value = expected->flag_z;
            divergence->actual_value   = actual->flag_z;
            ++divergence_count;
        }
    }

    if (expected->flag_n != actual->flag_n)
    {
        if (divergence_count < BAL_FUZZER_MAX_DIVERGENCES)
        {
            bal_fuzzer_divergence_t *BAL_RESTRICT divergence
                = &result.divergences[divergence_count];
            divergence->field          = BAL_FUZZER_FIELD_FLAG_N;
            divergence->register_index = 0U;
            divergence->expected_value = expected->flag_n;
            divergence->actual_value   = actual->flag_n;
            ++divergence_count;
        }
    }

    if (expected->flag_v != actual->flag_v)
    {
        if (divergence_count < BAL_FUZZER_MAX_DIVERGENCES)
        {
            bal_fuzzer_divergence_t *BAL_RESTRICT divergence
                = &result.divergences[divergence_count];
            divergence->field          = BAL_FUZZER_FIELD_FLAG_V;
            divergence->register_index = 0U;
            divergence->expected_value = expected->flag_v;
            divergence->actual_value   = actual->flag_v;
            ++divergence_count;
        }
    }

    result.match            = divergence_count == 0U;
    result.divergence_count = divergence_count;
    return result;
}

void
bal_fuzzer_state_capture_bal_cpu(bal_fuzzer_cpu_snapshot_t *snapshot, const bal_cpu_t *cpu)
{
    (void)memcpy(snapshot, cpu->x, sizeof(snapshot->x));
    snapshot->pc                = cpu->pc;
    snapshot->flag_c            = cpu->flag_c;
    snapshot->flag_z            = cpu->flag_z;
    snapshot->flag_n            = cpu->flag_n;
    snapshot->flag_v            = cpu->flag_v;
    snapshot->instruction_count = cpu->instruction_count;
}

void
bal_fuzzer_state_capture_unicorn_cpu(bal_fuzzer_cpu_snapshot_t *snapshot, uc_engine *cpu)
{
    (void)memset(snapshot, 0, sizeof(*snapshot));
    int register_ids[32] = {};

    for (uint32_t arm_register = 0U; arm_register < 29U; ++arm_register)
    {
        register_ids[arm_register] = UC_ARM64_REG_X0 + (int)arm_register;
    }

    // Unicorn's ARM register enum is NOT contiguous for X0-X31. Why???
    register_ids[29] = UC_ARM64_REG_X29;
    register_ids[30] = UC_ARM64_REG_X30;
    register_ids[31] = UC_ARM64_REG_XZR;

    uint64_t *BAL_RESTRICT register_cursor     = snapshot->x;
    void                  *register_values[32] = {};

    for (uint32_t arm_register = 0U; arm_register < 32U; ++arm_register)
    {
        register_values[arm_register] = register_cursor + arm_register;
    }

    (void)uc_reg_read_batch(cpu, register_ids, register_values, 32U);
    uint64_t pc = 0U;
    (void)uc_reg_read(cpu, UC_ARM64_REG_PC, &pc);
    snapshot->pc = pc;

    uint32_t nzcv = 0U;
    (void)uc_reg_read(cpu, UC_ARM64_REG_NZCV, &nzcv);
    snapshot->flag_n = (uint8_t)((nzcv >> 31U) & 1U);
    snapshot->flag_z = (uint8_t)((nzcv >> 30U) & 1U);
    snapshot->flag_c = (uint8_t)((nzcv >> 29U) & 1U);
    snapshot->flag_v = (uint8_t)((nzcv >> 28U) & 1U);
}

/*** end of file ***/