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
#include <stdint.h>

typedef enum {
    OP_ADD = 0x00,
    OP_SUB = 0x01,
    OP_MUL = 0x02,
    OP_DIV = 0x03,
    OP_MOD = 0x04,
    OP_AND = 0x05,
    OP_OR = 0x06,
    OP_XOR = 0x07,
    OP_SHL = 0x08,
    OP_SHR = 0x09,
    OP_SAR = 0x0A,
    OP_NOT = 0x0B,
    OP_INC = 0x0C,
    OP_DEC = 0x0D,
    OP_NEG = 0x0E,

    OP_LW = 0x10,
    OP_LH = 0x11,
    OP_LB = 0x12,
    OP_SW = 0x13,
    OP_SH = 0x14,
    OP_SB = 0x15,
    OP_LUI = 0x16,

    OP_FADD = 0x20,
    OP_FSUB = 0x21,
    OP_FMUL = 0x22,
    OP_FDIV = 0x23,
    OP_FNEG = 0x24,
    OP_FABS = 0x25,
    OP_FSQRT = 0x26,
    OP_FCVT_FI = 0x27,
    OP_FCVT_IF = 0x28,
    OP_FLW = 0x29,
    OP_FSW = 0x2A,
    OP_FJEQ = 0x2B,
    OP_FJLT = 0x2C,
    OP_FJGT = 0x2D,

    OP_JEQ = 0x30,
    OP_JNE = 0x31,
    OP_JLT = 0x32,
    OP_JGT = 0x33,
    OP_JLE = 0x34,
    OP_JGE = 0x35,
    OP_JLTU = 0x36,
    OP_JGTU = 0x37,

    OP_J = 0x38,
    OP_JR = 0x39,

    OP_CSR = 0x3D,
    OP_SYSTEM = 0x3E,
    OP_HALT = 0x3F,
} opcode;

typedef enum {
    CSR_STATUS = 0,
    CSR_IVT = 1,
    CSR_CAUSE = 2,
    CSR_EPC = 3,
    CSR_ESAVE = 4,
} csr_id;

typedef enum {
    CAUSE_SYSCALL = 0,
    CAUSE_FAULT_MEM = 1,
    CAUSE_FAULT_PRIV = 2,
    CAUSE_DIV_ZERO = 3,
    CAUSE_INVALID = 4,
    // reserved
    CAUSE_INT_UART_RX = 16,
} cause;

#define ROM_BASE 0x00000000u
#define ROM_END 0x0000FFFFu
#define RAM_BASE 0x00010000u
#define RAM_END 0x7FFFFFFFu
#define MMIO_BASE 0x80000000u
#define MMIO_END 0xFFFFFFFFu

#define UART_TX 0x80000000u
#define UART_RX 0x80000004u
#define UART_STATUS 0x80000008u

static inline int32_t sign_extend(uint32_t val, uint32_t bits) {
    uint32_t shift = 32 - bits;
    return (int32_t) (val << shift) >> shift;
}

static inline uint32_t bits_extract(uint32_t instr, uint32_t hi, uint32_t lo) {
    return (instr >> lo) & ((1u << (hi - lo + 1)) - 1);
}