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

#endif /* BALLISTIC_ATTRIBUTES_H */

/*** end of file ***/
