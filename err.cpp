#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void syserr(const char *fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "SYSTEM ERROR: ");
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    fprintf(stderr, " (%d; %s)\n", errno, strerror(errno));
    exit(EXIT_FAILURE);
}

void fatal(const char *fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

void safe_aux(const char* line_text, int line_number, const char* file_name, const char *emsg)
{
    fprintf(stderr, "Error at line #%d in file %s\n%s\n%s\n", line_number, file_name, line_text, emsg);
    exit(EXIT_FAILURE);
}

