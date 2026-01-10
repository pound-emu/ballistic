/** @file bal_errors.h
 *
 * @brief Defines Ballistic error types.       
 */     

#ifndef BAL_ERRORS_H
#define BAL_ERRORS_H  

typedef enum
{
    BAL_SUCCESS = 0,
    BAL_ERROR_INVALID_ARGUMENT = -1,
    BAL_ERROR_ALLOCATION_FAILED = -2,
} bal_error_t;

#endif /* BAL_ERRORS_H */   

/*** end of file ***/
