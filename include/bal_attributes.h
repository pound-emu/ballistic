/** @file bal_attributes.h
 *
 * @brief Compiler specific attribute macros.
 */

#ifndef BALLISTIC_ATTRIBUTES_H
#define BALLISTIC_ATTRIBUTES_H

#include "bal_platform.h"

/*!
 * BAL_HOT()/BAL_COLD()
 * Marks a function as hot or cold. Hot makes the compiller optimize it more
 * aggressively. Cold marks the function as rarely executed.
 *
 * Usage:
 * BAL_HOT bal_error_t emit_instruction(...);
 */

#if BAL_COMPILER_GCC

#define BAL_HOT  __attribute__((hot))
#define BAL_COLD __attribute__((cold))

#else

#define BAL_HOT
#define BAL_COLD

#endif

/*!
 * BAL_LIKELY(x)/BAL_UNLIKELY(x)
 * Hints to the CPU branch predictor. Should only be used in hot functions.
 *
 * Usage: if (BAL_UNLIKELY(ptr == NULL)) { ... }
 */

#if BAL_COMPILER_GCC

#define BAL_LIKELY(x)   __builtin_expect(!!(x), 1)
#define BAL_UNLIKELY(x) __builtin_expect(!!(x), 0)

#else

#define BAL_LIKELY(x)   (x)
#define BAL_UNLIKELY(x) (x)

#endif

/*!
 * BAL_ALIGNED(x)
 * Aligns a variable or a structure to x bytes.
 *
 * Usage: BAL_ALIGNED(64)  struct data { ... };
 */

#if BAL_COMPILER_GCC

#define BAL_ALIGNED(x) __attribute__((aligned(x)))

#elif BAL_COMPILER_MSVC

#define BAL_ALIGNED(x) __declspec(align(x))

#else

#define BAL_ALIGNED(x)

#endif

/*!
 * BAL_RESTRICT:
 * Tells the compiler that a pointer does not alias any other pointer in
 * current scope.
 */

#if BAL_COMPILER_GCC

#define BAL_RESTRICT __restrict__

#elif BAL_COMPILER_MSVC

#define BAL_RESTRICT __restrict

#else

#define BAL_RESTRICT

#endif

#endif /* BALLISTIC_ATTRIBUTES_H */

/*** end of file ***/
