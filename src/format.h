#ifndef FORMAT_H
#define FORMAT_H

#include "ast.h"

typedef struct {
    const char *start;
    int len;
    char conv;
} FormatSpec;

const char *format_next(const char *s, FormatSpec *spec);
Type format_arg_type(char conv);

#endif
