#include "../include/rand.h"

#include <stdint.h>

uint64_t prng_next(uint64_t* state) {
    /* Simple xorshift64, not cryptographic, enough for random delays */
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

uint64_t prng_range(uint64_t* state, uint64_t mod) {
    if (mod == 0)
        return 0;
    return prng_next(state) % mod;
}