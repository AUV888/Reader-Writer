#ifndef RUNTIME_H
#define RUNTIME_H

#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

#define LOG_PATH_SIZE 128

typedef struct {
    pthread_mutex_t log_mutex;
    sem_t mut;
    sem_t wrt;
    long read_count;

    int log_fd;
    char log_path[LOG_PATH_SIZE];
    struct timespec start_mono;

    atomic_uint_least64_t total_reads;
    atomic_uint_least64_t total_writes;
    atomic_uint_least64_t checksum_errors;
    atomic_uint_least64_t max_read_latency_ns;
    atomic_uint_least64_t max_write_latency_ns;
} runtime_t;

#endif
