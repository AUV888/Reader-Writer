#ifndef LOG_H
#define LOG_H

/**
 * @file log.h
 * @brief Binary log writing, log-file naming, and runtime statistics.
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include "runtime.h"

/*
 * Binary log record format: 32 bytes = 4 * uint64_t
 *
 * [0] timestamp: ns within one day, here based on UTC midnight
 * [1] business payload: value read/written
 * [2] business payload:
 *     bits 63..32: checksum low 32 bits
 *     bits 31..16: thread id
 *     bits 15..0 : sleep_time in ms, 0..4999
 * [3] reserved: 0
 */

/**
 * @brief Write all requested bytes to a file descriptor.
 *
 * The function retries automatically when a write is interrupted by a signal.
 *
 * @param[in] fd Destination file descriptor.
 * @param[in] buf Buffer containing the bytes to write.
 * @param[in] len Number of bytes to write.
 * @return 0 on success, or -1 if a non-recoverable write error occurs.
 */
int write_full(int fd, const void* buf, size_t len);

/**
 * @brief Append one binary log record to the runtime log file.
 *
 * The record contains the timestamp, business value, checksum, thread ID,
 * and sleep duration. Log-file access is serialized by runtime->log_mutex.
 *
 * @param[in,out] runtime Runtime state containing the log descriptor and lock.
 * @param[in] ts_ns Timestamp in nanoseconds within the UTC day.
 * @param[in] value Value read or written by the worker.
 * @param[in] checksum Business checksum associated with @p value.
 * @param[in] thread_id Worker identifier stored in the record.
 * @param[in] sleep_ms Sleep duration in milliseconds.
 *
 * @note Write failures are currently ignored after the record is formatted.
 */
void write_log_record(runtime_t* runtime, uint64_t ts_ns, uint64_t value,
                      unsigned long checksum, uint16_t thread_id,
                      uint16_t sleep_ms);

/**
 * @brief Generate a timestamp-based log filename.
 *
 * The path is stored in runtime->log_path. If the local time cannot be
 * obtained, the function falls back to "unknown.log".
 *
 * @param[in,out] runtime Runtime state whose log path will be populated.
 */
void make_log_path(runtime_t* runtime);

/**
 * @brief Print throughput, latency, and checksum statistics.
 *
 * QPS values are calculated from the runtime start time and the current
 * monotonic clock.
 *
 * @param[in] runtime Runtime state containing the accumulated statistics.
 */
void print_stats(runtime_t* runtime);

#endif
