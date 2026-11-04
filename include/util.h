#ifndef UTIL_H
#define UTIL_H

/**
 * @file util.h
 * @brief Timing, synchronization, and signal-aware utility functions.
 */

#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

/**
 * @brief Calculate the elapsed time between two POSIX timestamps.
 *
 * @param[in] start Earlier timestamp.
 * @param[in] end Later timestamp.
 * @return Elapsed time in nanoseconds, clamped to zero if @p end precedes
 * @p start.
 */
uint64_t timespec_diff_ns(const struct timespec* start,
                          const struct timespec* end);

/**
 * @brief Atomically raise a maximum counter to at least a given value.
 *
 * This function uses a compare-and-exchange loop and is safe for concurrent
 * callers operating on the same atomic value.
 *
 * @param[in,out] max_val Atomic maximum value to update.
 * @param[in] value Candidate value.
 */
void update_max(atomic_uint_least64_t* max_val, uint64_t value);

/**
 * @brief Wait on a semaphore while transparently retrying EINTR.
 *
 * @param[in,out] sem Semaphore to wait on.
 *
 * @warning On a non-interruption semaphore error, this function prints the
 * error and terminates the process with EXIT_FAILURE.
 */
void sem_wait_retry(sem_t* sem);

/**
 * @brief Sleep for a number of microseconds unless shutdown is requested.
 *
 * Interrupted sleeps are resumed with the remaining duration. The sleep ends
 * early when @p stop becomes non-zero.
 *
 * @param[in] us Duration in microseconds.
 * @param[in] stop Non-null pointer to the asynchronous shutdown flag.
 */
void sleep_us(uint32_t us, const volatile sig_atomic_t* stop);

/**
 * @brief Return the current UTC time as nanoseconds within its day.
 *
 * @return Nanoseconds since UTC midnight, in the range [0, 86400e9).
 */
uint64_t now_ns_in_day(void);

#endif
