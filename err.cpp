#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>

#include "err.h"

[[noreturn]] void syserr(const char* fmt, ...) {
    va_list fmt_args;
    int org_errno = errno;

    fprintf(stderr, "\tERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, " (%d; %s)\n", org_errno, strerror(org_errno));
    exit(1);
}

[[noreturn]] void fatal(const char* fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "\tERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    fprintf(stderr, "\n");
    exit(1);
}

void message_err(const uint8_t* buffer, size_t buffer_len) {
    size_t bytes_to_write = (buffer_len < 10) ? buffer_len : 10;

    fprintf(stderr, "ERROR MSG ");
    for (size_t i = 0; i < bytes_to_write; i++) {
        fprintf(stderr, "%02x ", buffer[i]);
    }
    fprintf(stderr, "\n");
}
