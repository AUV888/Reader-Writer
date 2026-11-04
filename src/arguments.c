#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/arguments.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Print accepted command-line syntax.
 *
 * @param[in] program_name Name used to invoke the executable.
 */
static void print_usage(const char* program_name) {
    fprintf(stderr,
            "Usage: %s --reader <count> --writer <count>\n"
            "  -r, --reader <count>  number of reader threads\n"
            "  -w, --writer <count>  number of writer threads\n",
            program_name);
}

/**
 * @brief Convert one positive decimal CLI value to an int.
 *
 * @param[in] text Option value supplied by the user.
 * @param[in] option_name Option name used in validation messages.
 * @param[out] value Parsed positive count.
 * @return 1 on success, or 0 after printing a diagnostic.
 */
static int parse_positive_count(const char* text, const char* option_name,
                                int* value) {
    char* end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < 1 ||
        parsed > INT_MAX) {
        fprintf(stderr,
                "%s must be a positive decimal integer, got '%s'\n",
                option_name, text);
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

int arguments_parse(int argc, char* argv[], arguments_t* arguments) {
    static const struct option long_options[] = {
        {"reader", required_argument, NULL, 'r'},
        {"writer", required_argument, NULL, 'w'},
        {NULL, 0, NULL, 0},
    };
    int have_reader = 0;
    int have_writer = 0;
    int option;
    const char* program_name;

    if (!arguments || argc < 1 || !argv || !argv[0]) {
        return -1;
    }

    program_name = argv[0];
    arguments->reader_count = 0;
    arguments->writer_count = 0;
    optind = 1;
    opterr = 0;

    while ((option = getopt_long(argc, argv, "r:w:", long_options, NULL)) !=
           -1) {
        switch (option) {
        case 'r':
            if (!parse_positive_count(optarg, "--reader",
                                      &arguments->reader_count)) {
                return -1;
            }
            have_reader = 1;
            break;
        case 'w':
            if (!parse_positive_count(optarg, "--writer",
                                      &arguments->writer_count)) {
                return -1;
            }
            have_writer = 1;
            break;
        case '?':
            if (optopt != 0) {
                fprintf(stderr, "Unknown option or missing value: -%c\n",
                        optopt);
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[optind - 1]);
            }
            print_usage(program_name);
            return -1;
        default:
            print_usage(program_name);
            return -1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected positional argument: %s\n", argv[optind]);
        print_usage(program_name);
        return -1;
    }

    if (!have_reader || !have_writer) {
        if (!have_reader) {
            fprintf(stderr, "Missing required option: --reader/-r\n");
        }
        if (!have_writer) {
            fprintf(stderr, "Missing required option: --writer/-w\n");
        }
        print_usage(program_name);
        return -1;
    }

    return 0;
}
