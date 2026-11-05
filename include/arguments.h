#ifndef ARGUMENTS_H
#define ARGUMENTS_H

/**
 * @file arguments.h
 * @brief Command-line configuration for the reader-writer simulation.
 */

/** No scheduling mode was selected. */
#define MODE_NONE 0

/** Reader-first scheduling mode. */
#define MODE_READER_FIRST 1

/** Writer-first scheduling mode. */
#define MODE_WRITER_FIRST 2

/** Adaptive scheduling mode reserved for future implementation. */
#define MODE_FIRST_ADAPTIVE 3

/**
 * @brief Selected scheduling mode.
 *
 * The value is set once during argument parsing and is read-only while worker
 * threads are running.
 */
extern int mode;

/**
 * @brief Validated runtime configuration supplied through the CLI.
 */
typedef struct {
    int reader_count;
    int writer_count;
} arguments_t;

/**
 * @brief Parse and validate GNU-style command-line arguments.
 *
 * Both --reader/-r and --writer/-w are required and must be followed by a
 * positive decimal integer. Exactly one scheduling flag must be supplied:
 * -fr/--fr, -fw/--fw, or -fa/--fa.
 *
 * @param[in] argc Number of command-line arguments.
 * @param[in] argv Command-line argument vector.
 * @param[out] arguments Parsed and validated thread counts.
 * @return 0 on success, or -1 after printing an error and usage information.
 */
int arguments_parse(int argc, char* argv[], arguments_t* arguments);

#endif
