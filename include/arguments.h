#ifndef ARGUMENTS_H
#define ARGUMENTS_H

/**
 * @file arguments.h
 * @brief Command-line configuration for the reader-writer simulation.
 */

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
 * positive decimal integer.
 *
 * @param[in] argc Number of command-line arguments.
 * @param[in] argv Command-line argument vector.
 * @param[out] arguments Parsed and validated thread counts.
 * @return 0 on success, or -1 after printing an error and usage information.
 */
int arguments_parse(int argc, char* argv[], arguments_t* arguments);

#endif
