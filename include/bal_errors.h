/** @file bal_errors.h
 *
 * @brief Defines Ballistic error types.
 */

#ifndef BAL_ERRORS_H
#define BAL_ERRORS_H

typedef enum
{
    // General Errors.
    //
    BAL_SUCCESS                    = 0,
    BAL_ERROR_INVALID_ARGUMENT     = -1,
    BAL_ERROR_ALLOCATION_FAILED    = -2,
    BAL_ERROR_MEMORY_ALIGNMENT     = -3,
    BAL_ERROR_ENGINE_STATE_INVALID = -4,

    // IR Errors.
    //
    BAL_ERROR_INSTRUCTION_OVERFLOW = -100,
} bal_error_t;

#endif /* BAL_ERRORS_H */

/*** end of file ***/
