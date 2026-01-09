/** @file module.c   
 * 
 * @brief A description of the module’s purpose.    
 * 
 * @par          
 */    

#include <stdint.h> 
#include <stdbool.h> 

#include “module.h”    

/*!   
 * @brief Identify the larger of two 8-bit integers.   
 * 
 * @param[in] num1  The first number to be compared.   
 * @param[in] num2  The second number to be compared.   
 * 
 * @return The value of the larger number. 
 */  
int8_t
max8 (int8_t num1, int8_t num2)
{      
    return ((num1 > num2) ? num1 : num2); 
}   

/*** end of file ***/
