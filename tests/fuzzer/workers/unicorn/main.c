#include "bal_engine.h"
#include "bal_fuzzer_ipc.h"
#include "bal_fuzzer_protocol.h"
#include "bal_fuzzer_state.h"
#include "unicorn/unicorn.h"
#include <string.h>
#include <unistd.h>

#define GUEST_BASE_ADDRESS 0x0
#define GUEST_MEMORY_SIZE  0X1000ULL // 4 KiB, one page.

int
main(void)
{
    bal_fuzzer_input_t    input         = {};
    bal_fuzzer_response_t response      = {};
    uc_engine            *engine        = NULL;
    uc_err                unicorn_error = uc_open(UC_ARCH_ARM64, UC_MODE_LITTLE_ENDIAN, &engine);

    if (unicorn_error != UC_ERR_OK)
    {
        (void)fprintf(stderr, "uc_open failed: %s\n", uc_strerror(unicorn_error));
        return 1;
    }

    unicorn_error = uc_ctl_set_cpu_model(engine, UC_CPU_ARM64_MAX);

    if (unicorn_error != UC_ERR_OK)
    {
        (void)fprintf(stderr, "uc_ctl_set_cpu_model failed: %s\n", uc_strerror(unicorn_error));
        return 1;
    }

    while (true)
    {
        if (bal_fuzzer_ipc_receive(STDIN_FILENO, &input, sizeof(input)) != BAL_SUCCESS)
        {
            break;
        }

        response.message_id = input.message_id;
        response.status     = BAL_FUZZER_WORKER_OK;
        (void)memset(&response.final_state, 0, sizeof(response.final_state));

        if (input.instruction_count > BAL_FUZZER_MAX_INSTRUCTIONS)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_COMPILE_FAILED;
            (void)bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        unicorn_error = uc_mem_map(
            engine, GUEST_BASE_ADDRESS, GUEST_MEMORY_SIZE, UC_PROT_READ | UC_PROT_EXEC);

        if (unicorn_error != UC_ERR_OK)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_COMPILE_FAILED;
            (void)bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        const uint64_t code_buffer_size_bytes = input.instruction_count * 4ULL;
        unicorn_error
            = uc_mem_write(engine, GUEST_BASE_ADDRESS, input.instructions, code_buffer_size_bytes);

        if (unicorn_error != UC_ERR_OK)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_COMPILE_FAILED;
            (void)uc_mem_unmap(engine, GUEST_BASE_ADDRESS, GUEST_MEMORY_SIZE);
            (void)bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        int   register_ids[32];
        void *register_values[32];

        for (uint32_t arm_register = 0U; arm_register < 32U; ++arm_register)
        {
            register_ids[arm_register]    = UC_ARM64_REG_X0 + (int)arm_register;
            register_values[arm_register] = &input.initial_state.x[arm_register];
        }

        // Unicorn's ARM register enum is NOT contiguous for X0-X31. Why???
        register_ids[29] = UC_ARM64_REG_X29;
        register_ids[30] = UC_ARM64_REG_X30;
        register_ids[31] = UC_ARM64_REG_XZR;

        unicorn_error = uc_reg_write_batch(engine, register_ids, register_values, 32);

        if (unicorn_error != UC_ERR_OK)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_EXECUTION_FAILED;
            (void)uc_mem_unmap(engine, GUEST_BASE_ADDRESS, GUEST_MEMORY_SIZE);
            (void)bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        uint64_t pc   = GUEST_BASE_ADDRESS + input.initial_state.pc;
        unicorn_error = uc_reg_write(engine, UC_ARM64_REG_PC, &pc);

        if (unicorn_error != UC_ERR_OK)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_EXECUTION_FAILED;
            (void)uc_mem_unmap(engine, GUEST_BASE_ADDRESS, GUEST_MEMORY_SIZE);
            (void)bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        uint64_t x30  = GUEST_BASE_ADDRESS + code_buffer_size_bytes;
        unicorn_error = uc_reg_write(engine, UC_ARM64_REG_X30, &x30);

        if (unicorn_error != UC_ERR_OK)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_EXECUTION_FAILED;
            (void)uc_mem_unmap(engine, GUEST_BASE_ADDRESS, GUEST_MEMORY_SIZE);
            (void)bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        uint32_t nzcv = 0U;

        if (input.initial_state.flag_n != 0)
        {
            nzcv |= 1U << 31U;
        }

        if (input.initial_state.flag_z != 0)
        {
            nzcv |= 1U << 30U;
        }

        if (input.initial_state.flag_c != 0)
        {
            nzcv |= 1U << 29U;
        }

        if (input.initial_state.flag_v != 0)
        {
            nzcv |= 1U << 28U;
        }

        unicorn_error = uc_reg_write(engine, UC_ARM64_REG_NZCV, &nzcv);

        if (unicorn_error != UC_ERR_OK)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_EXECUTION_FAILED;
            (void)uc_mem_unmap(engine, GUEST_BASE_ADDRESS, GUEST_MEMORY_SIZE);
            (void)bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        unicorn_error = uc_emu_start(engine, GUEST_BASE_ADDRESS, x30, 0U, input.instruction_count);
        if (unicorn_error != UC_ERR_OK)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_COMPILE_FAILED;
        }
        else
        {
            response.status = BAL_FUZZER_WORKER_OK;
        }

        bal_fuzzer_state_capture_unicorn_cpu(&response.final_state, engine);
        (void)uc_mem_unmap(engine, GUEST_BASE_ADDRESS, GUEST_MEMORY_SIZE);

        if (bal_fuzzer_ipc_send(STDOUT_FILENO, &response, sizeof(response)) != BAL_SUCCESS)
        {
            break;
        }
    }

    (void)uc_close(engine);
    return 0;
}

/*** end of file ***/