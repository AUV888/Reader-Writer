#ifndef UTIL_H
#define UTIL_H

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

uint64_t timespec_diff_ns(const struct timespec* start,
                          const struct timespec* end);

void update_max(atomic_uint_least64_t* max_val, uint64_t value);

void sem_wait_retry(sem_t* sem);

void sleep_us(uint32_t us, const volatile sig_atomic_t* stop);

uint64_t now_ns_in_day(void);

#endif
