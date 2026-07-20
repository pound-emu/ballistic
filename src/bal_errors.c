#include "bal_errors.h"

const char *
bal_error_to_string(const bal_error_t error)
{
    switch (error)
    {
        case BAL_SUCCESS:
            return "there is no error";
        case BAL_ERROR_INVALID_ARGUMENT:
            return "function argument is NULL or invalid";
        case BAL_ERROR_ALLOCATION_FAILED:
            return "failed to allocate memory";
        case BAL_ERROR_STRUCT_CORRUPTED:
            return "struct integrity check failed";
        case BAL_ERROR_BUFFER_OVERFLOW:
            return "buffer overflow";

        // --- Memory & Translation ---
        case BAL_ERROR_MEMORY_ALIGNMENT:
            return "buffer is not aligned to the required memory alignment";
        case BAL_ERROR_MEMORY_FAULT:
            return "accessed invalid or unmapped guest memory";

            // --- Engine & Execution ---
        case BAL_ERROR_ENGINE_ALREADY_RUNNING:
            return "engine is already executing guest code";
        case BAL_ERROR_PC_ALIGNMENT:
            return "guest code tried to execute an unaligned instruction";
        case BAL_ERROR_UNKNOWN_INSTRUCTION:
            return "failed to decode ARM instruction";

            // --- Threading ---
        case BAL_ERROR_THREAD_CREATION:
            return "could not create background thread";
        case BAL_ERROR_THREAD_CLEANUP:
            return "could not clean up background thread";

            // --- IR / Assembler ---
        case BAL_ERROR_INSTRUCTION_OVERFLOW:
            return "instruction buffer or IR arena overflowed";
        case BAL_ERROR_INCORRECT_REGISTER_TYPE:
            return "decoded register type mismatch";
        case BAL_ERROR_BRANCH_OFFSET_OVERFLOW:
            return "relative branch offset exceeds displacement limit";
        case BAL_ERROR_CAPACITY_TOO_BIG:
            return "buffer capacity is too large and would cause integer overflow";

        default:
            return "unknown error code";
    }
}

/*** end of file ***/
