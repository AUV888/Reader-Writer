#ifndef WORKER_H
#define WORKER_H

/**
 * @file worker.h
 * @brief Reader and writer thread entry points.
 */

#include <stdint.h>
/* High-contention workload with equal business time for both roles. */
#define ARRIVAL_DELAY_LIMIT_MS 20
#define READER_BIZ_TIME 5000
#define WRITER_BIZ_TIME 5000

/**
 * @brief Execute the reader-thread workload.
 *
 * The reader waits for a randomized delay, acquires shared read access,
 * validates the data checksum, records latency and throughput statistics,
 * and appends a log record. It exits when the shutdown flag is set.
 *
 * @param[in,out] args Heap-allocated args_t object. Ownership is transferred
 * to this function and the object is freed before the thread exits.
 * @return NULL on normal completion.
 *
 * @note @p args must point to a valid args_t and must not be NULL.
 */
void* reader(void* args);

/**
 * @brief Execute the writer-thread workload.
 *
 * The writer waits for a randomized delay, takes exclusive access, generates
 * a new value and checksum, records latency and throughput statistics, and
 * appends a log record. It exits when the shutdown flag is set.
 *
 * @param[in,out] args Heap-allocated args_t object. Ownership is transferred
 * to this function and the object is freed before the thread exits.
 * @return NULL on normal completion.
 *
 * @note @p args must point to a valid args_t and must not be NULL.
 */
void* writer(void* args);

#endif
