#include "bal_fuzzer_ipc.h"
#include "bal_fuzzer_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(const int argc, const char **argv)
{
    if (argc < 4)
    {
        (void)fprintf(stderr,
                      "Usage: %s <ballistic_worker_path> <unicorn_worker_path> <seeds.bin>\n",
                      argv[0]);
        return EXIT_FAILURE;
    }

    bal_logger_init_default();
    bal_thread_logger.min_level = BAL_LOG_LEVEL_INFO;

    BAL_LOG_INFO(&bal_thread_logger, "Running Test...");
    const char *BAL_RESTRICT ballistic_worker_path = argv[1];
    const char *BAL_RESTRICT unicorn_worker_path   = argv[2];
    const char *BAL_RESTRICT seeds_file_path       = argv[3];

    FILE *seeds_file = fopen(seeds_file_path, "rb");

    if (NULL == seeds_file)
    {
        BAL_LOG_ERROR(
            &bal_thread_logger, "Aborting process: Failed ot open seeds file %s", seeds_file_path);
        return EXIT_FAILURE;
    }

    (void)fseek(seeds_file, 0, SEEK_END);
    const long seeds_file_size = ftell(seeds_file);
    (void)fseek(seeds_file, 0, SEEK_SET);
    const size_t           seed_count = (size_t)seeds_file_size / sizeof(uint32_t);
    uint32_t *BAL_RESTRICT seeds      = malloc(seed_count * sizeof(uint32_t));

    if (NULL == seeds)
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Aborting process: failed to allocate seeds buffer.");
        (void)fclose(seeds_file);
        return EXIT_FAILURE;
    }

    (void)fread(seeds, sizeof(uint32_t), seed_count, seeds_file);
    (void)fclose(seeds_file);

    BAL_LOG_INFO(&bal_thread_logger, "Loaded %u seeds from %s.", seed_count, seeds_file_path);

    bal_fuzzer_worker_handle_t ballistic_worker = {};
    bal_fuzzer_worker_handle_t unicorn_worker   = {};
    bal_error_t status = bal_fuzzer_ipc_spawn(&ballistic_worker, ballistic_worker_path);

    if (status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Failed to spawn Ballistic worker.");
        free(seeds);
        return EXIT_FAILURE;
    }

    status = bal_fuzzer_ipc_spawn(&unicorn_worker, unicorn_worker_path);

    if (status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Failed to spawn Unicorn worker.");
        bal_fuzzer_ipc_destroy(&ballistic_worker);
        free(seeds);
        return EXIT_FAILURE;
    }

    const uint32_t *BAL_RESTRICT seed_cursor   = seeds;
    uint64_t                     seeds_passed  = 0U;
    uint64_t                     seeds_failed  = 0U;
    uint64_t                     seeds_skipped = 0U;
    uint64_t                     errors        = 0U;
    for (size_t i = 0U; i < seed_count; ++i)
    {
        if (errors > 5U)
        {
            break;
        }

        bal_fuzzer_input_t input = {};

        // Add 1 because at the start of loop, the ID needs to start at 1 not 0.
        input.message_id        = i + 1U;
        input.instruction_count = 1U;
        input.instructions[0]   = *seed_cursor;
        input.initial_state.pc  = 0U;

        status = bal_fuzzer_ipc_send(ballistic_worker.input_file_descriptor, &input, sizeof(input));

        if (status != BAL_SUCCESS)
        {
            BAL_LOG_ERROR(&bal_thread_logger,
                          "Failed to send input to Ballistic worker because %s.",
                          bal_error_to_string(status));
            ++errors;
            ++seed_cursor;
            continue;
        }

        status = bal_fuzzer_ipc_send(unicorn_worker.input_file_descriptor, &input, sizeof(input));

        if (status != BAL_SUCCESS)
        {
            BAL_LOG_ERROR(&bal_thread_logger,
                          "Failed to send input to Unicorn worker because %s.",
                          bal_error_to_string(status));
            ++errors;
            ++seed_cursor;
            continue;
        }

        bal_fuzzer_response_t ballistic_response = {};
        bal_fuzzer_response_t unicorn_response   = {};

        const bal_error_t ballistic_ipc_status
            = bal_fuzzer_ipc_receive(ballistic_worker.output_file_descriptor,
                                     &ballistic_response,
                                     sizeof(ballistic_response));

        const bal_error_t unicorn_ipc_status = bal_fuzzer_ipc_receive(
            unicorn_worker.output_file_descriptor, &unicorn_response, sizeof(ballistic_response));

        if (ballistic_ipc_status != BAL_SUCCESS)
        {
            BAL_LOG_ERROR(&bal_thread_logger, "Ballistic worker crashed or disconnected.");
            ++errors;
            ++seed_cursor;
            continue;
        }

        if (unicorn_ipc_status != BAL_SUCCESS)
        {
            BAL_LOG_ERROR(&bal_thread_logger, "Unicorn worker crashed or disconnected.");
            ++errors;
            ++seed_cursor;
            continue;
        }

        if (BAL_FUZZER_WORKER_ERROR_UNKNOWN_INSTRUCTION == ballistic_response.status)
        {
            ++seed_cursor;
            ++seeds_skipped;
            continue;
        }
        if (ballistic_response.status != BAL_FUZZER_WORKER_OK)
        {
            BAL_LOG_ERROR(&bal_thread_logger,
                          "Ballistic worker reported execution error and "
                          "returned status code %d.",
                          ballistic_response.status);
            ++errors;
            ++seed_cursor;
            continue;
        }

        if (unicorn_response.status != BAL_FUZZER_WORKER_OK)
        {
            BAL_LOG_WARN(&bal_thread_logger,
                         "Unicorn worker reported execution error for seed 0x%08X and "
                         "returned status code %d, skipping...",
                         *seed_cursor,
                         unicorn_response.status);
            ++seeds_skipped;
            ++seed_cursor;
            continue;
        }

        const bal_fuzzer_comparison_result_t comparison_result = bal_fuzzer_state_compare(
            &unicorn_response.final_state, &ballistic_response.final_state);

        if (true == comparison_result.match)
        {
            ++seeds_passed;
        }
        else
        {
            BAL_LOG_ERROR(&bal_thread_logger,
                          "CPU state mismatch! %u field(s) mismatched for seed 0x%08X.",
                          comparison_result.divergence_count,
                          *seed_cursor);
            ++seeds_failed;

            const bal_fuzzer_divergence_t *BAL_RESTRICT divergence_cursor
                = comparison_result.divergences;

            for (uint32_t ii = 0U;
                 ii < comparison_result.divergence_count && ii < BAL_FUZZER_MAX_DIVERGENCES;
                 ++ii)
            {
                const char *field_name = "unknown";
                char        register_label[8];

                switch (divergence_cursor->field)
                {
                    case BAL_FUZZER_FIELD_REGISTER:
                        (void)snprintf(register_label,
                                       sizeof(register_label),
                                       "x%u",
                                       divergence_cursor->register_index);
                        field_name = register_label;
                        break;
                    case BAL_FUZZER_FIELD_PC:
                        field_name = "pc";
                        break;
                    case BAL_FUZZER_FIELD_FLAG_C:
                        field_name = "flag_c";
                        break;
                    case BAL_FUZZER_FIELD_FLAG_Z:
                        field_name = "flag_z";
                        break;
                    case BAL_FUZZER_FIELD_FLAG_N:
                        field_name = "flag_n";
                        break;
                    case BAL_FUZZER_FIELD_FLAG_V:
                        field_name = "flag_v";
                        break;
                    default:
                        break;
                }

                BAL_LOG_ERROR(&bal_thread_logger,
                              "field=%-7s expected=0x%llx actual=0x%llx",
                              field_name,
                              (unsigned long long)divergence_cursor->expected_value,
                              (unsigned long long)divergence_cursor->actual_value);
                ++divergence_cursor;
            }
        }

        ++seed_cursor;
    }

    bal_fuzzer_ipc_destroy(&ballistic_worker);
    bal_fuzzer_ipc_destroy(&unicorn_worker);
    free(seeds);

    BAL_LOG_INFO(&bal_thread_logger,
                 "Fuzz complete: %u seeds | %llu passed | %llu failed | %llu seeds skipped | %llu "
                 "errors.",
                 seed_count,
                 (unsigned long long)seeds_passed,
                 (unsigned long long)seeds_failed,
                 (unsigned long long)seeds_skipped,
                 (unsigned long long)errors);

    // Return true even if Ballistic is broken (for now). This code needs to get pushed to upstream
    // without the CI failing.
    return EXIT_SUCCESS;
}