#ifndef BALLISTIC_BAL_FUZZER_PROTOCOL_H
#define BALLISTIC_BAL_FUZZER_PROTOCOL_H

#include "backend/bal_cpu.h"
#include "bal_attributes.h"
#include <stdint.h>

#define BAL_FUZZER_MAX_INSTRUCTIONS 60U

BAL_ALIGNED(64) typedef struct
{
    uint64_t x[32];
    uint64_t pc;
    uint8_t  flag_c;
    uint8_t  flag_z;
    uint8_t  flag_n;
    uint8_t  flag_v;
    uint32_t pad0;
    uint64_t instruction_count;
    uint64_t pad1[5];
} bal_fuzzer_cpu_snapshot_t;

static_assert(sizeof(bal_fuzzer_cpu_snapshot_t) == sizeof(bal_cpu_t), "Struct size mismatch");

typedef enum
{
    BAL_FUZZER_WORKER_OK = 0,
    BAL_FUZZER_WORKER_ERROR_COMPILE_FAILED,
    BAL_FUZZER_WORKER_ERROR_EXECUTION_FAILED,
    BAL_FUZZER_WORKER_ERROR_TIMEOUT,
    BAl_FUZZER_WORKER_ERROR_CRASHED,
    BAL_FUZZER_WORKER_ERROR_UNKNOWN_INSTRUCTION,
} bal_fuzzer_worker_status_t;

/// Fuzz input sent to the worker.
BAL_ALIGNED(64) typedef struct
{
    uint64_t                  message_id;
    uint32_t                  instruction_count;
    uint32_t                  pad;
    uint32_t                  instructions[BAL_FUZZER_MAX_INSTRUCTIONS];
    bal_fuzzer_cpu_snapshot_t initial_state;
} bal_fuzzer_input_t;

/// Response sent from the worker.
BAL_ALIGNED(64) typedef struct
{
    uint64_t                   message_id; // Must match the input message_id.
    bal_fuzzer_worker_status_t status;
    char                       pad[52];
    bal_fuzzer_cpu_snapshot_t  final_state;
} bal_fuzzer_response_t;

#endif // BALLISTIC_BAL_FUZZER_PROTOCOL_H

/*** end of file ***/