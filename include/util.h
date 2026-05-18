/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define die(fmt, ...)                                                                              \
    do {                                                                                           \
        fprintf(stderr, "ferrite: " fmt "\n", ##__VA_ARGS__);                                      \
        exit(1);                                                                                   \
    } while (0)

// auto abort on fail
static inline void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) {
        die("out of memory");
    }
    return p;
}
static inline void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) {
        die("out of memory");
    }
    return q;
}
static inline char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = xmalloc(n);
    memcpy(p, s, n);
    return p;
}