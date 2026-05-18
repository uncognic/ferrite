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
#include "util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    uint32_t addr;
} label_entry;

typedef struct {
    char *name;
    int32_t val;
} equ_entry;

static int label_cmp(const void *a, const void *b) {
    return strcmp(((label_entry *) a)->name, ((label_entry *) b)->name);
}
static int equ_cmp(const void *a, const void *b) {
    return strcmp(((equ_entry *) a)->name, ((equ_entry *) b)->name);
}

typedef struct {
    label_entry *labels;
    size_t nlabels;
    equ_entry *equs;
    size_t nequs;
} symtab;

// find label in symtab
static int sym_find_label(symtab *s, const char *name, uint32_t *out) {
    label_entry key = {(char *) name, 0};
    label_entry *r = bsearch(&key, s->labels, s->nlabels, sizeof(label_entry), label_cmp);
    if (!r) {
        return 0;
    }
    *out = r->addr;
    return 1;
}

// find equ in symtab
static int sym_find_equ(symtab *s, const char *name, int32_t *out) {
    equ_entry key = {(char *) name, 0};
    equ_entry *r = bsearch(&key, s->equs, s->nequs, sizeof(equ_entry), equ_cmp);
    if (!r) {
        return 0;
    }
    *out = r->val;
    return 1;
}

// find value from symtab via name
static int sym_resolve_name(symtab *s, const char *name, int32_t *out) {
    int32_t ev;
    uint32_t lv;
    if (sym_find_equ(s, name, &ev)) {
        *out = ev;
        return 1;
    }
    if (sym_find_label(s, name, &lv)) {
        *out = (int32_t) lv;
        return 1;
    }
    return 0;
}

static uint32_t enc_r(uint32_t op, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    return (op << 26) | (rd << 22) | (rs1 << 18) | (rs2 << 13);
}

static uint32_t enc_i(uint32_t op, uint32_t rd, uint32_t rs1, int32_t imm) {
    uint32_t imm17 = (uint32_t) imm & 0x1FFFFu;
    return (op << 26) | (rd << 22) | (rs1 << 18) | (1u << 17) | imm17;
}

static uint32_t enc_s(uint32_t op, uint32_t rs2, uint32_t rs1, int32_t imm) {
    return enc_i(op, rs2, rs1, imm);
}

static uint32_t enc_b(uint32_t op, uint32_t rs1, uint32_t rs2, int32_t off) {
    uint32_t off18 = (uint32_t) off & 0x3FFFFu;
    return (op << 26) | (rs1 << 22) | (rs2 << 18) | off18;
}

static uint32_t enc_j(uint32_t op, uint32_t rd, int32_t off) {
    uint32_t off22 = (uint32_t) off & 0x3FFFFFu;
    return (op << 26) | (rd << 22) | off22;
}

static uint32_t enc_u(uint32_t op, uint32_t rd, uint32_t imm22) {
    return (op << 26) | (rd << 22) | (imm22 & 0x3FFFFFu);
}

static uint32_t enc_csr_read(uint32_t rd, uint32_t csr_id) {
    return (OP_CSR << 26) | (rd << 22) | (csr_id << 14);
}

static uint32_t enc_csr_write(uint32_t csr_id, uint32_t rs1) {
    return (OP_CSR << 26) | (rs1 << 18) | (csr_id << 14) | (1u << 13);
}

static uint32_t enc_system(uint32_t dir) {
    return (OP_SYSTEM << 26) | (dir << 13);
}

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} bytebuf;

static void bb_init(bytebuf *b) {
    b->data = nullptr;
    b->len = 0;
    b->cap = 0;
}

static void bb_push(bytebuf *b, uint8_t byte) {
    if (b->len >= b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->data = xrealloc(b->data, b->cap);
    }
    b->data[b->len++] = byte;
}

static void bb_push32(bytebuf *b, uint32_t word) {
    bb_push(b, (uint8_t) (word));
    bb_push(b, (uint8_t) (word >> 8));
    bb_push(b, (uint8_t) (word >> 16));
    bb_push(b, (uint8_t) (word >> 24));
}

static void bb_pad(bytebuf *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        bb_push(b, 0);
    }
}

// get register number from opr
static int op_reg(stmt *s, int i, uint32_t *out, asm_error *err) {
    if (i >= s->noperands || s->operands[i].type != OP_REG) {
        asm_err(err, s->line, "%s: expected GPR at operand %d", s->mnemonic, i);
        return 0;
    }
    *out = (uint32_t) s->operands[i].reg;
    return 1;
}

// get floating register number from opr
static int op_freg(stmt *s, int i, uint32_t *out, asm_error *err) {
    if (i >= s->noperands || s->operands[i].type != OP_FREG) {
        asm_err(err, s->line, "%s: expected FPR at operand %d", s->mnemonic, i);
        return 0;
    }
    *out = (uint32_t) s->operands[i].reg;
    return 1;
}

// get immediate value from opr
static int op_imm(stmt *s, int i, symtab *sym, int32_t *out, asm_error *err) {
    if (i >= s->noperands) {
        asm_err(err, s->line, "%s: missing operand %d", s->mnemonic, i);
        return 0;
    }
    operand *op = &s->operands[i];
    if (op->type == OP_IMM) {
        *out = op->imm;
        return 1;
    }
    if (op->type == OP_NAME) {
        if (!sym_resolve_name(sym, op->name, out)) {
            asm_err(err, s->line, "undefined name '%s'", op->name);
            return 0;
        }
        return 1;
    }
    asm_err(err, s->line, "%s: expected immediate at operand %d", s->mnemonic, i);
    return 0;
}

// get pc relative offset of target label from opr
static int op_pcrel(stmt *s, int i, symtab *sym, uint32_t addr, int32_t *out, asm_error *err) {
    int32_t target;
    if (!op_imm(s, i, sym, &target, err)) {
        return 0;
    }
    *out = target - (int32_t) addr;
    return 1;
}

// get csr id from opr
static int op_csr(stmt *s, int i, symtab *sym, uint32_t *out, asm_error *err) {
    if (i >= s->noperands) {
        asm_err(err, s->line, "%s: missing CSR operand", s->mnemonic);
        return 0;
    }
    operand *op = &s->operands[i];
    if (op->type == OP_IMM) {
        *out = (uint32_t) op->imm;
        return 1;
    }
    if (op->type == OP_NAME) {
        static const struct {
            const char *name;
            uint32_t id;
        } csrs[] = {
            {"STATUS", 0}, {"IVT", 1}, {"CAUSE", 2}, {"EPC", 3}, {"ESAVE", 4},
        };
        for (size_t j = 0; j < sizeof(csrs) / sizeof(*csrs); j++) {
            if (strcasecmp(op->name, csrs[j].name) == 0) {
                *out = csrs[j].id;
                return 1;
            }
        }
        asm_err(err, s->line, "unknown CSR '%s'", op->name);
        return 0;
    }
    asm_err(err, s->line, "%s: expected CSR at operand %d", s->mnemonic, i);
    return 0;
}
