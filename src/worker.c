#include "../include/worker.h"

#include <stdlib.h>
#include <time.h>

#include "../include/structs.h"
#include "../include/util.h"

void* reader(void* args) {
    args_t* ar = (args_t*)args;
    shared_data_t* data = ar->d;
    uint16_t log_id = ar->log_id;
    uint64_t seed = ar->seed;
    runtime_t* runtime = ar->runtime;
    const volatile sig_atomic_t* stop = ar->stop;

    free(ar);

    while (!*stop) {
        /*
         * Original random sleep at most 5s.
         * Store ms in low 16 bits of log record, so use 0..4999 ms.
         */
        uint32_t sleep_ms = (uint32_t)prng_range(&seed, 5000);
        sleep_us(sleep_ms * 1000U, stop);

        if (*stop) {
            break;
        }

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        sem_wait_retry(&runtime->mut);
        if (*stop) {
            sem_post(&runtime->mut);
            break;
        }

        runtime->read_count++;

        if (runtime->read_count == 1) {
            sem_wait_retry(&runtime->wrt);

            if (*stop) {
                /* Rollback first reader state before exit */
                runtime->read_count--;
                sem_post(&runtime->wrt);
                sem_post(&runtime->mut);
                break;
            }
        }

        sem_post(&runtime->mut);

        clock_gettime(CLOCK_MONOTONIC, &t1);

        /*
         * Read lock latency:
         * from before sem_wait(&mut) to acquiring full read permission.
         * For the first reader, this includes waiting on wrt.
         */
        update_max(&runtime->max_read_latency_ns,
                   timespec_diff_ns(&t0, &t1));

        /* Perform reading */
        unsigned long val = data->value;
        unsigned long chk = data->checksum;

        /* Read Biz time cost: random within 1ms = 0..999us */
        uint32_t biz_us = (uint32_t)prng_range(&seed, 1000);
        sleep_us(biz_us, stop);

        /* Release read permission */
        sem_wait_retry(&runtime->mut);
        runtime->read_count--;
        if (runtime->read_count == 0) {
            sem_post(&runtime->wrt);
        }
        sem_post(&runtime->mut);

        if ((val ^ CHECKSUM_MAGIC_NUMBER) != chk) {
            atomic_fetch_add(&runtime->checksum_errors, 1);
        }

        uint64_t ts = now_ns_in_day();

        atomic_fetch_add(&runtime->total_reads, 1);
        write_log_record(runtime, ts, (uint64_t)val, chk, log_id,
                         (uint16_t)sleep_ms);
    }

    return NULL;
}

void* writer(void* args) {
    args_t* ar = (args_t*)args;
    shared_data_t* data = ar->d;
    uint16_t log_id = ar->log_id;
    uint64_t seed = ar->seed;
    runtime_t* runtime = ar->runtime;
    const volatile sig_atomic_t* stop = ar->stop;

    free(ar);

    while (!*stop) {
        /*
         * Original random sleep at most 5s.
         * Store ms in low 16 bits of log record, so use 0..4999 ms.
         */
        uint32_t sleep_ms = (uint32_t)prng_range(&seed, 5000);
        sleep_us(sleep_ms * 1000U, stop);

        if (*stop) {
            break;
        }

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        sem_wait_retry(&runtime->wrt);
        if (*stop) {
            sem_post(&runtime->wrt);
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &t1);

        /* Write lock latency: from before sem_wait(&wrt) to acquiring wrt */
        update_max(&runtime->max_write_latency_ns,
                   timespec_diff_ns(&t0, &t1));

        /* Generate new value and checksum */
        uint64_t rnd64 = prng_next(&seed);
        unsigned long new_val = (unsigned long)rnd64;
        unsigned long new_chk = new_val ^ CHECKSUM_MAGIC_NUMBER;

        data->value = new_val;
        data->checksum = new_chk;

        /* Write Biz time cost: random within 5ms = 0..4999us */
        uint32_t biz_us = (uint32_t)prng_range(&seed, 5000);
        sleep_us(biz_us, stop);

        sem_post(&runtime->wrt);

        uint64_t ts = now_ns_in_day();

        atomic_fetch_add(&runtime->total_writes, 1);
        write_log_record(runtime, ts, (uint64_t)new_val, new_chk, log_id,
                         (uint16_t)sleep_ms);
    }

    return NULL;
}
