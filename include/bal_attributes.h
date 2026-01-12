/** @file bal_attributes.h
 *
 * @brief Compiler specific attribute macros.
 */

#ifndef BALLISTIC_ATTRIBUTES_H
#define BALLISTIC_ATTRIBUTES_H

/*
 * Usage:
 * BAL_HOT bal_error_t emit_instruction(...);
 */

#if defined(__GNUC__) || defined(__clang__)

/*!
 * @brief Marks a function as hit. The compiler will optimize it more
 * aggresively.
 */
#define BAL_HOT __attribute__((hot))

#else

#define BAL_HOT

#endif

#endif /* BALLISTIC_ATTRIBUTES_H */

/*** end of file ***/
