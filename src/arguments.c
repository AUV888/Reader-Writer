#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../include/arguments.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int mode = MODE_NONE;

enum {
    OPTION_FIRST_READER = 1000,
    OPTION_FIRST_WRITER,
    OPTION_FIRST_ADAPTIVE,
};

/**
 * @brief Print accepted command-line syntax.
 *
 * @param[in] program_name Name used to invoke the executable.
 */
static void print_usage(const char* program_name) {
    fprintf(stderr,
            "Usage: %s --reader <count> --writer <count> "
            "(-fr|-fw|-fa)\n"
            "  -r, --reader <count>  number of reader threads\n"
            "  -w, --writer <count>  number of writer threads\n"
            "  -fr, --fr             reader-first scheduling\n"
            "  -fw, --fw             writer-first scheduling\n"
            "  -fa, --fa             adaptive scheduling (reserved)\n",
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

/**
 * @brief Select one scheduling mode without accepting conflicts.
 *
 * @param[in] selected_mode Mode represented by the current CLI flag.
 * @param[in] option_name CLI spelling used in validation messages.
 * @return 1 on success, or 0 if a different mode was already selected.
 */
static int select_mode(int selected_mode, const char* option_name) {
    if (mode != MODE_NONE && mode != selected_mode) {
        fprintf(stderr,
                "Conflicting scheduling options: mode is already %d; "
                "%s selects mode %d\n",
                mode, option_name, selected_mode);
        return 0;
    }

    mode = selected_mode;
    return 1;
}

int arguments_parse(int argc, char* argv[], arguments_t* arguments) {
    static const struct option long_options[] = {
        {"reader", required_argument, NULL, 'r'},
        {"writer", required_argument, NULL, 'w'},
        {"fr", no_argument, NULL, OPTION_FIRST_READER},
        {"fw", no_argument, NULL, OPTION_FIRST_WRITER},
        {"fa", no_argument, NULL, OPTION_FIRST_ADAPTIVE},
        {NULL, 0, NULL, 0},
    };
    int have_reader = 0;
    int have_writer = 0;
    int have_mode = 0;
    int option;
    const char* program_name;

    if (!arguments || argc < 1 || !argv || !argv[0]) {
        return -1;
    }

    program_name = argv[0];
    arguments->reader_count = 0;
    arguments->writer_count = 0;
    mode = MODE_NONE;
    optind = 1;
    opterr = 0;

    while ((option = getopt_long(argc, argv, "r:w:f:", long_options, NULL)) !=
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
        case 'f':
            if (optarg[0] == 'r' && optarg[1] == '\0') {
                if (!select_mode(MODE_READER_FIRST, "-fr")) {
                    return -1;
                }
            } else if (optarg[0] == 'w' && optarg[1] == '\0') {
                if (!select_mode(MODE_WRITER_FIRST, "-fw")) {
                    return -1;
                }
            } else if (optarg[0] == 'a' && optarg[1] == '\0') {
                if (!select_mode(MODE_FIRST_ADAPTIVE, "-fa")) {
                    return -1;
                }
            } else {
                fprintf(stderr,
                        "Unknown scheduling option: -f%s "
                        "(expected -fr, -fw, or -fa)\n",
                        optarg);
                return -1;
            }
            have_mode = 1;
            break;
        case OPTION_FIRST_READER:
            if (!select_mode(MODE_READER_FIRST, "--fr")) {
                return -1;
            }
            have_mode = 1;
            break;
        case OPTION_FIRST_WRITER:
            if (!select_mode(MODE_WRITER_FIRST, "--fw")) {
                return -1;
            }
            have_mode = 1;
            break;
        case OPTION_FIRST_ADAPTIVE:
            if (!select_mode(MODE_FIRST_ADAPTIVE, "--fa")) {
                return -1;
            }
            have_mode = 1;
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

    if (!have_mode) {
        fprintf(stderr,
                "Missing required scheduling option: -fr/-fw/-fa\n");
        print_usage(program_name);
        return -1;
    }

    return 0;
}
