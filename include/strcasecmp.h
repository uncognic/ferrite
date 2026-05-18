#pragma once
#include <ctype.h>

// custom implementation for portability to windows
static int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        unsigned char ca = (unsigned char) *a;
        unsigned char cb = (unsigned char) *b;

        int la = tolower(ca);
        int lb = tolower(cb);

        if (la != lb) {
            return la - lb;
        }

        a++;
        b++;
    }

    return tolower((unsigned char) *a) - tolower((unsigned char) *b);
}
