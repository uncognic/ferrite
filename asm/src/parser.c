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

#include "asm.h"
#include <ctype.h>
#include <string.h>
#include <strings.h>

typedef struct {
    token_vec *tokens;
    size_t pos;
    arena *a;
} parser;

static token *peek(parser *p) {
    return &p->tokens->data[p->pos];
}

static token *peek_ahead(parser *p, size_t offset) {
    size_t idx = p->pos + offset;
    if (idx >= (size_t) vec_len(p->tokens)) {
        idx = (size_t) vec_len(p->tokens) - 1;
    }
    return &p->tokens->data[idx];
}

static token *advance(parser *p) {
    token *t = &p->tokens->data[p->pos];
    if (p->pos + 1 < (size_t) vec_len(p->tokens)) {
        p->pos++;
    }
    return t;
}

static int is_operand_start(parser *p) {
    tok_type t = peek(p)->type;
    return t == TOK_REG || t == TOK_FREG || t == TOK_INT || t == TOK_FLOAT || t == TOK_IDENT ||
           t == TOK_STR;
}

static int parse_operand(parser *p, operand *out, asm_error *err) {
    token *t = peek(p);
    switch (t->type) {
        case TOK_REG:
            advance(p);
            out->type = OP_REG;
            out->reg = (int32_t) t->ival;
            return 1;
        case TOK_FREG:
            advance(p);
            out->type = OP_FREG;
            out->reg = (int32_t) t->ival;
            return 1;
        case TOK_INT: {
            advance(p);
            int64_t n = t->ival;
            if (n < (int64_t) INT32_MIN && n > (int64_t) UINT32_MAX) {
                asm_err(err, t->line, "immediate %lld out of 32-bit range", (long long) n);
                return 0;
            }
            out->type = OP_IMM;
            out->imm = (int32_t) n;
            return 1;
        }
        case TOK_FLOAT:
            advance(p);
            out->type = OP_FLOAT;
            out->fval = (float) t->fval;
            return 1;
        case TOK_IDENT:
            advance(p);
            out->type = OP_NAME;
            out->name = t->str;
            return 1;
        case TOK_STR:
            advance(p);
            out->type = OP_STR;
            out->name = t->str;
            return 1;
        default:
            asm_err(err, t->line, "expected operand, got token type %d", t->type);
            return 0;
    }
}

static int parse_operand_list(parser *p, operand **out, int *nout, arena *a, asm_error *err) {
    // collect ops into a vec
    vec(operand) ops = {0};

    // parse operands until we hit a non-operand token
    while (is_operand_start(p)) {
        operand op = {0};
        if (!parse_operand(p, &op, err)) {
            vec_free(&ops);
            return 0;
        }
        vec_push(&ops, op);
        if (peek(p)->type == TOK_COMMA) {
            advance(p);
        } else {
            break;
        }
    }

    // copy ops to output array
    *nout = (int) vec_len(&ops);
    if (*nout > 0) {
        *out = arena_alloc(a, sizeof(operand) * (size_t) *nout);
        memcpy(*out, ops.data, sizeof(operand) * (size_t) *nout);
    } else {
        *out = NULL;
    }
    vec_free(&ops);
    return 1;
}

static int expect_eol(parser *p, asm_error *err) {
    tok_type t = peek(p)->type;
    if (t == TOK_NEWLINE || t == TOK_EOF) {
        advance(p);
        return 1;
    }
    asm_err(err, peek(p)->line, "expected end of line, got token type %d", t);
    return 0;
}
int parse(token_vec *tokens, stmt_vec *out, arena *a, asm_error *err) {
    parser p = {tokens, 0, a};

    // parse
    for (;;) {
        // skip blank lines
        while (peek(&p)->type == TOK_NEWLINE) {
            advance(&p);
        }
        if (peek(&p)->type == TOK_EOF) {
            break;
        }

        // label or dir
        while (peek(&p)->type == TOK_IDENT) {
            if (peek_ahead(&p, 1)->type == TOK_COLON) {
                // label
                token *name_tok = advance(&p); // name
                advance(&p);                   // colon
                stmt s = {STMT_LABEL, name_tok->line, {0}};
                s.label = name_tok->str;
                vec_push(out, s);
                while (peek(&p)->type == TOK_NEWLINE) {
                    advance(&p);
                }
                if (peek(&p)->type == TOK_EOF) {
                    return 1;
                }
            } else if (peek_ahead(&p, 1)->type == TOK_DIRECTIVE) {
                // directive
                token *name_tok = advance(&p); // name
                // check if .equ
                if (strcasecmp(peek(&p)->str, "equ") == 0) {
                    advance(&p); // consume .equ
                    int line = peek(&p)->line;
                    if (peek(&p)->type != TOK_INT) {
                        asm_err(err, line, ".equ requires a numeric value");
                        return 0;
                    }
                    int32_t val = (int32_t) peek(&p)->ival;
                    advance(&p);
                    stmt s = {STMT_EQU, name_tok->line, {0}};
                    s.equ_name = name_tok->str;
                    s.equ_val = val;
                    vec_push(out, s);
                    if (!expect_eol(&p, err)) {
                        return 0;
                    }
                } else {
                    stmt s = {STMT_LABEL, name_tok->line, {0}};
                    s.label = name_tok->str;
                    vec_push(out, s);
                }
                break;
            } else {
                break;
            }
        }

        // skip blanks again
        while (peek(&p)->type == TOK_NEWLINE) {
            advance(&p);
        }
        if (peek(&p)->type == TOK_EOF) {
            break;
        }

        token *cur = peek(&p);

        // directive
        if (cur->type == TOK_DIRECTIVE) {
            int line = cur->line;
            char *name = cur->str;
            advance(&p);

            if (strcasecmp(name, "entry") == 0) {
                operand *args = NULL;
                int nargs = 0;
                if (!parse_operand_list(&p, &args, &nargs, a, err)) {
                    return 0;
                }
                stmt s = {STMT_DIR, line, {0}};
                s.dir_name = name;
                s.dir_args = args;
                s.dir_nargs = nargs;
                vec_push(out, s);
                if (!expect_eol(&p, err)) {
                    return 0;
                }
            } else {
                operand *args = NULL;
                int nargs = 0;
                if (!parse_operand_list(&p, &args, &nargs, a, err)) {
                    return 0;
                }
                stmt s = {STMT_DIR, line, {0}};
                s.dir_name = name;
                s.dir_args = args;
                s.dir_nargs = nargs;
                vec_push(out, s);
                if (!expect_eol(&p, err)) {
                    return 0;
                }
            }
            // instruction
        } else if (cur->type == TOK_IDENT) {
            int line = cur->line;
            char *mnemonic = cur->str;
            // upprecase it
            for (char *c = mnemonic; *c; c++) {
                *c = (char) toupper((unsigned char) *c);
            }
            advance(&p);

            operand *ops = NULL;
            int nops = 0;
            if (!parse_operand_list(&p, &ops, &nops, a, err)) {
                return 0;
            }

            stmt s = {STMT_INSTR, line, {0}};
            s.mnemonic = mnemonic;
            s.operands = ops;
            s.noperands = nops;
            vec_push(out, s);
            if (!expect_eol(&p, err)) {
                return 0;
            }
        } else if (cur->type != TOK_NEWLINE && cur->type != TOK_EOF) {
            asm_err(err, cur->line, "unexpected token type %d", cur->type);
            return 0;
        }
    }

    return 1;
}
