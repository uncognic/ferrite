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
#include "arena.h"
#include "strbuf.h"
#include "vec.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int line;
    char msg[256];
} asm_error;

#define asm_err(e, l, fmt, ...)                                                                    \
    do {                                                                                           \
        (e)->line = (l);                                                                           \
        snprintf((e)->msg, sizeof((e)->msg), fmt, ##__VA_ARGS__);                                  \
    } while (0)

typedef enum {
    TOK_IDENT,
    TOK_REG,  // R0-R15, resolved by lexer
    TOK_FREG, // F0-F15, resolved by lexer
    TOK_INT,
    TOK_FLOAT,
    TOK_STR,
    TOK_COMMA,
    TOK_COLON,
    TOK_DIRECTIVE,
    TOK_NEWLINE,
    TOK_EOF,
} tok_type;

typedef struct {
    tok_type type;
    int line;
    union {
        char *str;
        int64_t ival;
        double fval;
    };
} token;

typedef enum {
    OP_REG,
    OP_FREG,
    OP_IMM,
    OP_FLOAT,
    OP_NAME,
    OP_STR,
} operand_type;

typedef struct {
    operand_type type;
    union {
        int32_t reg;
        int32_t imm;
        float fval;
        char *name;
    };
} operand;

typedef enum {
    STMT_LABEL,
    STMT_EQU,
    STMT_INSTR,
    STMT_DIR,
} stmt_type;

typedef struct {
    stmt_type type;
    int line;
    union {
        char *label;
        struct {
            char *equ_name;
            int32_t equ_val;
        };
        struct {
            char *mnemonic;
            operand *operands;
            int noperands;
        };
        struct {
            char *dir_name;
            operand *dir_args;
            int dir_nargs;
        };
    };
} stmt;

typedef vec(token) token_vec;
typedef vec(stmt) stmt_vec;

[[nodiscard]] int lex(const char *src, token_vec *out, arena *a, asm_error *err);
[[nodiscard]] int parse(token_vec *tokens, stmt_vec *out, arena *a, asm_error *err);
[[nodiscard]] int encode(stmt_vec *stmts, uint8_t **out, size_t *out_len, asm_error *err);