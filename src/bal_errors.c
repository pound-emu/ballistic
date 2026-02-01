#include "bal_errors.h"
#include <stdio.h>

const char* bal_error_to_string(bal_error_t error)
{
    const char* string = "";
    switch (error)
    {
        case BAL_ERROR_INVALID_ARGUMENT:
           string = "function argument is NULL or invalid";
           break;
        case BAL_ERROR_ALLOCATION_FAILED:
           string = "failed to allocate memory";
           break;
        case BAL_ERROR_MEMORY_ALIGNMENT:
           string = "buffer is not aligned to the required memory alignment";
           break;
        case BAL_ERROR_ENGINE_STATE_INVALID:
            string = "the ballistic engine != BAL_SUCCESS";
           break;
        case BAL_ERROR_UNKNOWN_INSTRUCTION:
           string = "failed to decode arm instruction";
           break;
        case BAL_ERROR_INSTRUCTION_OVERFLOW:
           string = "instructions array overflowed";
           break;
        case BAL_SUCCESS:
           string = "there is no error";
           break;
    }

    return string;
}

/*** end of file ***/
