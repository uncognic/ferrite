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
#include "isa.h"
#include "strcasecmp.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int is_ident_start(char c) {
    return isalpha((unsigned char) c) || c == '_';
}

static int is_ident_cont(char c) {
    return isalnum((unsigned char) c) || c == '_' || c == '.';
}

static char *resolve_register(const char *name, tok_type *out_type, int64_t *out_val) {
    // numbered r
    if ((name[0] == 'R' || name[0] == 'r') && isdigit((unsigned char) name[1])) {
        int n = atoi(name + 1);
        if (n >= 0 && n < 16 && name[2 + (n >= 10)] == '\0') {
            *out_type = TOK_REG;
            *out_val = n;
            return (char *) name;
        }
    }
    // numbered f
    if ((name[0] == 'F' || name[0] == 'f') && isdigit((unsigned char) name[1])) {
        int n = atoi(name + 1);
        if (n >= 0 && n < 16) {
            *out_type = TOK_FREG;
            *out_val = n;
            return (char *) name;
        }
    }

    // non floating aliases
    static const struct {
        const char *name;
        int reg;
    } gpr[] = {
        {"zero", 0}, {"ra", 1},  {"sp", 2},  {"fp", 3},  {"rv", 4},  {"a0", 5},
        {"a1", 6},   {"a2", 7},  {"a3", 8},  {"a4", 9},  {"t0", 10}, {"t1", 11},
        {"t2", 12},  {"t3", 13}, {"s0", 14}, {"s1", 15},
    };
    for (size_t i = 0; i < sizeof(gpr) / sizeof(*gpr); i++) {
        if (strcasecmp(name, gpr[i].name) == 0) {
            *out_type = TOK_REG;
            *out_val = gpr[i].reg;
            return (char *) name;
        }
    }

    // floating aliases
    static const struct {
        const char *name;
        int reg;
    } fpr[] = {
        {"fa0", 0}, {"fa1", 1}, {"fa2", 2}, {"fa3", 3}, {"ft0", 4},  {"ft1", 5},
        {"ft2", 6}, {"ft3", 7}, {"fs0", 8}, {"fs1", 9}, {"fs2", 10}, {"fs3", 11},
    };
    for (size_t i = 0; i < sizeof(fpr) / sizeof(*fpr); i++) {
        if (strcasecmp(name, fpr[i].name) == 0) {
            *out_type = TOK_FREG;
            *out_val = fpr[i].reg;
            return (char *) name;
        }
    }

    return nullptr;
}

int lex(const char *src, token_vec *out, arena *a, asm_error *err) {
    const char *p = src; // current position
    int line = 1;        // current line number

    while (*p) {
        // whitespace
        while (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
        }
        if (!*p) {
            break;
        }

        // comment
        if (*p == ';') {
            // skip to eol
            while (*p && *p != '\n') {
                p++;
            }
        }

        // newline
        if (*p == '\n') {
            token t = {TOK_NEWLINE, line, {0}};
            vec_push(out, t);
            p++;
            continue;
        }

        if (*p == '\r') {
            p++;
            continue;
        }

        if (*p == ',') {
            token t = {TOK_COMMA, line, {0}};
            vec_push(out, t);
            p++;
            continue;
        }

        if (*p == '"') {
            p++; // skip opening quote
            strbuf sb;
            strbuf_init(&sb);
            while (*p && *p != '"') {
                if (*p == '\\') {
                    p++;
                    switch (*p) {
                        case 'n':
                            strbuf_appendc(&sb, '\n');
                            break;
                        case 't':
                            strbuf_appendc(&sb, '\t');
                            break;
                        case 'r':
                            strbuf_appendc(&sb, '\r');
                            break;
                        case '0':
                            strbuf_appendc(&sb, '\0');
                            break;
                        case '\\':
                            strbuf_appendc(&sb, '\\');
                            break;
                        case '"':
                            strbuf_appendc(&sb, '"');
                            break;
                        default:
                            asm_err(err, line, "unknown escape '\\%c'", *p);
                            strbuf_free(&sb);
                            return 0;
                    }
                } else {
                    strbuf_appendc(&sb, *p);
                }
                p++;
            }
            if (*p != '"') {
                asm_err(err, line, "unterminated string literal");
                strbuf_free(&sb);
                return 0;
            }
            p++;

            size_t len = sb.len;
            char *copy = arena_alloc(a, len + 1);
            memcpy(copy, strbuf_get(&sb), len + 1);
            strbuf_free(&sb);

            token t = {TOK_STR, line, {0}};
            t.str = copy;
            vec_push(out, t);
            continue;
        }

        // directive
        if (*p == '.') {
            p++;
            const char *start = p;
            while (is_ident_cont(*p)) {
                p++;
            }
            size_t len = (size_t) (p - start);
            if (len == 0) {
                asm_err(err, line, "expected directive name after '.'");
                return 0;
            }
            char *name = arena_alloc(a, len + 1);
            memcpy(name, start, len);
            name[len] = '\0';
            for (size_t i = 0; i < len; i++) {
                name[i] = (char) tolower((unsigned char) name[i]);
            }
            token t = {TOK_DIRECTIVE, line, {0}};
            t.str = name;
            vec_push(out, t);
            continue;
        }

        // number
        if (isdigit((unsigned char) *p) || (*p == '-' && isdigit((unsigned char) p[1]))) {
            int negative = *p == '-';
            if (negative) {
                p++;
            }

            // hex
            if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
                p += 2;
                const char *start = p;
                while (isxdigit((unsigned char) *p)) {
                    p++;
                }
                char buf[32] = {0};
                size_t len = (size_t) (p - start);
                if (len >= sizeof(buf)) {
                    asm_err(err, line, "hex literal too long");
                    return 0;
                }
                memcpy(buf, start, len);
                int64_t val = (int64_t) strtoll(buf, nullptr, 16);
                token t = {TOK_INT, line, {0}};
                t.ival = negative ? -val : val;
                vec_push(out, t);
                continue;
            }

            // binary
            if (*p == '0' && (p[1] == 'b' || p[1] == 'B')) {
                p += 2;
                const char *start = p;
                while (*p == '0' || *p == '1') {
                    p++;
                }
                char buf[64] = {0};
                size_t len = (size_t) (p - start);
                if (len >= sizeof(buf)) {
                    asm_err(err, line, "binary literal too long");
                    return 0;
                }
                memcpy(buf, start, len);
                int64_t n = (int64_t) strtoll(buf, NULL, 2);
                token t = {TOK_INT, line, {0}};
                t.ival = negative ? -n : n;
                vec_push(out, t);
                continue;
            }

            // decimal
            const char *start = p;
            while (isdigit((unsigned char) *p)) {
                p++;
            }

            // float
            if (*p == '.' && isdigit((unsigned char) p[1])) {
                p++;
                while (isdigit((unsigned char) *p)) {
                    p++;
                }
                if (*p == 'e' || *p == 'E') {
                    p++;
                    if (*p == '+' || *p == '-') {
                        p++;
                    }
                    while (isdigit((unsigned char) *p)) {
                        p++;
                    }
                }
                size_t len = (size_t) (p - start);
                char buf[64] = {0};
                if (len >= sizeof(buf)) {
                    asm_err(err, line, "float literal too long");
                    return 0;
                }
                memcpy(buf, start, len);
                double f = strtod(buf, NULL);
                token t = {TOK_FLOAT, line, {0}};
                t.fval = negative ? -f : f;
                vec_push(out, t);
                continue;
            }

            // integer
            size_t len = (size_t) (p - start);
            char buf[32] = {0};
            if (len >= sizeof(buf)) {
                asm_err(err, line, "integer literal too long");
                return 0;
            }
            memcpy(buf, start, len);
            int64_t n = strtoll(buf, NULL, 10);
            token t = {TOK_INT, line, {0}};
            t.ival = negative ? -n : n;
            vec_push(out, t);
            continue;
        }

        // ident or register
        if (is_ident_start(*p)) {
            const char *start = p;
            while (is_ident_cont(*p)) {
                p++;
            }
            size_t len = (size_t) (p - start);
            char *name = arena_alloc(a, len + 1);
            memcpy(name, start, len);
            name[len] = '\0';

            const char *q = p;
            while (*q == ' ' || *q == '\t') {
                q++;
            }

            // label
            if (*q == ':') {
                token ti = {TOK_IDENT, line, {0}};
                ti.str = name;
                token tc = {TOK_COLON, line, {0}};
                vec_push(out, ti);
                vec_push(out, tc);
                p = q + 1;
                continue;
            }

            // check for directive
            tok_type rtype;
            int64_t rval;
            if (resolve_register(name, &rtype, &rval)) {
                token t = {rtype, line, {0}};
                t.ival = rval;
                vec_push(out, t);
            } else {
                token t = {TOK_IDENT, line, {0}};
                t.str = name;
                vec_push(out, t);
            }
            continue;
        }

        asm_err(err, line, "unexpected character '%c'", *p);
        return 0;
    }

    token eof = {TOK_EOF, line, {0}};
    vec_push(out, eof);
    return 1;
}