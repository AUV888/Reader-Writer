#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../include/log.h"
#include "../include/rand.h"
#include "../include/runtime.h"
#include "../include/structs.h"
#include "../include/util.h"
#include "../include/worker.h"

#define READER_SIZE 5
#define WRITER_SIZE 3

static volatile sig_atomic_t g_stop = 0;

static void sigint_handler(int sig) {
    (void)sig;
    g_stop = 1;
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);

    /*
     * Do not set SA_RESTART, so nanosleep/pause can be interrupted
     * and the program can stop faster after SIGINT.
     */
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    runtime_t runtime = {
        .log_mutex = PTHREAD_MUTEX_INITIALIZER,
        .read_count = 0,
        .log_fd = -1,
        .total_reads = 0,
        .total_writes = 0,
        .checksum_errors = 0,
        .max_read_latency_ns = 0,
        .max_write_latency_ns = 0,
    };

    sem_init(&runtime.mut, 0, 1);
    sem_init(&runtime.wrt, 0, 1);

    make_log_path(&runtime);

    runtime.log_fd =
        open(runtime.log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (runtime.log_fd < 0) {
        perror("open log file");
        return EXIT_FAILURE;
    }

    printf("Log file: %s\n", runtime.log_path);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &runtime.start_mono);

    shared_data_t data;
    data.value = 0;
    data.checksum = 0 ^ CHECKSUM_MAGIC_NUMBER;

    pthread_t r[READER_SIZE];
    pthread_t w[WRITER_SIZE];

    uint64_t base_seed = (uint64_t)time(NULL);
    if (base_seed == 0) {
        base_seed = 0x123456789ABCDEF0ULL;
    }

    /* Create readers: thread id 0..4 */
    for (int i = 0; i < READER_SIZE; i++) {
        args_t* args = malloc(sizeof(*args));
        if (!args) {
            perror("malloc reader args");
            return EXIT_FAILURE;
        }

        args->i = i;
        args->log_id = (uint16_t)i;
        args->d = &data;
        args->seed = base_seed ^ (0x9E3779B97F4A7C15ULL * (uint64_t)(i + 1));
        args->runtime = &runtime;
        args->stop = &g_stop;

        if (args->seed == 0) {
            args->seed = 0xDEADBEEFCAFEBABEULL;
        }

        if (pthread_create(&r[i], NULL, reader, args) != 0) {
            perror("pthread_create reader");
            return EXIT_FAILURE;
        }
    }

    /* Create writers: thread id 100..102 to avoid conflict with readers */
    for (int i = 0; i < WRITER_SIZE; i++) {
        args_t* args = malloc(sizeof(*args));
        if (!args) {
            perror("malloc writer args");
            return EXIT_FAILURE;
        }

        args->i = i;
        args->log_id = (uint16_t)(100 + i);
        args->d = &data;
        args->seed = base_seed ^ (0x9E3779B97F4A7C15ULL * (uint64_t)(READER_SIZE + i + 1));
        args->runtime = &runtime;
        args->stop = &g_stop;

        if (args->seed == 0) {
            args->seed = 0xBADBEEFCAFED00DULL;
        }

        if (pthread_create(&w[i], NULL, writer, args) != 0) {
            perror("pthread_create writer");
            return EXIT_FAILURE;
        }
    }

    /* Wait for SIGINT */
    while (!g_stop) {
        pause();
    }

    for (int i = 0; i < READER_SIZE; i++) {
        pthread_join(r[i], NULL);
    }

    for (int i = 0; i < WRITER_SIZE; i++) {
        pthread_join(w[i], NULL);
    }

    print_stats(&runtime);

    if (runtime.log_fd >= 0) {
        fsync(runtime.log_fd);
        close(runtime.log_fd);
    }

    sem_destroy(&runtime.mut);
    sem_destroy(&runtime.wrt);
    pthread_mutex_destroy(&runtime.log_mutex);

    return EXIT_SUCCESS;
}
