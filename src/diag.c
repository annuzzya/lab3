#include "diag.h"

#include <stdarg.h>
#include <stdio.h>

static const char *file_name = "<input>";
static int errors;

void diag_set_file(const char *name)
{
    file_name = name;
}

void diag_error(int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: error: ", file_name, line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    errors++;
}

int diag_count(void)
{
    return errors;
}

const char *diag_file(void)
{
    return file_name;
}
