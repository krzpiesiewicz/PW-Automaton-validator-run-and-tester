#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <zconf.h>
#include "err.h"

extern int sys_nerr;

void syserr(enum exiting mode, const char *fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR (pid %d): ", getpid());

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);
    fprintf(stderr, " (%d; %s)\n", errno, strerror(errno));
    if (mode == EXIT) {
        exit(1);
    }
}

void fatal(enum exiting mode, const char *fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR (pid %d): ", getpid());

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);

    fprintf(stderr, "\n");
    if (mode == EXIT) {
        exit(1);
    }
}