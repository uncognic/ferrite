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

// fast alloc of many small objects
// for AST

#define ARENA_BLOCK_SIZE (64 * 1024) // 64 kb blocks

typedef struct arena_block {
    struct arena_block *next;
    size_t used;
    size_t cap;
    char data[]; // flexible array member
} arena_block;

typedef struct {
    arena_block *head;
} arena;

static inline void arena_init(arena *a) {
    a->head = nullptr;
}

static inline void *arena_alloc(arena *a, size_t size) {
    // align to 8 bytes
    size = (size + 7) & ~(size_t) 7;

    // if current block doesn't have enough space, allocate a new one
    if (!a->head || a->head->used + size > a->head->cap) {
        size_t cap = size > ARENA_BLOCK_SIZE ? size : ARENA_BLOCK_SIZE;
        arena_block *blk = xmalloc(sizeof(arena_block) + cap);
        blk->next = a->head;
        blk->used = 0;
        blk->cap = cap;
        a->head = blk;
    }

    void *p = a->head->data + a->head->used;
    a->head->used += size;
    return p;
}

static inline void *arena_calloc(arena *a, size_t size) {
    void *p = arena_alloc(a, size);
    memset(p, 0, size);
    return p;
}

static inline void arena_free(arena *a) {
    arena_block *blk = a->head;
    while (blk) {
        arena_block *next = blk->next;
        free(blk);
        blk = next;
    }
    a->head = nullptr;
}

// allocate and zero a struct from an arena
#define arena_new(a, T) ((T *) arena_calloc((a), sizeof(T)))