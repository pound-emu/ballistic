#include "bal_attributes.h"
#include "bal_engine.h"
#include "bal_fuzzer_protocol.h"
#include "bal_fuzzer_state.h"
#include "bal_log.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define WORKER_GUEST_MEMORY 4096U
#define ARM64_RET_ENCODING  0xD65F03C0U

static int read_exact(int fd, void *data, size_t size);
static int write_exact(int fd, const void *data, size_t size);

int
main(void)
{
    // Dont corrupt the pipe with logs.
    bal_thread_logger.min_level    = BAL_LOG_LEVEL_NONE;
    bal_fuzzer_input_t    input    = {};
    bal_fuzzer_response_t response = {};

    for (;;)
    {
        if (read_exact(STDIN_FILENO, &input, sizeof(input)) != 0)
        {
            return 0;
        }

        response.message_id = input.message_id;
        response.status     = BAL_FUZZER_WORKER_OK;
        (void)memset(&response.final_state, 0, sizeof(response.final_state));

        uint32_t guest_memory[WORKER_GUEST_MEMORY / sizeof(uint32_t)];
        (void)memset(guest_memory, 0, sizeof(guest_memory));

        const uint32_t instruction_count = input.instruction_count;

        if (instruction_count > BAL_FUZZER_MAX_INSTRUCTIONS)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_COMPILE_FAILED;
            (void)write_exact(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        (void)memcpy(guest_memory, input.instructions, instruction_count * sizeof(uint32_t));
        guest_memory[instruction_count] = ARM64_RET_ENCODING;
        bal_allocator_t allocator       = {};
        bal_allocator_default_init(&allocator);
        bal_memory_interface_t memory_interface = {};
        bal_error_t            status           = bal_flat_translation_interface_init(
            &allocator, &memory_interface, guest_memory, sizeof(guest_memory));

        if (status != BAL_SUCCESS)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_COMPILE_FAILED;
            (void)write_exact(STDOUT_FILENO, &response, sizeof(response));
            continue;
        }

        bal_cpu_t cpu = {};
        (void)memcpy(cpu.x, input.initial_state.x, sizeof(cpu.x));
        cpu.pc     = input.initial_state.pc;
        cpu.flag_c = input.initial_state.flag_c;
        cpu.flag_z = input.initial_state.flag_z;
        cpu.flag_n = input.initial_state.flag_n;
        cpu.flag_v = input.initial_state.flag_v;

        cpu.x[30]           = BAL_ENGINE_SENTINEL;
        bal_engine_t engine = {};
        status              = bal_engine_init(&engine, &cpu, &allocator, &memory_interface);

        if (status != BAL_SUCCESS)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_EXECUTION_FAILED;
        }

        status = bal_engine_run_thread(&engine);

        if (BAL_ERROR_UNKNOWN_INSTRUCTION == status)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_UNKNOWN_INSTRUCTION;
        }
        else if (status != BAL_SUCCESS)
        {
            response.status = BAL_FUZZER_WORKER_ERROR_EXECUTION_FAILED;
        }
        else
        {
            response.status = BAL_FUZZER_WORKER_OK;
        }

        bal_fuzzer_state_capture_bal_cpu(&response.final_state, &cpu);
        bal_engine_destroy(&engine);
        (void)bal_flat_translation_interface_destroy(&allocator, &memory_interface);

        if (write_exact(STDOUT_FILENO, &response, sizeof(response)) != 0)
        {
            return 0;
        }
    }
}

int
read_exact(const int fd, void *BAL_RESTRICT data, const size_t size)
{
    uint8_t *BAL_RESTRICT cursor    = (uint8_t *)data;
    size_t                remaining = size;

    while (remaining > 0U)
    {
        const ssize_t bytes_read = read(fd, cursor, remaining);

        if (bytes_read <= 0)
        {
            return -1;
        }

        cursor += (size_t)bytes_read;
        remaining -= (size_t)bytes_read;
    }

    return 0;
}

int
write_exact(const int fd, const void *data, const size_t size)
{
    const uint8_t *cursor    = (const uint8_t *)data;
    size_t         remaining = size;

    while (remaining > 0U)
    {
        const ssize_t bytes_read = write(fd, cursor, remaining);

        if (bytes_read <= 0)
        {
            return -1;
        }

        cursor += (size_t)bytes_read;
        remaining -= (size_t)bytes_read;
    }

    return 0;
}
