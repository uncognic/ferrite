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
#include "util.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} strbuf;

static inline void strbuf_init(strbuf *sb) {
    sb->data = nullptr;
    sb->len = 0;
    sb->cap = 0;
}

static inline void strbuf_free(strbuf *sb) {
    free(sb->data);
    strbuf_init(sb);
}

static inline void strbuf_grow(strbuf *sb, size_t extra) {
    size_t needed = sb->len + extra + 1; // null termiantor
    if (needed <= sb->cap) {
        return;
    }
    size_t cap = sb->cap ? sb->cap * 2 : 64;
    while (cap < needed) {
        cap *= 2;
    }
    sb->data = xrealloc(sb->data, cap);
    sb->cap = cap;
}

static inline void strbuf_appendc(strbuf *sb, char c) {
    strbuf_grow(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static inline void strbuf_append(strbuf *sb, const char *s) {
    size_t n = strlen(s);
    strbuf_grow(sb, n);
    memcpy(sb->data + sb->len, s, n + 1);
    sb->len += n;
}

static inline void strbuf_appendn(strbuf *sb, const char *s, size_t n) {
    strbuf_grow(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static inline void strbuf_clear(strbuf *sb) {
    sb->len = 0;
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

static inline const char *strbuf_get(const strbuf *sb) {
    return sb->data ? sb->data : "";
}