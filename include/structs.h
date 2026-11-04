#ifndef STRUCTS_H
#define STRUCTS_H

#include <signal.h>
#include <stdint.h>

#include "runtime.h"

#define CHECKSUM_MAGIC_NUMBER 0x00000000DEADBEEFULL

typedef struct {
    unsigned long value;
    unsigned long checksum;
} shared_data_t;

typedef struct {
    int i;
    uint16_t log_id;
    shared_data_t* d;
    uint64_t seed;
    runtime_t* runtime;
    const volatile sig_atomic_t* stop;
} args_t;

#endif
