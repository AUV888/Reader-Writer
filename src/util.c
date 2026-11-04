#define _POSIX_C_SOURCE 200809L

#include "../include/util.h"

#include <stdio.h>
#include <stdlib.h>

uint64_t timespec_diff_ns(const struct timespec* start,
                          const struct timespec* end) {
    long long sec = (long long)end->tv_sec - (long long)start->tv_sec;
    long long nsec = (long long)end->tv_nsec - (long long)start->tv_nsec;
    long long total = sec * 1000000000LL + nsec;
    if (total < 0)
        total = 0;
    return (uint64_t)total;
}

void update_max(atomic_uint_least64_t* max_val, uint64_t value) {
    uint_least64_t old = atomic_load(max_val);
    uint_least64_t desired = (uint_least64_t)value;

    while (old < desired) {
        if (atomic_compare_exchange_weak(max_val, &old, desired)) {
            break;
        }
    }
}

void sem_wait_retry(sem_t* sem) {
    while (sem_wait(sem) != 0) {
        if (errno == EINTR) {
            continue;
        }
        perror("sem_wait");
        exit(EXIT_FAILURE);
    }
}

void sleep_us(uint32_t us, const volatile sig_atomic_t* stop) {
    if (us == 0)
        return;

    struct timespec req;
    req.tv_sec = us / 1000000U;
    req.tv_nsec = (us % 1000000U) * 1000L;

    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        if (*stop) {
            break;
        }
    }
}

uint64_t now_ns_in_day(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    /*
     * Keep only time within one day.
     * This uses UTC day boundary. If local timezone midnight is required,
     * replace this with localtime-based calculation.
     */
    uint64_t sec_in_day = (uint64_t)ts.tv_sec % 86400ULL;
    return sec_in_day * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
