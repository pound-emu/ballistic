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

/*** end of file ***/