#define _POSIX_C_SOURCE 200809L

#include "../include/log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../include/util.h"

int write_full(int fd, const void* buf, size_t len) {
    const unsigned char* p = (const unsigned char*)buf;
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, p + off, len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        off += (size_t)n;
    }

    return 0;
}

void write_log_record(runtime_t* runtime, uint64_t ts_ns, uint64_t value,
                      unsigned long checksum, uint16_t thread_id,
                      uint64_t latency_ns) {
    uint64_t rec[4];

    rec[0] = ts_ns;
    rec[1] = value;

    rec[2] = ((uint64_t)(uint32_t)checksum << 32) |
             ((uint64_t)(thread_id & 0xFFFFU) << 16);
    rec[3] = latency_ns;

    pthread_mutex_lock(&runtime->log_mutex);
    if (runtime->log_fd >= 0) {
        (void)write_full(runtime->log_fd, rec, sizeof(rec));
    }
    pthread_mutex_unlock(&runtime->log_mutex);
}

void make_log_path(runtime_t* runtime) {
    time_t now = time(NULL);
    struct tm tmv;

    if (localtime_r(&now, &tmv) == NULL) {
        snprintf(runtime->log_path, sizeof(runtime->log_path), "unknown.log");
        return;
    }

    strftime(runtime->log_path, sizeof(runtime->log_path),
             "%Y_%m_%d_%H%M%S.log", &tmv);
}

void print_stats(runtime_t* runtime) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double run_s =
        (double)timespec_diff_ns(&runtime->start_mono, &now) / 1e9;

    uint64_t reads = (uint64_t)atomic_load(&runtime->total_reads);
    uint64_t writes = (uint64_t)atomic_load(&runtime->total_writes);
    uint64_t errors = (uint64_t)atomic_load(&runtime->checksum_errors);

    uint64_t max_read_lat =
        (uint64_t)atomic_load(&runtime->max_read_latency_ns);
    uint64_t max_write_lat =
        (uint64_t)atomic_load(&runtime->max_write_latency_ns);

    printf("\n================ STATISTICS ================\n");
    printf("Log file              : %s\n", runtime->log_path);
    printf("Run time              : %.3f s\n", run_s);

    printf("Total reads           : %llu\n", (unsigned long long)reads);
    printf("Total writes          : %llu\n", (unsigned long long)writes);

    if (run_s > 0.0) {
        printf("Read QPS              : %.2f\n", (double)reads / run_s);
        printf("Write QPS             : %.2f\n", (double)writes / run_s);
    } else {
        printf("Read QPS              : 0.00\n");
        printf("Write QPS             : 0.00\n");
    }

    printf("Max read latency      : %llu ns (%.3f ms)\n", (unsigned long long)max_read_lat,
           (double)max_read_lat / 1e6);

    printf("Max write latency     : %llu ns (%.3f ms)\n", (unsigned long long)max_write_lat,
           (double)max_write_lat / 1e6);

    printf("Checksum errors       : %llu\n", (unsigned long long)errors);
    printf("============================================\n");
}
