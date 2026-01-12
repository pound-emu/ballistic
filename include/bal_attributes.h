/** @file bal_attributes.h
 *
 * @brief Compiler specific attribute macros.
 */

#ifndef BALLISTIC_ATTRIBUTES_H
#define BALLISTIC_ATTRIBUTES_H

/*
 * BAL_HOT()/BAL_COLD()
 * Marks a function as hot or cold. Hot makes the compiller optimize it more
 * aggressively. Cold marks the function as rarely executed.
 *
 * Usage:
 * BAL_HOT bal_error_t emit_instruction(...);
 */

#if defined(__GNUC__) || defined(__clang__)

#define BAL_HOT  __attribute__((hot))
#define BAL_COLD __attribute__((cold))

#else

#define BAL_HOT
#define BAL_COLD

#endif

/*
 * BAL_LIKELY(x)/BAL_UNLIKELY(x)
 * Hints to the CPU branch predictor. Should only be used in hot functions.
 *
 * Usage: if (BAL_UNLIKELY(ptr == NULL)) { ... }
 */

#if defined(__GNUC__) || defined(__clang__)

#define BAL_LIKELY(x)   __builtin_expect(!!(x), 1)
#define BAL_UNLIKELY(x) __builtin_expect(!!(x), 0)

#else

#define BAL_LIKELY(x)   (x)
#define BAL_UNLIKELY(x) (x)

#endif 


#endif /* BALLISTIC_ATTRIBUTES_H */

/*** end of file ***/
