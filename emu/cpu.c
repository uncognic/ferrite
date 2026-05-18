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

#include "cpu.h"
#include "exception.h"
#include "isa.h"
#include "mem.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// write to general purpose register
static inline void wgpr(cpu *c, uint32_t r, uint32_t val) {
    if (r != 0) {
        c->gpr[r] = val;
    }
}

// write to general floating point register
static inline void wfpr(cpu *c, uint32_t r, float val) {
    c->fpr[r] = val;
}

// jump to pc
static inline void cpu_jump(cpu *c, int32_t offset) {
    c->pc = c->fetch_pc + (uint32_t) offset;
}

// check if current ring is supervisor, else raise exception
static inline int require_supervisor(cpu *c, uint32_t epc, exception *exc) {
    if (cpu_ring(c) == RING_SUPERVISOR) {
        return 1;
    }
    *exc = exc_fault_priv(epc);
    return 0;
}

// self explanatory
static inline void set_ring(cpu *c, ring r) {
    c->csr[CSR_STATUS] = (c->csr[CSR_STATUS] & ~0x3u) | (uint32_t) r;
}

void cpu_init(cpu *c) {
    memset(c, 0, sizeof(*c));
    set_ring(c, RING_SUPERVISOR);
}

// trap handling
static step_result take_exception(cpu *c, bus *b, exception exc) {
    // double fault
    if (c->in_trap) {
        fprintf(stderr, "[ferrite] double fault: cause=%u epc=%08x — halting\n", exc.cause,
                exc.epc);
        return STEP_HALTED;
    }

    // set state
    c->in_trap = 1;
    c->csr[CSR_CAUSE] = exc.cause;
    c->csr[CSR_EPC] = exc.epc;
    set_ring(c, RING_SUPERVISOR);

    // fetch handler from IVT
    uint32_t vec_addr = c->csr[CSR_IVT] + exc.cause * 4;
    uint32_t handler = 0;
    exception inner = {0};

    if (!bus_fetch32(b, vec_addr, vec_addr, &handler, &inner)) {
        fprintf(stderr, "[ferrite] fault reading IVT at %08x for cause=%u — halting\n", vec_addr,
                exc.cause);
        return STEP_HALTED;
    }
    // jump to handler
    c->pc = handler;
    c->in_trap = 0;
    return STEP_OK;
}

static step_result execute(cpu *c, bus *b, uint32_t instr) {
    uint32_t opcode1 = bits_extract(instr, 31, 26);
    uint32_t rd = bits_extract(instr, 25, 22);
    uint32_t rs1 = bits_extract(instr, 21, 18);
    uint32_t m_bit = bits_extract(instr, 17, 17);
    uint32_t op2_reg = bits_extract(instr, 16, 13);
    int32_t op2_imm = sign_extend(instr & 0x1FFFFu, 17);

    uint32_t a = c->gpr[rs1];
    uint32_t b_u = m_bit ? (uint32_t) op2_imm : c->gpr[op2_reg];
    int32_t a_i = (int32_t) a;
    int32_t b_i = (int32_t) b_u;
    uint32_t epc = c->fetch_pc;

    exception exc = {0};

    switch ((opcode) opcode1) {
        case OP_ADD:
            wgpr(c, rd, a + b_u);
            break;
        case OP_SUB:
            wgpr(c, rd, a - b_u);
            break;
        case OP_MUL:
            wgpr(c, rd, a * b_u);
            break;
        case OP_DIV:
            if (!b_u) {
                return take_exception(c, b, exc_div_zero(epc));
            }
            wgpr(c, rd, (uint32_t) (a_i / b_i));
            break;
        case OP_MOD:
            if (!b_u) {
                return take_exception(c, b, exc_div_zero(epc));
            }
            wgpr(c, rd, (uint32_t) (a_i % b_i));
            break;
        case OP_AND:
            wgpr(c, rd, a & b_u);
            break;
        case OP_OR:
            wgpr(c, rd, a | b_u);
            break;
        case OP_XOR:
            wgpr(c, rd, a ^ b_u);
            break;
        case OP_SHL:
            wgpr(c, rd, a << (b_u & 0x1F));
            break;
        case OP_SHR:
            wgpr(c, rd, a >> (b_u & 0x1F));
            break;
        case OP_SAR:
            wgpr(c, rd, (uint32_t) (a_i >> (b_u & 0x1F)));
            break;
        case OP_NOT:
            wgpr(c, rd, ~a);
            break;
        case OP_INC:
            wgpr(c, rd, a + 1);
            break;
        case OP_DEC:
            wgpr(c, rd, a - 1);
            break;
        case OP_NEG:
            wgpr(c, rd, (uint32_t) (-a_i));
            break;
        case OP_LW: {
            uint32_t addr = a + (uint32_t) op2_imm;
            uint32_t val = 0;
            if (!bus_read32(b, addr, cpu_ring_e(c), epc, &val, &exc)) {
                return take_exception(c, b, exc);
            }
            wgpr(c, rd, val);
            break;
        }
        case OP_LH: {
            uint32_t addr = a + (uint32_t) op2_imm;
            uint16_t val = 0;
            if (!bus_read16(b, addr, cpu_ring_e(c), epc, &val, &exc)) {
                return take_exception(c, b, exc);
            }
            wgpr(c, rd, val);
            break;
        }
        case OP_LB: {
            uint32_t addr = a + (uint32_t) op2_imm;
            uint8_t val = 0;
            if (!bus_read8(b, addr, cpu_ring_e(c), epc, &val, &exc)) {
                return take_exception(c, b, exc);
            }
            wgpr(c, rd, val);
            break;
        }
        case OP_SW: {
            uint32_t addr = a + (uint32_t) op2_imm;
            if (!bus_write32(b, addr, cpu_ring_e(c), epc, c->gpr[rd], &exc)) {
                return take_exception(c, b, exc);
            }
            break;
        }
        case OP_SH: {
            uint32_t addr = a + (uint32_t) op2_imm;
            if (!bus_write16(b, addr, cpu_ring_e(c), epc, (uint16_t) c->gpr[rd], &exc)) {
                return take_exception(c, b, exc);
            }
            break;
        }
        case OP_SB: {
            uint32_t addr = a + (uint32_t) op2_imm;
            if (!bus_write8(b, addr, cpu_ring_e(c), epc, (uint8_t) c->gpr[rd], &exc)) {
                return take_exception(c, b, exc);
            }
            break;
        }
        case OP_LUI:
            wgpr(c, rd, bits_extract(instr, 21, 0) << 10);
            break;
        case OP_FADD:
            wfpr(c, rd, c->fpr[rs1] + c->fpr[op2_reg]);
            break;
        case OP_FSUB:
            wfpr(c, rd, c->fpr[rs1] - c->fpr[op2_reg]);
            break;
        case OP_FMUL:
            wfpr(c, rd, c->fpr[rs1] * c->fpr[op2_reg]);
            break;
        case OP_FDIV:
            wfpr(c, rd, c->fpr[rs1] / c->fpr[op2_reg]);
            break;
        case OP_FNEG:
            wfpr(c, rd, -c->fpr[rs1]);
            break;
        case OP_FABS:
            wfpr(c, rd, fabsf(c->fpr[rs1]));
            break;
        case OP_FSQRT:
            wfpr(c, rd, sqrtf(c->fpr[rs1]));
            break;
        case OP_FCVT_FI:
            wfpr(c, rd, (float) (int32_t) c->gpr[rs1]);
            break;
        case OP_FCVT_IF:
            wgpr(c, rd, (uint32_t) (int32_t) c->fpr[rs1]);
            break;
        case OP_FLW: {
            uint32_t addr = a + (uint32_t) op2_imm;
            uint32_t raw = 0;
            if (!bus_read32(b, addr, cpu_ring_e(c), epc, &raw, &exc)) {
                return take_exception(c, b, exc);
            }
            float f;
            memcpy(&f, &raw, 4);
            wfpr(c, rd, f);
            break;
        }
        case OP_FSW: {
            uint32_t addr = a + (uint32_t) op2_imm;
            uint32_t raw = 0;
            memcpy(&raw, &c->fpr[rd], 4);
            if (!bus_write32(b, addr, cpu_ring_e(c), epc, raw, &exc)) {
                return take_exception(c, b, exc);
            }
            break;
        }
        case OP_FJEQ: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            float fs1 = c->fpr[rd];
            float fs2 = c->fpr[rs1];
            if (fs1 == fs2) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_FJLT: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if (c->fpr[rd] < c->fpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_FJGT: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if (c->fpr[rd] > c->fpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JEQ: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if ((int32_t) c->gpr[rd] == (int32_t) c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JNE: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if ((int32_t) c->gpr[rd] != (int32_t) c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JLT: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if ((int32_t) c->gpr[rd] < (int32_t) c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JGT: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if ((int32_t) c->gpr[rd] > (int32_t) c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JLE: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if ((int32_t) c->gpr[rd] <= (int32_t) c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JGE: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if ((int32_t) c->gpr[rd] >= (int32_t) c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JLTU: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if (c->gpr[rd] < c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_JGTU: {
            int32_t off = sign_extend(bits_extract(instr, 17, 0), 18);
            if (c->gpr[rd] > c->gpr[rs1]) {
                cpu_jump(c, off);
            }
            break;
        }
        case OP_J: {
            int32_t off = sign_extend(bits_extract(instr, 21, 0), 22);
            uint32_t ret = c->pc;
            wgpr(c, rd, ret);
            c->pc = c->fetch_pc + (uint32_t) off;
            break;
        }
        case OP_JR: {
            uint32_t ret = c->pc;
            wgpr(c, rd, ret);
            c->pc = a + (uint32_t) op2_imm;
            break;
        }
        case OP_CSR: {
            if (!require_supervisor(c, epc, &exc)) {
                return take_exception(c, b, exc);
            }
            uint32_t csr_id = bits_extract(instr, 16, 14);
            uint32_t dir_bit = bits_extract(instr, 13, 13);
            if (!dir_bit) {
                wgpr(c, rd, c->csr[csr_id]);
            } else {
                c->csr[csr_id] = c->gpr[rs1];
            }
            break;
        }
        case OP_SYSTEM: {
            uint32_t dir = bits_extract(instr, 13, 13);
            if (!dir) {
                // SYSCALL
                return take_exception(c, b, exc_syscall(epc));
            } else {
                // SYSRET
                if (!require_supervisor(c, epc, &exc)) {
                    return take_exception(c, b, exc);
                }
                c->pc = c->csr[CSR_EPC];
                set_ring(c, RING_USER);
            }
            break;
        }

        case OP_HALT: {
            if (!require_supervisor(c, epc, &exc)) {
                return take_exception(c, b, exc);
            }
            return STEP_HALTED;
        }

        default:
            return take_exception(c, b, exc_invalid(epc));
    }

    return STEP_OK;
}

step_result cpu_step(cpu *c, bus *b) {
    int32_t irq = bus_tick(b);
    if (irq >= 0) {
        exception exc = {(cause) irq, c->pc};
        return take_exception(c, b, exc);
    }

    c->fetch_pc = c->pc;
    uint32_t instr = 0;
    exception exc = {0};
    if (!bus_fetch32(b, c->fetch_pc, c->fetch_pc, &instr, &exc)) {
        return take_exception(c, b, exc);
    }

    c->pc += 4;
    return execute(c, b, instr);
}