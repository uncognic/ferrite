use std::collections::HashMap;

use crate::{
    ast::{Operand, Statement},
    error::AsmError,
};
use ferrite_emu::isa::{csr, op, sign_extend};

pub struct Encoder {
    stmts: Vec<Statement>,
}

impl Encoder {
    pub fn new(stmts: Vec<Statement>) -> Self {
        Self { stmts }
    }

    pub fn encode(self) -> Result<Vec<u8>, AsmError> {
        // find entry point
        let entry = self.find_entry()?;
        // handle .equs
        let equs = self.find_equs()?;
        // build labels
        let labels = self.build_label_map(&entry)?;
        // emit bytes
        self.emit(&entry, &labels, &equs)
    }

    fn find_equs(&self) -> Result<HashMap<String, i32>, AsmError> {
        let mut map = HashMap::new();
        for stmt in &self.stmts {
            if let Statement::Equ { name, value } = stmt {
                if map.insert(name.clone(), *value).is_some() {
                    return Err(AsmError::new(0, format!("duplicate .equ '{}'", name)));
                }
            }
        }
        Ok(map)
    }
    fn find_entry(&self) -> Result<Option<String>, AsmError> {
        let mut found = None;
        for stmt in &self.stmts {
            if let Statement::Directive { name, args, line } = stmt {
                if name == "entry" {
                    if found.is_some() {
                        return Err(AsmError::new(*line, "duplicate .entry directive"));
                    }
                    match args.first() {
                        Some(Operand::Name(s)) => {
                            found = Some(s.clone());
                        }
                        _ => return Err(AsmError::new(*line, ".entry requires a label name")),
                    }
                }
            }
        }
        Ok(found)
    }

    fn build_label_map(&self, entry: &Option<String>) -> Result<HashMap<String, u32>, AsmError> {
        let mut map = HashMap::new();
        let mut addr = if entry.is_some() { 4u32 } else { 0u32 };

        for stmt in &self.stmts {
            match stmt {
                Statement::Label(name) => {
                    if map.insert(name.clone(), addr).is_some() {
                        return Err(AsmError::new(0, format!("duplicate label '{}'", name)));
                    }
                }
                Statement::Equ { .. } => {}
                Statement::Instruction {
                    mnemonic, operands, ..
                } => {
                    addr += instruction_size(mnemonic, operands);
                }
                Statement::Directive { name, args, line } => match name.as_str() {
                    "entry" | "equ" => {}
                    "org" => {
                        let target = match args.first() {
                            Some(Operand::Imm(n)) => *n as u32,
                            _ => return Err(AsmError::new(*line, ".org requires an address")),
                        };
                        if target < addr {
                            return Err(AsmError::new(
                                *line,
                                format!(
                                    ".org address {:#010x} is behind current address {:#010x}",
                                    target, addr
                                ),
                            ));
                        }
                        addr = target;
                    }
                    "align" => {
                        let n = match args.first() {
                            Some(Operand::Imm(n)) if *n > 0 => *n as u32,
                            _ => {
                                return Err(AsmError::new(
                                    *line,
                                    ".align requires a positive integer",
                                ));
                            }
                        };
                        let remainder = addr % n;
                        if remainder != 0 {
                            addr += n - remainder;
                        }
                    }
                    _ => {
                        addr += directive_size(name, args, *line)?;
                    }
                },
            }
        }
        Ok(map)
    }

    fn emit(
        &self,
        entry: &Option<String>,
        labels: &HashMap<String, u32>,
        equs: &HashMap<String, i32>,
    ) -> Result<Vec<u8>, AsmError> {
        let mut out = Vec::new();
        let mut addr = if entry.is_some() { 4u32 } else { 0u32 };

        if let Some(name) = entry {
            let target = labels
                .get(name)
                .copied()
                .ok_or_else(|| AsmError::new(0, format!("entry label '{}' not defined", name)))?;
            let offset = target as i32;
            out.extend_from_slice(&enc_j(op::J, 0, offset).to_le_bytes());
        }

        for stmt in &self.stmts {
            match stmt {
                Statement::Label(_) | Statement::Equ { .. } => {}

                Statement::Instruction {
                    mnemonic,
                    operands,
                    line,
                } => {
                    let words =
                        self.encode_instruction(*line, addr, mnemonic, operands, labels, equs)?;
                    for w in words {
                        out.extend_from_slice(&w.to_le_bytes());
                        addr += 4;
                    }
                }

                Statement::Directive { name, args, line } => {
                    match name.as_str() {
                        "entry" => {}

                        "org" => {
                            let target = match args.first() {
                                Some(Operand::Imm(n)) => *n as u32,
                                Some(Operand::Name(s)) => equs.get(s).copied().ok_or_else(|| {
                                    AsmError::new(*line, format!("undefined name '{}'", s))
                                })?
                                    as u32,
                                _ => return Err(AsmError::new(*line, ".org requires an address")),
                            };
                            if target < addr {
                                return Err(AsmError::new(
                                    *line,
                                    format!(
                                        ".org address {:#010x} is behind current address {:#010x}",
                                        target, addr
                                    ),
                                ));
                            }
                            // pad with zeros
                            let padding = (target - addr) as usize;
                            out.extend(std::iter::repeat(0u8).take(padding));
                            addr = target;
                        }

                        "align" => {
                            let n = match args.first() {
                                Some(Operand::Imm(n)) if *n > 0 => *n as u32,
                                _ => {
                                    return Err(AsmError::new(
                                        *line,
                                        ".align requires a positive integer",
                                    ));
                                }
                            };
                            if !n.is_power_of_two() {
                                return Err(AsmError::new(
                                    *line,
                                    ".align argument must be a power of two",
                                ));
                            }
                            let remainder = addr % n;
                            if remainder != 0 {
                                let padding = (n - remainder) as usize;
                                out.extend(std::iter::repeat(0u8).take(padding));
                                addr += n - remainder;
                            }
                        }

                        _ => {
                            let bytes = encode_directive(*line, name, args, labels)?;
                            addr += bytes.len() as u32;
                            out.extend_from_slice(&bytes);
                        }
                    }
                }
            }
        }
        Ok(out)
    }
    fn encode_instruction(
        &self,
        line: usize,
        addr: u32,
        mnemonic: &str,
        operands: &[Operand],
        labels: &HashMap<String, u32>,
        equs: &HashMap<String, i32>,
    ) -> Result<Vec<u32>, AsmError> {
        let reg = |i: usize| {
            operands.get(i).and_then(|o| {
                if let Operand::Reg(r) = o {
                    Some(*r)
                } else {
                    None
                }
            })
        };
        let freg = |i: usize| {
            operands.get(i).and_then(|o| {
                if let Operand::FReg(r) = o {
                    Some(*r)
                } else {
                    None
                }
            })
        };
        let imm = |i: usize| -> Result<i32, AsmError> {
            match operands.get(i) {
                Some(Operand::Imm(n)) => Ok(*n),
                Some(Operand::Name(s)) => {
                    // check equs first, then labels
                    if let Some(&v) = equs.get(s) {
                        return Ok(v);
                    }
                    labels
                        .get(s)
                        .copied()
                        .map(|a| a as i32)
                        .ok_or_else(|| AsmError::new(line, format!("undefined name '{}'", s)))
                }
                _ => Err(AsmError::new(
                    line,
                    format!("{}: expected immediate at operand {}", mnemonic, i),
                )),
            }
        };
        let pcrel = |i: usize| -> Result<i32, AsmError> {
            let target = imm(i)?;
            Ok(target - addr as i32)
        };
        let csr_id = |i: usize| -> Result<u32, AsmError> {
            match operands.get(i) {
                Some(Operand::Imm(n)) => Ok(*n as u32),
                Some(Operand::Name(s)) => resolve_csr_name(s)
                    .ok_or_else(|| AsmError::new(line, format!("unknown CSR '{}'", s))),
                _ => Err(AsmError::new(
                    line,
                    format!("{}: expected CSR at operand {}", mnemonic, i),
                )),
            }
        };
        let req_reg = |i: usize| {
            reg(i).ok_or_else(|| {
                AsmError::new(line, format!("{}: expected GPR at operand {}", mnemonic, i))
            })
        };
        let req_freg = |i: usize| {
            freg(i).ok_or_else(|| {
                AsmError::new(line, format!("{}: expected FPR at operand {}", mnemonic, i))
            })
        };

        let word = match mnemonic {
            "NOP" => enc_r(op::ADD, 0, 0, 0),
            "CLR" => enc_r(op::ADD, req_reg(0)?, 0, 0),
            "MOV" => match operands.get(1) {
                Some(Operand::Reg(rs)) => enc_r(op::ADD, req_reg(0)?, *rs, 0),
                _ => enc_i(op::ADD, req_reg(0)?, 0, imm(1)?),
            },
            "NEG" => enc_r(op::SUB, req_reg(0)?, 0, req_reg(1)?),
            "JZ" => enc_b(op::JEQ, req_reg(0)?, 0, pcrel(1)?),
            "JNZ" => enc_b(op::JNE, req_reg(0)?, 0, pcrel(1)?),
            "CALL" => enc_j(op::J, 1, pcrel(0)?),
            "RET" => enc_jr(op::JR, 0, 1, 0),

            "J" if operands.len() == 1 => enc_j(op::J, 0, pcrel(0)?),

            "LI" => return encode_li(line, req_reg(0)?, imm(1)?),
            "ADD" => enc_ri(op::ADD, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "SUB" => enc_ri(op::SUB, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "MUL" => enc_ri(op::MUL, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "DIV" => enc_ri(op::DIV, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "MOD" => enc_ri(op::MOD, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "AND" => enc_ri(op::AND, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "OR" => enc_ri(op::OR, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "XOR" => enc_ri(op::XOR, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "SHL" => enc_ri(op::SHL, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "SHR" => enc_ri(op::SHR, req_reg(0)?, req_reg(1)?, operands, 2, line)?,
            "SAR" => enc_ri(op::SAR, req_reg(0)?, req_reg(1)?, operands, 2, line)?,

            "NOT" => enc_r(op::NOT, req_reg(0)?, req_reg(1)?, 0),
            "INC" => enc_r(op::INC, req_reg(0)?, req_reg(0)?, 0),
            "DEC" => enc_r(op::DEC, req_reg(0)?, req_reg(0)?, 0),
            "NEG2" => enc_r(op::NEG, req_reg(0)?, 0, 0),

            "LW" => enc_i(op::LW, req_reg(0)?, req_reg(1)?, imm(2)?),
            "LH" => enc_i(op::LH, req_reg(0)?, req_reg(1)?, imm(2)?),
            "LB" => enc_i(op::LB, req_reg(0)?, req_reg(1)?, imm(2)?),
            "SW" => enc_s(op::SW, req_reg(0)?, req_reg(1)?, imm(2)?),
            "SH" => enc_s(op::SH, req_reg(0)?, req_reg(1)?, imm(2)?),
            "SB" => enc_s(op::SB, req_reg(0)?, req_reg(1)?, imm(2)?),
            "LUI" => enc_u(op::LUI, req_reg(0)?, imm(1)? as u32),

            "FADD" => enc_r(op::FADD, req_freg(0)?, req_freg(1)?, req_freg(2)?),
            "FSUB" => enc_r(op::FSUB, req_freg(0)?, req_freg(1)?, req_freg(2)?),
            "FMUL" => enc_r(op::FMUL, req_freg(0)?, req_freg(1)?, req_freg(2)?),
            "FDIV" => enc_r(op::FDIV, req_freg(0)?, req_freg(1)?, req_freg(2)?),
            "FNEG" => enc_r(op::FNEG, req_freg(0)?, req_freg(1)?, 0),
            "FABS" => enc_r(op::FABS, req_freg(0)?, req_freg(1)?, 0),
            "FSQRT" => enc_r(op::FSQRT, req_freg(0)?, req_freg(1)?, 0),
            "FCVT.FI" => enc_r(op::FCVT_FI, req_freg(0)?, req_reg(1)?, 0),
            "FCVT.IF" => enc_r(op::FCVT_IF, req_reg(0)?, req_freg(1)?, 0),
            "FLW" => enc_i(op::FLW, req_freg(0)?, req_reg(1)?, imm(2)?),
            "FSW" => enc_s(op::FSW, req_freg(0)?, req_reg(1)?, imm(2)?),

            "FJEQ" => enc_b(op::FJEQ, req_freg(0)?, req_freg(1)?, pcrel(2)?),
            "FJLT" => enc_b(op::FJLT, req_freg(0)?, req_freg(1)?, pcrel(2)?),
            "FJGT" => enc_b(op::FJGT, req_freg(0)?, req_freg(1)?, pcrel(2)?),

            "JEQ" => enc_b(op::JEQ, req_reg(0)?, req_reg(1)?, pcrel(2)?),
            "JNE" => enc_b(op::JNE, req_reg(0)?, req_reg(1)?, pcrel(2)?),
            "JLT" => enc_b(op::JLT, req_reg(0)?, req_reg(1)?, pcrel(2)?),
            "JGT" => enc_b(op::JGT, req_reg(0)?, req_reg(1)?, pcrel(2)?),
            "JLE" => enc_b(op::JLE, req_reg(0)?, req_reg(1)?, pcrel(2)?),
            "JGE" => enc_b(op::JGE, req_reg(0)?, req_reg(1)?, pcrel(2)?),
            "JLTU" => enc_b(op::JLTU, req_reg(0)?, req_reg(1)?, pcrel(2)?),
            "JGTU" => enc_b(op::JGTU, req_reg(0)?, req_reg(1)?, pcrel(2)?),

            "J" => enc_j(op::J, req_reg(0)?, pcrel(1)?),
            "JR" => enc_jr(op::JR, req_reg(0)?, req_reg(1)?, imm(2)?),

            "CSRR" => {
                let rd = req_reg(0)?;
                let cid = csr_id(1)?;
                enc_csr_read(rd, cid)
            }
            "CSRW" => {
                let cid = csr_id(0)?;
                let rs1 = req_reg(1)?;
                enc_csr_write(cid, rs1)
            }

            "SYSCALL" => enc_system(0),
            "SYSRET" => enc_system(1),
            "HALT" => op::HALT << 26,

            _ => {
                return Err(AsmError::new(
                    line,
                    format!("unknown mnemonic '{}'", mnemonic),
                ));
            }
        };

        Ok(vec![word])
    }
}

/// rd is the register being written to
/// rs are the source registers

/// R format
/// two source registers, one destination register, no immediate
fn enc_r(opcode: u32, rd: u8, rs1: u8, rs2: u8) -> u32 {
    (opcode << 26) | ((rd as u32) << 22) | ((rs1 as u32) << 18) | ((rs2 as u32) << 13)
}

/// I format
/// one source register, one destination register, 17 bit immediate
/// the R and I formats are differentiated by bit 17 of the Instructionuction (1 for I, 0 for R)
fn enc_i(opcode: u32, rd: u8, rs1: u8, imm: i32) -> u32 {
    let imm17 = (imm as u32) & 0x1_FFFF;
    (opcode << 26) | ((rd as u32) << 22) | ((rs1 as u32) << 18) | (1 << 17) | imm17
}

/// S format, stores
/// rs1 has the address base, rs2 has the value to store, 17 bit immediate offset
fn enc_s(opcode: u32, rs2: u8, rs1: u8, imm: i32) -> u32 {
    enc_i(opcode, rs2, rs1, imm)
}

/// B format, small jumps
/// no destination, just compare rs1 and rs2 and jump to pc+offset with 18 bit address offset
fn enc_b(opcode: u32, rs1: u8, rs2: u8, offset: i32) -> u32 {
    let off18 = (offset as u32) & 0x3_FFFF;
    (opcode << 26) | ((rs1 as u32) << 22) | ((rs2 as u32) << 18) | off18
}

/// J format, unconditional large jumps
/// same as B except with a 22 bit offset and no rs2
fn enc_j(opcode: u32, rd: u8, offset: i32) -> u32 {
    let off22 = (offset as u32) & 0x3F_FFFF;
    (opcode << 26) | ((rd as u32) << 22) | off22
}

fn enc_jr(opcode: u32, rd: u8, rs1: u8, imm: i32) -> u32 {
    enc_i(opcode, rd, rs1, imm)
}

/// U format, large immediates
/// loads a 22 bit immediate into the upper bits of the register, lower bits are zeroed
fn enc_u(opcode: u32, rd: u8, imm22: u32) -> u32 {
    (opcode << 26) | ((rd as u32) << 22) | (imm22 & 0x3F_FFFF)
}

fn enc_csr_read(rd: u8, csr_id: u32) -> u32 {
    (op::CSR << 26) | ((rd as u32) << 22) | (csr_id << 14)
}

fn enc_csr_write(csr_id: u32, rs1: u8) -> u32 {
    (op::CSR << 26) | ((rs1 as u32) << 18) | (csr_id << 14) | (1 << 13)
}

fn enc_system(directive: u32) -> u32 {
    (op::SYSTEM << 26) | (directive << 13)
}

fn enc_ri(
    opcode: u32,
    rd: u8,
    rs1: u8,
    operands: &[Operand],
    op_idx: usize,
    line: usize,
) -> Result<u32, AsmError> {
    match operands.get(op_idx) {
        Some(Operand::Reg(rs2)) => Ok(enc_r(opcode, rd, rs1, *rs2)),
        Some(Operand::Imm(imm)) => Ok(enc_i(opcode, rd, rs1, *imm)),
        _ => Err(AsmError::new(
            line,
            "expected register or immediate as last operand",
        )),
    }
}

fn encode_li(_line: usize, rd: u8, imm: i32) -> Result<Vec<u32>, AsmError> {
    let val = imm as u32;
    let lower = val & 0x3FF; // low 10 bits
    let mut upper = val >> 10; // high 22 bits

    // if bit 9 of lower is set, ADD will sign-extend it to a negative number.
    // compensate by incrementing upper so the result is still correct.
    if lower & 0x200 != 0 {
        upper = upper.wrapping_add(1);
    }

    let lower_signed = sign_extend(lower, 10);
    Ok(vec![
        enc_u(op::LUI, rd, upper),
        enc_i(op::ADD, rd, rd, lower_signed),
    ])
}

fn directive_size(name: &str, args: &[Operand], line: usize) -> Result<u32, AsmError> {
    match name {
        "word" => Ok(4 * args.len() as u32),
        "half" => Ok(2 * args.len() as u32),
        "byte" => Ok(args.len() as u32),
        "string" => match args.first() {
            Some(Operand::Str(s)) => {
                let len = s.len() as u32 + 1;
                Ok((len + 3) & !3)
            }
            _ => Err(AsmError::new(line, ".string requires a string literal")),
        },
        "entry" | "org" | "align" | "equ" => Ok(0),
        _ => Err(AsmError::new(
            line,
            format!("unknown directive '.{}'", name),
        )),
    }
}

fn encode_directive(
    line: usize,
    name: &str,
    args: &[Operand],
    labels: &HashMap<String, u32>,
) -> Result<Vec<u8>, AsmError> {
    let resolve = |op: &Operand| -> Result<i32, AsmError> {
        match op {
            Operand::Imm(n) => Ok(*n),
            Operand::Name(s) => labels
                .get(s)
                .copied()
                .map(|a| a as i32)
                .ok_or_else(|| AsmError::new(line, format!("undefined label '{}'", s))),
            _ => Err(AsmError::new(line, "expected immediate in directive")),
        }
    };

    match name {
        "word" => {
            let mut out = Vec::new();
            for op in args {
                match op {
                    Operand::Float(f) => out.extend_from_slice(&f.to_bits().to_le_bytes()),
                    _ => out.extend_from_slice(&(resolve(op)? as u32).to_le_bytes()),
                }
            }
            Ok(out)
        }
        "half" => {
            let mut out = Vec::new();
            for op in args {
                out.extend_from_slice(&(resolve(op)? as u16).to_le_bytes());
            }
            Ok(out)
        }
        "byte" => {
            let mut out = Vec::new();
            for op in args {
                out.push(resolve(op)? as u8);
            }
            Ok(out)
        }
        "string" => match args.first() {
            Some(Operand::Str(s)) => {
                let mut out: Vec<u8> = s.bytes().collect();
                out.push(0);
                while out.len() % 4 != 0 {
                    out.push(0);
                }
                Ok(out)
            }
            _ => Err(AsmError::new(line, ".string requires a string literal")),
        },
        "org" | "align" | "entry" | "equ" => Ok(Vec::new()),
        _ => Err(AsmError::new(
            line,
            format!("unknown directive '.{}'", name),
        )),
    }
}

fn instruction_size(mnemonic: &str, _operands: &[Operand]) -> u32 {
    match mnemonic {
        "LI" => 8,
        _ => 4,
    }
}

fn resolve_csr_name(name: &str) -> Option<u32> {
    match name.to_ascii_uppercase().as_str() {
        "STATUS" => Some(csr::STATUS),
        "IVT" => Some(csr::IVT),
        "CAUSE" => Some(csr::CAUSE),
        "EPC" => Some(csr::EPC),
        "ESAVE" => Some(csr::ESAVE),
        _ => None,
    }
}
