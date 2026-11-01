#include "format.h"

#include <ctype.h>
#include <string.h>

const char *format_next(const char *s, FormatSpec *spec)
{
    const char *p = strchr(s, '%');
    if (!p)
        return NULL;
    spec->start = p++;
    while (*p && strchr("-+ 0#", *p))
        p++;
    while (isdigit((unsigned char)*p))
        p++;
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p))
            p++;
    }
    while (*p == 'l' || *p == 'h')
        p++;
    spec->conv = *p;
    if (*p)
        p++;
    spec->len = (int)(p - spec->start);
    return p;
}

Type format_arg_type(char conv)
{
    switch (conv) {
    case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': case 'c':
        return T_INT;
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
        return T_DOUBLE;
    case 's':
        return T_STRING;
    case '%':
        return T_VOID;
    default:
        return T_ERROR;
    }
}
