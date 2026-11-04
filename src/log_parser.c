#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#define CHECKSUM_MAGIC_NUMBER 0x00000000DEADBEEFULL
#define REC_SIZE 32

/*
 * Binary log record format: 32 bytes = 4 * uint64_t
 *
 * [0] timestamp: ns within one day
 * [1] business payload: value
 * [2] business payload:
 *     bits 63..32: checksum low 32 bits
 *     bits 31..16: thread id
 *     bits 15..0 : sleep_time in ms
 * [3] reserved
 */

static void format_ns_in_day(uint64_t ns, char *out, size_t out_size) {
    const uint64_t DAY_NS = 86400ULL * 1000000000ULL;
    const uint64_t HOUR_NS = 3600ULL * 1000000000ULL;
    const uint64_t MIN_NS = 60ULL * 1000000000ULL;
    const uint64_t SEC_NS = 1000000000ULL;

    ns %= DAY_NS;

    unsigned h = (unsigned)(ns / HOUR_NS);
    ns %= HOUR_NS;

    unsigned m = (unsigned)(ns / MIN_NS);
    ns %= MIN_NS;

    unsigned s = (unsigned)(ns / SEC_NS);
    unsigned nsec = (unsigned)(ns % SEC_NS);

    snprintf(out, out_size, "%02u:%02u:%02u.%09u", h, m, s, nsec);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <logfile>\n", argv[0]);
        fprintf(stderr, "Example: %s a.log > parsed.csv\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open %s: %s\n", filename, strerror(errno));
        return EXIT_FAILURE;
    }

    /*
     * CSV header.
     * If you do not want header, delete or comment out this printf.
     */
    printf(
        "seq,"
        "time,"
        "value_dec,"
        "checksum_hex,"
        "checksum_ok,"
        "thread_id,"
        "sleep_ms,"
        "reserved_hex\n"
    );

    unsigned char buf[REC_SIZE];
    uint64_t seq = 0;
    size_t n;

    while ((n = fread(buf, 1, REC_SIZE, fp)) == REC_SIZE) {
        uint64_t rec[4];

        /*
         * Use memcpy to avoid potential alignment issues.
         *
         * This assumes the parser runs on the same endianness as the writer.
         * The writer writes raw uint64_t memory directly.
         */
        memcpy(rec, buf, sizeof(rec));

        uint64_t timestamp_ns = rec[0];
        uint64_t value = rec[1];
        uint64_t third = rec[2];
        uint64_t reserved = rec[3];

        uint32_t checksum = (uint32_t)(third >> 32);
        uint32_t thread_id = (uint32_t)((third >> 16) & 0xFFFFULL);
        uint32_t sleep_ms = (uint32_t)(third & 0xFFFFULL);

        uint32_t expected_checksum =
            (uint32_t)(value ^ CHECKSUM_MAGIC_NUMBER);

        int checksum_ok = (expected_checksum == checksum);

        char time_str[64];
        format_ns_in_day(timestamp_ns, time_str, sizeof(time_str));

        printf(
            "%" PRIu64 ","
            "%s,"
            "%" PRIu64 ","
            "0x%08" PRIx32 ","
            "%d,"
            "%" PRIu32 ","
            "%" PRIu32 ","
            "0x%016" PRIx64 "\n",
            seq,
            time_str,
            value,
            checksum,
            checksum_ok,
            thread_id,
            sleep_ms,
            reserved
        );

        seq++;
    }

    if (n != 0) {
        fprintf(
            stderr,
            "Warning: trailing %zu bytes are not a complete 32-byte record, ignored.\n",
            n
        );
    }

    if (ferror(fp)) {
        fprintf(stderr, "Read error on %s\n", filename);
        fclose(fp);
        return EXIT_FAILURE;
    }

    fclose(fp);
    return EXIT_SUCCESS;
}