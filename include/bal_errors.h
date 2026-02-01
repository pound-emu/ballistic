/** @file bal_errors.h
 *
 * @brief Defines Ballistic error types.
 */

#ifndef BAL_ERRORS_H
#define BAL_ERRORS_H

#include "bal_attributes.h"

typedef enum
{
    // General Errors.
    //
    BAL_SUCCESS                    = 0,
    BAL_ERROR_INVALID_ARGUMENT     = -1,
    BAL_ERROR_ALLOCATION_FAILED    = -2,
    BAL_ERROR_MEMORY_ALIGNMENT     = -3,
    BAL_ERROR_ENGINE_STATE_INVALID = -4,
    BAL_ERROR_UNKNOWN_INSTRUCTION  = -5,

    // IR Errors.
    //
    BAL_ERROR_INSTRUCTION_OVERFLOW = -100,
} bal_error_t;

/// Converts the enum into a readable string for error handling.
BAL_COLD const char* bal_error_to_string(bal_error_t error);

#endif /* BAL_ERRORS_H */

/*** end of file ***/
