#include "../include/worker.h"

#include <stdlib.h>
#include <time.h>

#include "../include/arguments.h"
#include "../include/log.h"
#include "../include/rand.h"
#include "../include/structs.h"
#include "../include/util.h"

/**
 * @brief Acquire read access according to the selected scheduling mode.
 *
 * @param[in,out] runtime Shared synchronization state.
 * @param[in] stop Asynchronous shutdown flag.
 * @return 1 when read access is acquired, or 0 when shutdown interrupts it.
 */
static int acquire_read_access(runtime_t* runtime,
                               const volatile sig_atomic_t* stop) {
    if (mode == MODE_READER_FIRST) {
        sem_wait_retry(&runtime->mut);
        if (*stop) {
            sem_post(&runtime->mut);
            return 0;
        }

        runtime->read_count++;
        if (runtime->read_count == 1) {
            sem_wait_retry(&runtime->wrt);
            if (*stop) {
                runtime->read_count--;
                sem_post(&runtime->wrt);
                sem_post(&runtime->mut);
                return 0;
            }
        }

        sem_post(&runtime->mut);
        return 1;
    }

    if (mode == MODE_WRITER_FIRST) {
        sem_wait_retry(&runtime->reader_gate);
        if (*stop) {
            sem_post(&runtime->reader_gate);
            return 0;
        }

        sem_wait_retry(&runtime->mut);
        if (*stop) {
            sem_post(&runtime->mut);
            sem_post(&runtime->reader_gate);
            return 0;
        }

        runtime->read_count++;
        if (runtime->read_count == 1) {
            sem_wait_retry(&runtime->wrt);
            if (*stop) {
                runtime->read_count--;
                sem_post(&runtime->wrt);
                sem_post(&runtime->mut);
                sem_post(&runtime->reader_gate);
                return 0;
            }
        }

        sem_post(&runtime->mut);
        sem_post(&runtime->reader_gate);
        return 1;
    }

    return 0;
}

/**
 * @brief Release read access after all readers have finished.
 *
 * @param[in,out] runtime Shared synchronization state.
 */
static void release_read_access(runtime_t* runtime) {
    sem_wait_retry(&runtime->mut);
    runtime->read_count--;
    if (runtime->read_count == 0) {
        sem_post(&runtime->wrt);
    }
    sem_post(&runtime->mut);
}

/**
 * @brief Acquire write access according to the selected scheduling mode.
 *
 * In writer-first mode, the first waiting writer closes reader_gate so new
 * readers cannot enter while writers are pending.
 *
 * @param[in,out] runtime Shared synchronization state.
 * @param[in] stop Asynchronous shutdown flag.
 * @return 1 when write access is acquired, or 0 when shutdown interrupts it.
 */
static int acquire_write_access(runtime_t* runtime,
                                const volatile sig_atomic_t* stop) {
    if (mode == MODE_READER_FIRST) {
        sem_wait_retry(&runtime->wrt);
        if (*stop) {
            sem_post(&runtime->wrt);
            return 0;
        }
        return 1;
    }

    if (mode == MODE_WRITER_FIRST) {
        sem_wait_retry(&runtime->writer_mutex);
        if (*stop) {
            sem_post(&runtime->writer_mutex);
            return 0;
        }

        runtime->writer_count++;
        if (runtime->writer_count == 1) {
            sem_wait_retry(&runtime->reader_gate);
            if (*stop) {
                runtime->writer_count--;
                sem_post(&runtime->writer_mutex);
                sem_post(&runtime->reader_gate);
                return 0;
            }
        }
        sem_post(&runtime->writer_mutex);

        sem_wait_retry(&runtime->wrt);
        if (*stop) {
            sem_post(&runtime->wrt);
            sem_wait_retry(&runtime->writer_mutex);
            runtime->writer_count--;
            if (runtime->writer_count == 0) {
                sem_post(&runtime->reader_gate);
            }
            sem_post(&runtime->writer_mutex);
            return 0;
        }
        return 1;
    }

    return 0;
}

/**
 * @brief Release write access and reopen the reader gate when appropriate.
 *
 * @param[in,out] runtime Shared synchronization state.
 */
static void release_write_access(runtime_t* runtime) {
    if (mode == MODE_READER_FIRST) {
        sem_post(&runtime->wrt);
        return;
    }

    if (mode == MODE_WRITER_FIRST) {
        sem_post(&runtime->wrt);
        sem_wait_retry(&runtime->writer_mutex);
        runtime->writer_count--;
        if (runtime->writer_count == 0) {
            sem_post(&runtime->reader_gate);
        }
        sem_post(&runtime->writer_mutex);
    }
}

void* reader(void* args) {
    args_t* ar = (args_t*)args;
    shared_data_t* data = ar->d;
    uint16_t log_id = ar->log_id;
    uint64_t seed = ar->seed;
    runtime_t* runtime = ar->runtime;
    const volatile sig_atomic_t* stop = ar->stop;

    free(ar);

    while (!*stop) {
        /* Randomize the interval before this reader's next request. */
        uint32_t arrival_delay_ms = (uint32_t)prng_range(&seed, 5000);
        sleep_us(arrival_delay_ms * 1000U, stop);

        if (*stop) {
            break;
        }

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        if (!acquire_read_access(runtime, stop)) {
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &t1);

        /*
         * Read lock latency: from before the policy-specific lock
         * acquisition to obtaining complete read permission.
         */
        uint64_t latency_ns = timespec_diff_ns(&t0, &t1);
        update_max(&runtime->max_read_latency_ns, latency_ns);

        /* Perform reading */
        unsigned long val = data->value;
        unsigned long chk = data->checksum;

        /* Read Biz time cost */
        uint32_t biz_us = (uint32_t)prng_range(&seed, READER_BIZ_TIME);
        sleep_us(biz_us, stop);

        release_read_access(runtime);

        if ((val ^ CHECKSUM_MAGIC_NUMBER) != chk) {
            atomic_fetch_add(&runtime->checksum_errors, 1);
        }

        uint64_t ts = now_ns_in_day();

        atomic_fetch_add(&runtime->total_reads, 1);
        write_log_record(runtime, ts, (uint64_t)val, chk, log_id, latency_ns);
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
        /* Randomize the interval before this writer's next request. */
        uint32_t arrival_delay_ms = (uint32_t)prng_range(&seed, 5000);
        sleep_us(arrival_delay_ms * 1000U, stop);

        if (*stop) {
            break;
        }

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        if (!acquire_write_access(runtime, stop)) {
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &t1);

        /* Write lock latency: from before sem_wait(&wrt) to acquiring wrt */
        uint64_t latency_ns = timespec_diff_ns(&t0, &t1);
        update_max(&runtime->max_write_latency_ns, latency_ns);

        /* Generate new value and checksum */
        uint64_t rnd64 = prng_next(&seed);
        unsigned long new_val = (unsigned long)rnd64;
        unsigned long new_chk = new_val ^ CHECKSUM_MAGIC_NUMBER;

        data->value = new_val;
        data->checksum = new_chk;

        /* Write Biz time cost */
        uint32_t biz_us = (uint32_t)prng_range(&seed, WRITER_BIZ_TIME);
        sleep_us(biz_us, stop);

        release_write_access(runtime);

        uint64_t ts = now_ns_in_day();

        atomic_fetch_add(&runtime->total_writes, 1);
        write_log_record(runtime, ts, (uint64_t)new_val, new_chk, log_id, latency_ns);
    }

    return NULL;
}
