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
        int32_t eval;
        if (sym_find_equ(sym, op->name, &eval)) {
            *out = (uint32_t) eval;
            return 1;
        }
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
// get either reg or imm operand
static int op_ri(stmt *s, int i, symtab *sym, uint32_t op_code, uint32_t rd, uint32_t rs1,
                 uint32_t *out, asm_error *err) {
    if (i >= s->noperands) {
        asm_err(err, s->line, "%s: missing operand %d", s->mnemonic, i);
        return 0;
    }
    operand *op = &s->operands[i];
    if (op->type == OP_REG) {
        *out = enc_r(op_code, rd, rs1, (uint32_t) op->reg);
        return 1;
    }
    int32_t imm;
    if (!op_imm(s, i, sym, &imm, err)) {
        return 0;
    }
    *out = enc_i(op_code, rd, rs1, imm);
    return 1;
}

static void encode_li(uint32_t rd, int32_t imm, uint32_t *w1, uint32_t *w2) {
    uint32_t val = (uint32_t) imm;
    uint32_t lower = val & 0x3FF;
    uint32_t upper = val >> 10;
    if (lower & 0x200) {
        upper = (upper + 1) & 0x3FFFFF;
    }
    int32_t lower_signed = sign_extend(lower, 10);
    *w1 = enc_u(OP_LUI, rd, upper);
    *w2 = enc_i(OP_ADD, rd, rd, lower_signed);
}

static uint32_t instr_size(stmt *s) {
    if (strcmp(s->mnemonic, "LI") == 0) {
        return 8;
    }
    return 4;
}

static int dir_size(stmt *s, uint32_t *out, asm_error *err) {
    const char *name = s->dir_name;
    if (strcasecmp(name, "word") == 0) {
        *out = 4 * (uint32_t) s->dir_nargs;
        return 1;
    }
    if (strcasecmp(name, "half") == 0) {
        *out = 2 * (uint32_t) s->dir_nargs;
        return 1;
    }
    if (strcasecmp(name, "byte") == 0) {
        *out = (uint32_t) s->dir_nargs;
        return 1;
    }
    if (strcasecmp(name, "string") == 0) {
        if (s->dir_nargs < 1 || s->dir_args[0].type != OP_STR) {
            asm_err(err, s->line, ".string requires a string literal");
            return 0;
        }
        uint32_t len = (uint32_t) strlen(s->dir_args[0].name) + 1;
        *out = (len + 3) & ~3u; // pad to word
        return 1;
    }
    if (strcasecmp(name, "entry") == 0 || strcasecmp(name, "equ") == 0 ||
        strcasecmp(name, "org") == 0 || strcasecmp(name, "align") == 0) {
        *out = 0;
        return 1;
    }
    asm_err(err, s->line, "unknown directive '.%s'", name);
    return 0;
}

static int build_symtab(stmt_vec *stmts, const char *entry_name, symtab *sym, asm_error *err) {
    // count labels and equs first
    size_t nlabels = 0, nequs = 0;
    for (size_t i = 0; i < vec_len(stmts); i++) {
        stmt *s = &vec_get(stmts, i);
        if (s->type == STMT_LABEL) {
            nlabels++;
        }
        if (s->type == STMT_EQU) {
            nequs++;
        }
    }

    sym->labels = nlabels ? xmalloc(sizeof(label_entry) * nlabels) : NULL;
    sym->equs = nequs ? xmalloc(sizeof(equ_entry) * nequs) : NULL;
    sym->nlabels = 0;
    sym->nequs = 0;

    uint32_t addr = entry_name ? 4u : 0u; // reserve first word for entry

    for (size_t i = 0; i < vec_len(stmts); i++) {
        stmt *s = &vec_get(stmts, i);
        switch (s->type) {
            case STMT_LABEL: {
                // check for duplicates
                for (size_t j = 0; j < sym->nlabels; j++) {
                    if (strcmp(sym->labels[j].name, s->label) == 0) {
                        asm_err(err, s->line, "duplicate label '%s'", s->label);
                        return 0;
                    }
                }
                sym->labels[sym->nlabels++] = (label_entry){s->label, addr};
                break;
            }
            case STMT_EQU:
                sym->equs[sym->nequs++] = (equ_entry){s->equ_name, s->equ_val};
                break;
            case STMT_INSTR:
                addr += instr_size(s);
                break;
            case STMT_DIR: {
                const char *dn = s->dir_name;
                if (strcasecmp(dn, "org") == 0) {
                    if (s->dir_nargs < 1 || s->dir_args[0].type != OP_IMM) {
                        asm_err(err, s->line, ".org requires an address");
                        return 0;
                    }
                    uint32_t target = (uint32_t) s->dir_args[0].imm;
                    if (target < addr) {
                        asm_err(err, s->line, ".org %08x is behind current address %08x", target,
                                addr);
                        return 0;
                    }
                    addr = target;
                } else if (strcasecmp(dn, "align") == 0) {
                    if (s->dir_nargs < 1 || s->dir_args[0].type != OP_IMM) {
                        asm_err(err, s->line, ".align requires a positive integer");
                        return 0;
                    }
                    uint32_t n = (uint32_t) s->dir_args[0].imm;
                    uint32_t r = addr % n;
                    if (r) {
                        addr += n - r;
                    }
                } else if (strcasecmp(dn, "entry") == 0 || strcasecmp(dn, "equ") == 0) {
                    // nop
                } else {
                    uint32_t sz = 0;
                    if (!dir_size(s, &sz, err)) {
                        return 0;
                    }
                    addr += sz;
                }
                break;
            }
        }
    }

    qsort(sym->labels, sym->nlabels, sizeof(label_entry), label_cmp);
    qsort(sym->equs, sym->nequs, sizeof(equ_entry), equ_cmp);
    return 1;
}

static int emit_instr(stmt *s, uint32_t addr, symtab *sym, bytebuf *bb, asm_error *err) {
    const char *m = s->mnemonic;
    uint32_t rd = 0, rs1 = 0, rs2 = 0;
    int32_t imm = 0;
    uint32_t word = 0;

#define REQ_RD                                                                                     \
    if (!op_reg(s, 0, &rd, err))                                                                   \
    return 0
#define REQ_RS1                                                                                    \
    if (!op_reg(s, 1, &rs1, err))                                                                  \
    return 0
#define REQ_RS2                                                                                    \
    if (!op_reg(s, 2, &rs2, err))                                                                  \
    return 0
#define REQ_FD                                                                                     \
    if (!op_freg(s, 0, &rd, err))                                                                  \
    return 0
#define REQ_FS1                                                                                    \
    if (!op_freg(s, 1, &rs1, err))                                                                 \
    return 0
#define REQ_FS2                                                                                    \
    if (!op_freg(s, 2, &rs2, err))                                                                 \
    return 0

    // pseudo instructions
    if (strcmp(m, "NOP") == 0) {
        word = enc_r(OP_ADD, 0, 0, 0);
    } else if (strcmp(m, "CLR") == 0) {
        REQ_RD;
        word = enc_r(OP_ADD, rd, 0, 0);
    } else if (strcmp(m, "MOV") == 0) {
        REQ_RD;
        if (s->noperands > 1 && s->operands[1].type == OP_REG) {
            rs1 = (uint32_t) s->operands[1].reg;
            word = enc_r(OP_ADD, rd, 0, rs1);
        } else {
            if (!op_imm(s, 1, sym, &imm, err)) {
                return 0;
            }
            word = enc_i(OP_ADD, rd, 0, imm);
        }
    } else if (strcmp(m, "LI") == 0) {
        REQ_RD;
        if (!op_imm(s, 1, sym, &imm, err)) {
            return 0;
        }
        uint32_t w1, w2;
        encode_li(rd, imm, &w1, &w2);
        bb_push32(bb, w1);
        bb_push32(bb, w2);
        return 1;
    } else if (strcmp(m, "NEG") == 0) {
        REQ_RD;
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        word = enc_r(OP_SUB, rd, 0, rs1);
    } else if (strcmp(m, "JZ") == 0) {
        REQ_RD;
        if (!op_pcrel(s, 1, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JEQ, rd, 0, imm);
    } else if (strcmp(m, "JNZ") == 0) {
        REQ_RD;
        if (!op_pcrel(s, 1, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JNE, rd, 0, imm);
    } else if (strcmp(m, "CALL") == 0) {
        if (!op_pcrel(s, 0, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_j(OP_J, 1, imm);
    } else if (strcmp(m, "RET") == 0) {
        word = enc_i(OP_JR, 0, 1, 0);
    } else if (strcmp(m, "J") == 0 && s->noperands == 1) {
        if (!op_pcrel(s, 0, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_j(OP_J, 0, imm);

        // arithmetic
    } else if (strcmp(m, "ADD") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_ADD, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "SUB") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_SUB, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "MUL") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_MUL, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "DIV") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_DIV, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "MOD") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_MOD, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "AND") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_AND, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "OR") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_OR, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "XOR") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_XOR, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "SHL") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_SHL, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "SHR") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_SHR, rd, rs1, &word, err)) {
            return 0;
        }
    } else if (strcmp(m, "SAR") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_ri(s, 2, sym, OP_SAR, rd, rs1, &word, err)) {
            return 0;
        }

        // single registr
    } else if (strcmp(m, "NOT") == 0) {
        REQ_RD;
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        word = enc_r(OP_NOT, rd, rs1, 0);
    } else if (strcmp(m, "INC") == 0) {
        REQ_RD;
        word = enc_r(OP_INC, rd, rd, 0);
    } else if (strcmp(m, "DEC") == 0) {
        REQ_RD;
        word = enc_r(OP_DEC, rd, rd, 0);
    } else if (strcmp(m, "NEG2") == 0) {
        REQ_RD;
        word = enc_r(OP_NEG, rd, 0, 0);

        // load store
    } else if (strcmp(m, "LW") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_i(OP_LW, rd, rs1, imm);
    } else if (strcmp(m, "LH") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_i(OP_LH, rd, rs1, imm);
    } else if (strcmp(m, "LB") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_i(OP_LB, rd, rs1, imm);
    } else if (strcmp(m, "SW") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_s(OP_SW, rd, rs1, imm);
    } else if (strcmp(m, "SH") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_s(OP_SH, rd, rs1, imm);
    } else if (strcmp(m, "SB") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_s(OP_SB, rd, rs1, imm);
    } else if (strcmp(m, "LUI") == 0) {
        REQ_RD;
        if (!op_imm(s, 1, sym, &imm, err)) {
            return 0;
        }
        word = enc_u(OP_LUI, rd, (uint32_t) imm);

        // fp
    } else if (strcmp(m, "FADD") == 0) {
        REQ_FD;
        REQ_FS1;
        REQ_FS2;
        word = enc_r(OP_FADD, rd, rs1, rs2);
    } else if (strcmp(m, "FSUB") == 0) {
        REQ_FD;
        REQ_FS1;
        REQ_FS2;
        word = enc_r(OP_FSUB, rd, rs1, rs2);
    } else if (strcmp(m, "FMUL") == 0) {
        REQ_FD;
        REQ_FS1;
        REQ_FS2;
        word = enc_r(OP_FMUL, rd, rs1, rs2);
    } else if (strcmp(m, "FDIV") == 0) {
        REQ_FD;
        REQ_FS1;
        REQ_FS2;
        word = enc_r(OP_FDIV, rd, rs1, rs2);
    } else if (strcmp(m, "FNEG") == 0) {
        REQ_FD;
        REQ_FS1;
        word = enc_r(OP_FNEG, rd, rs1, 0);
    } else if (strcmp(m, "FABS") == 0) {
        REQ_FD;
        REQ_FS1;
        word = enc_r(OP_FABS, rd, rs1, 0);
    } else if (strcmp(m, "FSQRT") == 0) {
        REQ_FD;
        REQ_FS1;
        word = enc_r(OP_FSQRT, rd, rs1, 0);
    } else if (strcmp(m, "FCVT.FI") == 0) {
        REQ_FD;
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        word = enc_r(OP_FCVT_FI, rd, rs1, 0);
    } else if (strcmp(m, "FCVT.IF") == 0) {
        REQ_RD;
        if (!op_freg(s, 1, &rs1, err)) {
            return 0;
        }
        word = enc_r(OP_FCVT_IF, rd, rs1, 0);
    } else if (strcmp(m, "FLW") == 0) {
        REQ_FD;
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_i(OP_FLW, rd, rs1, imm);
    } else if (strcmp(m, "FSW") == 0) {
        REQ_FD;
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_s(OP_FSW, rd, rs1, imm);
    } else if (strcmp(m, "FJEQ") == 0) {
        REQ_FD;
        REQ_FS1;
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_FJEQ, rd, rs1, imm);
    } else if (strcmp(m, "FJLT") == 0) {
        REQ_FD;
        REQ_FS1;
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_FJLT, rd, rs1, imm);
    } else if (strcmp(m, "FJGT") == 0) {
        REQ_FD;
        REQ_FS1;
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_FJGT, rd, rs1, imm);

        // branches
    } else if (strcmp(m, "JEQ") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JEQ, rd, rs1, imm);
    } else if (strcmp(m, "JNE") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JNE, rd, rs1, imm);
    } else if (strcmp(m, "JLT") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JLT, rd, rs1, imm);
    } else if (strcmp(m, "JGT") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JGT, rd, rs1, imm);
    } else if (strcmp(m, "JLE") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JLE, rd, rs1, imm);
    } else if (strcmp(m, "JGE") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JGE, rd, rs1, imm);
    } else if (strcmp(m, "JLTU") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JLTU, rd, rs1, imm);
    } else if (strcmp(m, "JGTU") == 0) {
        if (!op_reg(s, 0, &rd, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        if (!op_pcrel(s, 2, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_b(OP_JGTU, rd, rs1, imm);
    } else if (strcmp(m, "J") == 0) {
        REQ_RD;
        if (!op_pcrel(s, 1, sym, addr, &imm, err)) {
            return 0;
        }
        word = enc_j(OP_J, rd, imm);
    } else if (strcmp(m, "JR") == 0) {
        REQ_RD;
        REQ_RS1;
        if (!op_imm(s, 2, sym, &imm, err)) {
            return 0;
        }
        word = enc_i(OP_JR, rd, rs1, imm);

        // CSR
    } else if (strcmp(m, "CSRR") == 0) {
        REQ_RD;
        uint32_t cid;
        if (!op_csr(s, 1, sym, &cid, err)) {
            return 0;
        }
        word = enc_csr_read(rd, cid);
    } else if (strcmp(m, "CSRW") == 0) {
        uint32_t cid;
        if (!op_csr(s, 0, sym, &cid, err)) {
            return 0;
        }
        if (!op_reg(s, 1, &rs1, err)) {
            return 0;
        }
        word = enc_csr_write(cid, rs1);
    } else if (strcmp(m, "SYSCALL") == 0) {
        word = enc_system(0);
    } else if (strcmp(m, "SYSRET") == 0) {
        word = enc_system(1);
    } else if (strcmp(m, "HALT") == 0) {
        word = (uint32_t) OP_HALT << 26;
    } else {
        asm_err(err, s->line, "unknown mnemonic '%s'", m);
        return 0;
    }

    bb_push32(bb, word);
    return 1;
}

static int emit_dir(stmt *s, uint32_t *addr, symtab *sym, bytebuf *bb, asm_error *err) {
    const char *name = s->dir_name;

    if (strcasecmp(name, "entry") == 0 || strcasecmp(name, "equ") == 0) {
        return 1;
    }

    if (strcasecmp(name, "org") == 0) {
        uint32_t target = (uint32_t) s->dir_args[0].imm;
        while (*addr < target) {
            bb_push(bb, 0);
            (*addr)++;
        }
        return 1;
    }
    if (strcasecmp(name, "align") == 0) {
        uint32_t n = (uint32_t) s->dir_args[0].imm;
        uint32_t r = *addr % n;
        if (r) {
            uint32_t pad = n - r;
            while (pad--) {
                bb_push(bb, 0);
                (*addr)++;
            }
        }
        return 1;
    }
    if (strcasecmp(name, "word") == 0) {
        for (int i = 0; i < s->dir_nargs; i++) {
            operand *op = &s->dir_args[i];
            uint32_t val = 0;
            if (op->type == OP_FLOAT) {
                float f = op->fval;
                memcpy(&val, &f, 4);
            } else {
                int32_t imm;
                if (!op_imm(s, i, sym, &imm, err)) {
                    return 0;
                }
                val = (uint32_t) imm;
            }
            bb_push32(bb, val);
            *addr += 4;
        }
        return 1;
    }
    if (strcasecmp(name, "half") == 0) {
        for (int i = 0; i < s->dir_nargs; i++) {
            int32_t imm;
            if (!op_imm(s, i, sym, &imm, err)) {
                return 0;
            }
            bb_push(bb, (uint8_t) (imm & 0xFF));
            bb_push(bb, (uint8_t) ((imm >> 8) & 0xFF));
            *addr += 2;
        }
        return 1;
    }
    if (strcasecmp(name, "byte") == 0) {
        for (int i = 0; i < s->dir_nargs; i++) {
            int32_t imm;
            if (!op_imm(s, i, sym, &imm, err)) {
                return 0;
            }
            bb_push(bb, (uint8_t) (imm & 0xFF));
            (*addr)++;
        }
        return 1;
    }
    if (strcasecmp(name, "string") == 0) {
        if (s->dir_nargs < 1 || s->dir_args[0].type != OP_STR) {
            asm_err(err, s->line, ".string requires a string literal");
            return 0;
        }
        const char *str = s->dir_args[0].name;
        size_t len = strlen(str);
        for (size_t i = 0; i <= len; i++) {
            bb_push(bb, (uint8_t) str[i]);
            (*addr)++;
        }
        while (*addr % 4) {
            bb_push(bb, 0);
            (*addr)++;
        }
        return 1;
    }

    asm_err(err, s->line, "unknown directive '.%s'", name);
    return 0;
}

int encode(stmt_vec *stmts, uint8_t **out, size_t *out_len, asm_error *err) {
    // find entry
    char *entry_name = NULL;
    for (size_t i = 0; i < vec_len(stmts); i++) {
        stmt *s = &vec_get(stmts, i);
        if (s->type == STMT_DIR && strcasecmp(s->dir_name, "entry") == 0) {
            if (s->dir_nargs < 1 || s->dir_args[0].type != OP_NAME) {
                asm_err(err, s->line, ".entry requires a label name");
                return 0;
            }
            if (entry_name) {
                asm_err(err, s->line, "duplicate .entry directive");
                return 0;
            }
            entry_name = s->dir_args[0].name;
        }
    }

    // build symtab
    symtab sym = {0};
    if (!build_symtab(stmts, entry_name, &sym, err)) {
        free(sym.labels);
        free(sym.equs);
        return 0;
    }

    // emit
    bytebuf bb;
    bb_init(&bb);
    uint32_t addr = entry_name ? 4u : 0u;

    // entry jump
    if (entry_name) {
        uint32_t target = 0;
        if (!sym_find_label(&sym, entry_name, &target)) {
            asm_err(err, 0, "entry label '%s' not defined", entry_name);
            free(sym.labels);
            free(sym.equs);
            return 0;
        }
        bb_push32(&bb, enc_j(OP_J, 0, (int32_t) target));
    }

    for (size_t i = 0; i < vec_len(stmts); i++) {
        stmt *s = &vec_get(stmts, i);
        switch (s->type) {
            case STMT_LABEL:
            case STMT_EQU:
                break;
            case STMT_INSTR:
                if (!emit_instr(s, addr, &sym, &bb, err)) {
                    free(sym.labels);
                    free(sym.equs);
                    free(bb.data);
                    return 0;
                }
                addr += instr_size(s);
                break;
            case STMT_DIR:
                if (!emit_dir(s, &addr, &sym, &bb, err)) {
                    free(sym.labels);
                    free(sym.equs);
                    free(bb.data);
                    return 0;
                }
                break;
        }
    }

    free(sym.labels);
    free(sym.equs);

    *out = bb.data;
    *out_len = bb.len;
    return 1;
}