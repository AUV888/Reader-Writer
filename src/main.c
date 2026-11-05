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

#include "../include/arguments.h"
#include "../include/log.h"
#include "../include/rand.h"
#include "../include/runtime.h"
#include "../include/structs.h"
#include "../include/util.h"
#include "../include/worker.h"

static volatile sig_atomic_t g_stop = 0;

/**
 * @brief Request an orderly shutdown of all worker threads.
 *
 * SIGINT is redirected to this handler, which only sets an async-signal-safe
 * flag. Worker threads observe the flag and leave their main loops.
 *
 * @param[in] sig Signal number supplied by the C runtime.
 */
static void sigint_handler(int sig) {
    (void)sig;
    g_stop = 1;
}

/**
 * @brief Run the reader-writer simulation until SIGINT is received.
 *
 * The function installs the signal handler, initializes shared runtime state,
 * opens the binary log, starts the requested number of readers and writers,
 * waits for shutdown, joins every thread, and prints final statistics.
 *
 * @return EXIT_SUCCESS after a completed simulation, or EXIT_FAILURE if
 * initialization or thread creation fails.
 */
int main(int argc, char* argv[]) {
    arguments_t arguments;
    if (arguments_parse(argc, argv, &arguments) != 0) {
        return EXIT_FAILURE;
    }
    if (mode == MODE_FIRST_ADAPTIVE) {
        fprintf(stderr,
                "Adaptive scheduling (mode=3) is reserved but not "
                "implemented yet\n");
        return EXIT_FAILURE;
    }

    const int reader_size = arguments.reader_count;
    const int writer_size = arguments.writer_count;
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
        .writer_count = 0,
        .log_fd = -1,
        .total_reads = 0,
        .total_writes = 0,
        .checksum_errors = 0,
        .max_read_latency_ns = 0,
        .max_write_latency_ns = 0,
    };

    sem_init(&runtime.mut, 0, 1);
    sem_init(&runtime.wrt, 0, 1);
    sem_init(&runtime.reader_gate, 0, 1);
    sem_init(&runtime.writer_mutex, 0, 1);

    make_log_path(&runtime);

    runtime.log_fd =
        open(runtime.log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (runtime.log_fd < 0) {
        perror("open log file");
        return EXIT_FAILURE;
    }

    printf("Readers: %d, Writers: %d, Mode: %d\n", reader_size,
           writer_size, mode);
    printf("Log file: %s\n", runtime.log_path);
    fflush(stdout);

    clock_gettime(CLOCK_MONOTONIC, &runtime.start_mono);

    shared_data_t data;
    data.value = 0;
    data.checksum = 0 ^ CHECKSUM_MAGIC_NUMBER;

    pthread_t* reader_threads =
        malloc((size_t)reader_size * sizeof(*reader_threads));
    pthread_t* writer_threads =
        malloc((size_t)writer_size * sizeof(*writer_threads));
    if (!reader_threads || !writer_threads) {
        perror("malloc thread handles");
        free(reader_threads);
        free(writer_threads);
        return EXIT_FAILURE;
    }

    uint64_t base_seed = (uint64_t)time(NULL);
    if (base_seed == 0) {
        base_seed = 0x123456789ABCDEF0ULL;
    }

    /* Reader log IDs start at zero. */
    for (int i = 0; i < reader_size; i++) {
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

        if (pthread_create(&reader_threads[i], NULL, reader, args) != 0) {
            perror("pthread_create reader");
            return EXIT_FAILURE;
        }
    }

    /* Writer log IDs start at 100 to keep the roles distinguishable. */
    for (int i = 0; i < writer_size; i++) {
        args_t* args = malloc(sizeof(*args));
        if (!args) {
            perror("malloc writer args");
            return EXIT_FAILURE;
        }

        args->i = i;
        args->log_id = (uint16_t)(100 + i);
        args->d = &data;
        args->seed =
            base_seed ^
            (0x9E3779B97F4A7C15ULL *
             ((uint64_t)reader_size + (uint64_t)i + 1));
        args->runtime = &runtime;
        args->stop = &g_stop;

        if (args->seed == 0) {
            args->seed = 0xBADBEEFCAFED00DULL;
        }

        if (pthread_create(&writer_threads[i], NULL, writer, args) != 0) {
            perror("pthread_create writer");
            return EXIT_FAILURE;
        }
    }

    /* Wait for SIGINT */
    while (!g_stop) {
        pause();
    }

    for (int i = 0; i < reader_size; i++) {
        pthread_join(reader_threads[i], NULL);
    }

    for (int i = 0; i < writer_size; i++) {
        pthread_join(writer_threads[i], NULL);
    }

    free(reader_threads);
    free(writer_threads);
    print_stats(&runtime);

    if (runtime.log_fd >= 0) {
        fsync(runtime.log_fd);
        close(runtime.log_fd);
    }

    sem_destroy(&runtime.mut);
    sem_destroy(&runtime.wrt);
    sem_destroy(&runtime.reader_gate);
    sem_destroy(&runtime.writer_mutex);
    pthread_mutex_destroy(&runtime.log_mutex);

    return EXIT_SUCCESS;
}
