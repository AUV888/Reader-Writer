#ifndef RAND_H
#define RAND_H

/**
 * @file rand.h
 * @brief Small non-cryptographic pseudo-random number helpers.
 */

#include <stdint.h>

/**
 * @brief Advance an xorshift64 generator and return its next value.
 *
 * @param[in,out] state Generator state; it is updated in place.
 * @return The next pseudo-random 64-bit value.
 *
 * @warning This generator is suitable for simulation only and must not be
 * used for security-sensitive purposes.
 * @warning A zero state remains zero forever. Callers should provide a
 * non-zero initial seed.
 */
uint64_t prng_next(uint64_t* state);

/**
 * @brief Return a pseudo-random value in the range [0, mod).
 *
 * @param[in,out] state Generator state; it is updated in place.
 * @param[in] mod Exclusive upper bound.
 * @return A value in [0, mod), or 0 when @p mod is zero.
 */
uint64_t prng_range(uint64_t* state, uint64_t mod);

#endif
