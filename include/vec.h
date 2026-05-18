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

// growable array via macros
//   vec(Token) tokens = {0};
//   vec_push(&tokens, tok);
//   Token t = vec_get(&tokens, i);
//   vec_free(&tokens);

#define vec(T)                                                                                     \
    struct {                                                                                       \
        T *data;                                                                                   \
        size_t len;                                                                                \
        size_t cap;                                                                                \
    }

#define vec_push(v, item)                                                                          \
    do {                                                                                           \
        if ((v)->len >= (v)->cap) {                                                                \
            (v)->cap = (v)->cap ? (v)->cap * 2 : 8;                                                \
            (v)->data = xrealloc((v)->data, (v)->cap * sizeof(*(v)->data));                        \
        }                                                                                          \
        (v)->data[(v)->len++] = (item);                                                            \
    } while (0)

#define vec_get(v, i) ((v)->data[i])

#define vec_len(v) ((v)->len)

#define vec_free(v)                                                                                \
    do {                                                                                           \
        free((v)->data);                                                                           \
        (v)->data = nullptr;                                                                       \
        (v)->len = 0;                                                                              \
        (v)->cap = 0;                                                                              \
    } while (0)

#define vec_clear(v)                                                                               \
    do {                                                                                           \
        (v)->len = 0;                                                                              \
    } while (0)

#define vec_last(v) ((v)->data[(v)->len - 1])

#define vec_pop(v) ((v)->data[--(v)->len])