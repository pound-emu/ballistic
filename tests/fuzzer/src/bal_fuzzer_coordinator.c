#include "bal_fuzzer_ipc.h"
#include "bal_fuzzer_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(const int argc, const char **argv)
{
    if (argc < 3)
    {
        (void)fprintf(stderr, "Usage: %s <ballistic_worker_path> <unicorn_worker_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    bal_logger_init_default();

    bal_fuzzer_input_t input       = {};
    input.message_id               = 1U;
    const uint32_t instructions[9] = {
        0xD2800C81U, // MOVZ X1, #100
        0xD2801902U, // MOVZ X2, #200
        0x8B020023U, // ADD  X3, X1, X2
        0xD2800644U, // MOVZ X4, #50
        0xCB040065U, // SUB  X5, X3, X4
        0xD2801FE6U, // MOVZ X6, #0xFF
        0x8A0600A7U, // AND  X7, X5, X6
        0x910014E0U, // ADD  X0, X7, #5
        0xF103FC1FU, // CMP  X0, #255
    };
    (void)memcpy(input.instructions, instructions, sizeof(instructions));
    input.instruction_count = 9U;
    input.initial_state.pc  = 0U;

    BAL_LOG_INFO(&bal_thread_logger, "Running Test...");
    const char *BAL_RESTRICT   ballistic_worker_path = argv[1];
    const char *BAL_RESTRICT   unicorn_worker_path   = argv[2];
    bal_fuzzer_worker_handle_t ballistic_worker      = {};
    bal_fuzzer_worker_handle_t unicorn_worker        = {};
    bal_error_t status = bal_fuzzer_ipc_spawn(&ballistic_worker, ballistic_worker_path);

    if (status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Failed to spawn Ballistic worker.");
        return EXIT_FAILURE;
    }

    status = bal_fuzzer_ipc_spawn(&unicorn_worker, unicorn_worker_path);

    if (status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Failed to spawn Unicorn worker.");
        bal_fuzzer_ipc_destroy(&ballistic_worker);
        return EXIT_FAILURE;
    }

    status = bal_fuzzer_ipc_send(ballistic_worker.input_file_descriptor, &input);

    if (status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Failed to send input to Ballistic worker because %s.",
                      bal_error_to_string(status));
    }

    status = bal_fuzzer_ipc_send(unicorn_worker.input_file_descriptor, &input);

    if (status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Failed to send input to Unicorn worker because %s.",
                      bal_error_to_string(status));
    }

    bal_fuzzer_response_t ballistic_response = {};
    bal_fuzzer_response_t unicorn_response   = {};

    const bal_error_t ballistic_status
        = bal_fuzzer_ipc_receive(ballistic_worker.output_file_descriptor, &ballistic_response);

    const bal_error_t unicorn_status
        = bal_fuzzer_ipc_receive(unicorn_worker.output_file_descriptor, &unicorn_response);

    bal_fuzzer_ipc_destroy(&ballistic_worker);
    bal_fuzzer_ipc_destroy(&unicorn_worker);

    if (ballistic_status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Ballistic worker crashed or disconnected.");
        return EXIT_FAILURE;
    }

    if (unicorn_status != BAL_SUCCESS)
    {
        BAL_LOG_ERROR(&bal_thread_logger, "Unicorn worker crashed or disconnected.");
        return EXIT_FAILURE;
    }

    if (ballistic_response.status != BAL_FUZZER_WORKER_OK)
    {
        BAL_LOG_ERROR(&bal_thread_logger,
                      "Ballistic worker reported execution error and "
                      "returned status code %d.",
                      ballistic_response.status);

        return EXIT_FAILURE;
    }

    const bal_fuzzer_comparison_result_t comparison_result
        = bal_fuzzer_state_compare(&unicorn_response.final_state, &ballistic_response.final_state);

    if (true == comparison_result.match)
    {
        BAL_LOG_INFO(&bal_thread_logger, "CPU states matched! Test successful.");
        return EXIT_SUCCESS;
    }

    BAL_LOG_ERROR(&bal_thread_logger,
                  "CPU state mismatch! %u field(s) mismatched.",
                  comparison_result.divergence_count);

    const bal_fuzzer_divergence_t *BAL_RESTRICT divergence_cursor = comparison_result.divergences;

    for (uint32_t i = 0U; i < comparison_result.divergence_count && i < BAL_FUZZER_MAX_DIVERGENCES;
         ++i)
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