#ifndef LOG_H
#define LOG_H

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

int write_full(int fd, const void* buf, size_t len);

void write_log_record(runtime_t* runtime, uint64_t ts_ns, uint64_t value,
                      unsigned long checksum, uint16_t thread_id,
                      uint16_t sleep_ms);

void make_log_path(runtime_t* runtime);

void print_stats(runtime_t* runtime);

#endif
