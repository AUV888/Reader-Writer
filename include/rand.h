#ifndef RAND_H
#define RAND_H

#include <stdint.h>

uint64_t prng_next(uint64_t* state);

uint64_t prng_range(uint64_t* state, uint64_t mod);

#endif