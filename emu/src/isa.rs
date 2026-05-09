/*
* Copyright (C) 2026 uncognic
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

// Ferrite ISA

// 6 bit opcodes

pub mod op {
    pub const ADD: u32 = 0x00;
    pub const SUB: u32 = 0x01;
    pub const MUL: u32 = 0x02;
    pub const DIV: u32 = 0x03;
    pub const MOD: u32 = 0x04;
    pub const AND: u32 = 0x05;
    pub const OR: u32 = 0x06;
    pub const XOR: u32 = 0x07;
    pub const SHL: u32 = 0x08;
    pub const SHR: u32 = 0x09;
    pub const SAR: u32 = 0x0A;
    pub const NOT: u32 = 0x0B;
    pub const INC: u32 = 0x0C;
    pub const DEC: u32 = 0x0D;
    pub const NEG: u32 = 0x0E;

    pub const LW: u32 = 0x10;
    pub const LH: u32 = 0x11;
    pub const LB: u32 = 0x12;
    pub const SW: u32 = 0x13;
    pub const SH: u32 = 0x14;
    pub const SB: u32 = 0x15;
    pub const LUI: u32 = 0x16;

    pub const FADD: u32 = 0x20;
    pub const FSUB: u32 = 0x21;
    pub const FMUL: u32 = 0x22;
    pub const FDIV: u32 = 0x23;
    pub const FNEG: u32 = 0x24;
    pub const FABS: u32 = 0x25;
    pub const FSQRT: u32 = 0x26;
    pub const FCVT_FI: u32 = 0x27;
    pub const FCVT_IF: u32 = 0x28;
    pub const FLW: u32 = 0x29;
    pub const FSW: u32 = 0x2A;
    pub const FJEQ: u32 = 0x2B;
    pub const FJLT: u32 = 0x2C;
    pub const FJGT: u32 = 0x2D;

    pub const JEQ: u32 = 0x30;
    pub const JNE: u32 = 0x31;
    pub const JLT: u32 = 0x32;
    pub const JGT: u32 = 0x33;
    pub const JLE: u32 = 0x34;
    pub const JGE: u32 = 0x35;
    pub const JLTU: u32 = 0x36;
    pub const JGTU: u32 = 0x37;

    pub const J: u32 = 0x38;
    pub const JR: u32 = 0x39;

    pub const CSR: u32 = 0x3D;
    pub const SYSTEM: u32 = 0x3E;
    pub const HALT: u32 = 0x3F;
}

// csr ids
pub mod csr {
    pub const STATUS: u32 = 0;
    pub const IVT: u32 = 1;
    pub const CAUSE: u32 = 2;
    pub const EPC: u32 = 3;
    pub const ESAVE: u32 = 4;
}

// exception / interrupt causes
pub mod cause {
    pub const SYSCALL: u32 = 0;
    pub const FAULT_MEM: u32 = 1;
    pub const FAULT_PRIV: u32 = 2;
    pub const DIV_ZERO: u32 = 3;
    pub const INVALID: u32 = 4;
    // reserved
    pub const INT_UART_RX: u32 = 16;
}

// memory map
pub mod mmap {
    pub const ROM_BASE: u32 = 0x0000_0000;
    pub const ROM_END: u32 = 0x0000_FFFF;
    pub const RAM_BASE: u32 = 0x0001_0000;
    pub const RAM_END: u32 = 0x7FFF_FFFF;
    pub const MMIO_BASE: u32 = 0x8000_0000;
    pub const MMIO_END: u32 = 0xFFFF_FFFF;

    pub const UART_TX: u32 = 0x8000_0000;
    pub const UART_RX: u32 = 0x8000_0004;
    pub const UART_STATUS: u32 = 0x8000_0008;
}

// helpers
#[inline]
pub fn sign_extend(val: u32, bits: u32) -> i32 {
    let shift = 32 - bits;
    ((val << shift) as i32) >> shift
}
#[inline]
pub fn bits(instr: u32, hi: u32, lo: u32) -> u32 {
    (instr >> lo) & ((1 << (hi - lo + 1)) - 1)
}
