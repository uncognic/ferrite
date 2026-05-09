use crate::{
    exception::Exception,
    isa::{bits, csr, op, sign_extend},
    mem::Bus,
    privilege::Ring,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StepResult {
    Ok,
    Halted,
}

pub struct Cpu {
    gpr: [u32; 16],
    fpr: [f32; 16],
    pc: u32,
    csr: [u32; 8],
    fetch_pc: u32,
    in_trap: bool,
}

impl Cpu {
    pub fn new() -> Self {
        let mut cpu = Self {
            gpr: [0; 16],
            fpr: [0.0; 16],
            pc: 0,
            csr: [0; 8],
            fetch_pc: 0,
            in_trap: false,
        };
        cpu.set_ring(Ring::Supervisor);
        return cpu;
    }

    pub fn pc(&self) -> u32 {
        self.pc
    }
    pub fn gpr(&self) -> &[u32; 16] {
        &self.gpr
    }
    pub fn fpr(&self) -> &[f32; 16] {
        &self.fpr
    }
    pub fn ring(&self) -> Ring {
        Ring::from_u32(self.csr[csr::STATUS as usize])
    }

    // step
    pub fn step(&mut self, bus: &mut Bus) -> StepResult {
        if let Some(cause) = bus.tick_devices() {
            return self.take_exception(
                Exception {
                    cause,
                    epc: self.pc,
                },
                bus,
            );
        }

        self.fetch_pc = self.pc;
        let instr = match bus.fetch32(self.fetch_pc, self.fetch_pc) {
            Ok(w) => w,
            Err(e) => return self.take_exception(e, bus),
        };
        self.pc = self.pc.wrapping_add(4);

        match self.execute(instr, bus) {
            Ok(r) => r,
            Err(e) => self.take_exception(e, bus),
        }
    }

    // exec
    fn execute(&mut self, instr: u32, bus: &mut Bus) -> Result<StepResult, Exception> {
        let opcode = bits(instr, 31, 26);
        let rd = bits(instr, 25, 22) as usize;
        let rs1 = bits(instr, 21, 18) as usize;
        let m_bit = bits(instr, 17, 17);
        let op2_reg = bits(instr, 16, 13) as usize;
        let op2_imm = sign_extend(instr & 0x1_FFFF, 17);

        let a = self.gpr[rs1];
        let b_u = if m_bit == 0 {
            self.gpr[op2_reg]
        } else {
            op2_imm as u32
        };
        let a_i = a as i32;
        let b_i = b_u as i32;
        let epc = self.fetch_pc;

        // main dispatch
        match opcode {
            op::ADD => {
                self.wgpr(rd, a.wrapping_add(b_u));
            }
            op::SUB => {
                self.wgpr(rd, a.wrapping_sub(b_u));
            }
            op::MUL => {
                self.wgpr(rd, a.wrapping_mul(b_u));
            }
            op::DIV => {
                if b_u == 0 {
                    return Err(Exception::div_zero(epc));
                }
                self.wgpr(rd, a_i.wrapping_div(b_i) as u32);
            }
            op::MOD => {
                if b_u == 0 {
                    return Err(Exception::div_zero(epc));
                }
                self.wgpr(rd, a_i.wrapping_rem(b_i) as u32);
            }
            op::AND => {
                self.wgpr(rd, a & b_u);
            }
            op::OR => {
                self.wgpr(rd, a | b_u);
            }
            op::XOR => {
                self.wgpr(rd, a ^ b_u);
            }
            op::SHL => {
                self.wgpr(rd, a << (b_u & 0x1F));
            }
            op::SHR => {
                self.wgpr(rd, a >> (b_u & 0x1F));
            }
            op::SAR => {
                self.wgpr(rd, (a_i >> (b_u & 0x1F)) as u32);
            }

            // single register
            op::NOT => {
                self.wgpr(rd, !a);
            }
            op::INC => {
                self.wgpr(rd, a.wrapping_add(1));
            }
            op::DEC => {
                self.wgpr(rd, a.wrapping_sub(1));
            }
            op::NEG => {
                self.wgpr(rd, (-(a_i)) as u32);
            }

            // load/store
            op::LW => {
                let v = bus.read32(a.wrapping_add(op2_imm as u32), self.ring(), epc)?;
                self.wgpr(rd, v);
            }
            op::LH => {
                let v = bus.read16(a.wrapping_add(op2_imm as u32), self.ring(), epc)?;
                self.wgpr(rd, v as u32);
            }
            op::LB => {
                let v = bus.read8(a.wrapping_add(op2_imm as u32), self.ring(), epc)?;
                self.wgpr(rd, v as u32);
            }
            op::SW => {
                bus.write32(
                    a.wrapping_add(op2_imm as u32),
                    self.gpr[rd],
                    self.ring(),
                    epc,
                )?;
            }
            op::SH => {
                bus.write16(
                    a.wrapping_add(op2_imm as u32),
                    self.gpr[rd] as u16,
                    self.ring(),
                    epc,
                )?;
            }
            op::SB => {
                bus.write8(
                    a.wrapping_add(op2_imm as u32),
                    self.gpr[rd] as u8,
                    self.ring(),
                    epc,
                )?;
            }
            op::LUI => {
                self.wgpr(rd, bits(instr, 21, 0) << 10);
            }

            // floating point
            op::FADD => {
                let v = self.fpr[rs1] + self.fpr[op2_reg];
                self.wfpr(rd, v);
            }
            op::FSUB => {
                let v = self.fpr[rs1] - self.fpr[op2_reg];
                self.wfpr(rd, v);
            }
            op::FMUL => {
                let v = self.fpr[rs1] * self.fpr[op2_reg];
                self.wfpr(rd, v);
            }
            op::FDIV => {
                let v = self.fpr[rs1] / self.fpr[op2_reg];
                self.wfpr(rd, v);
            }
            op::FNEG => {
                let v = -self.fpr[rs1];
                self.wfpr(rd, v);
            }
            op::FABS => {
                let v = self.fpr[rs1].abs();
                self.wfpr(rd, v);
            }
            op::FSQRT => {
                let v = self.fpr[rs1].sqrt();
                self.wfpr(rd, v);
            }
            op::FCVT_FI => {
                let v = self.gpr[rs1] as i32 as f32;
                self.wfpr(rd, v);
            }
            op::FCVT_IF => {
                let v = self.fpr[rs1].trunc() as i32 as u32;
                self.wgpr(rd, v);
            }
            op::FLW => {
                let bits = bus.read32(a.wrapping_add(op2_imm as u32), self.ring(), epc)?;
                self.wfpr(rd, f32::from_bits(bits));
            }
            op::FSW => {
                bus.write32(
                    a.wrapping_add(op2_imm as u32),
                    self.fpr[rd].to_bits(),
                    self.ring(),
                    epc,
                )?;
            }
            op::FJEQ => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if self.fpr[bits(instr, 25, 22) as usize] == self.fpr[bits(instr, 21, 18) as usize]
                {
                    self.jump(off);
                }
            }
            op::FJLT => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if self.fpr[bits(instr, 25, 22) as usize] < self.fpr[bits(instr, 21, 18) as usize] {
                    self.jump(off);
                }
            }
            op::FJGT => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if self.fpr[bits(instr, 25, 22) as usize] > self.fpr[bits(instr, 21, 18) as usize] {
                    self.jump(off);
                }
            }

            // branches
            op::JEQ => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if self.gpr[rd] == self.gpr[rs1] {
                    self.jump(off);
                }
            }
            op::JNE => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if self.gpr[rd] != self.gpr[rs1] {
                    self.jump(off);
                }
            }
            op::JLT => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if (self.gpr[rd] as i32) < (self.gpr[rs1] as i32) {
                    self.jump(off);
                }
            }
            op::JGT => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if (self.gpr[rd] as i32) > (self.gpr[rs1] as i32) {
                    self.jump(off);
                }
            }
            op::JLE => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if (self.gpr[rd] as i32) <= (self.gpr[rs1] as i32) {
                    self.jump(off);
                }
            }
            op::JGE => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if (self.gpr[rd] as i32) >= (self.gpr[rs1] as i32) {
                    self.jump(off);
                }
            }
            op::JLTU => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if self.gpr[rd] < self.gpr[rs1] {
                    self.jump(off);
                }
            }
            op::JGTU => {
                let off = sign_extend(bits(instr, 17, 0), 18);
                if self.gpr[rd] > self.gpr[rs1] {
                    self.jump(off);
                }
            }

            // unconditional jump
            op::J => {
                let off = sign_extend(bits(instr, 21, 0), 22);
                let ret = self.pc;
                self.wgpr(rd, ret);
                self.pc = self.fetch_pc.wrapping_add(off as u32);
            }
            op::JR => {
                let ret = self.pc;
                self.wgpr(rd, ret);
                self.pc = a.wrapping_add(op2_imm as u32);
            }

            //csr
            op::CSR => {
                self.require_supervisor(epc)?;
                let csr_id = bits(instr, 16, 14) as usize;
                let dir_bit = bits(instr, 13, 13);
                if dir_bit == 0 {
                    self.wgpr(rd, self.csr[csr_id]);
                } else {
                    self.csr[csr_id] = self.gpr[rs1];
                }
            }

            // system
            op::SYSTEM => {
                if bits(instr, 13, 13) == 0 {
                    return Err(Exception::syscall(epc));
                } else {
                    self.require_supervisor(epc)?;
                    self.pc = self.csr[csr::EPC as usize];
                    self.set_ring(Ring::User);
                }
            }

            op::HALT => {
                self.require_supervisor(epc)?;
                return Ok(StepResult::Halted);
            }

            _ => {
                return Err(Exception::invalid(epc));
            }
        }

        Ok(StepResult::Ok)
    }

    // trap

    fn take_exception(&mut self, e: Exception, bus: &mut Bus) -> StepResult {
        if self.in_trap {
            eprintln!(
                "[ferrite] double fault: cause={} epc={:#010x} — halting",
                e.cause, e.epc
            );
            return StepResult::Halted;
        }
        self.in_trap = true;
        self.csr[csr::CAUSE as usize] = e.cause;
        self.csr[csr::EPC as usize] = e.epc;
        self.set_ring(Ring::Supervisor);

        let vec_addr = self.csr[csr::IVT as usize].wrapping_add(e.cause.wrapping_mul(4));
        match bus.fetch32(vec_addr, vec_addr) {
            Ok(addr) => {
                self.pc = addr;
                self.in_trap = false;
                StepResult::Ok
            }
            Err(_) => {
                eprintln!(
                    "[ferrite] fault reading IVT at {:#010x} for cause={} — halting",
                    vec_addr, e.cause
                );
                StepResult::Halted
            }
        }
    }

    // helpers
    #[inline]
    fn wgpr(&mut self, r: usize, v: u32) {
        if r != 0 {
            self.gpr[r] = v;
        }
    }

    #[inline]
    fn wfpr(&mut self, r: usize, v: f32) {
        self.fpr[r] = v;
    }

    #[inline]
    fn jump(&mut self, offset: i32) {
        self.pc = self.fetch_pc.wrapping_add(offset as u32);
    }

    #[inline]
    fn require_supervisor(&self, epc: u32) -> Result<(), Exception> {
        if self.ring().is_supervisor() {
            Ok(())
        } else {
            Err(Exception::fault_priv(epc))
        }
    }

    fn set_ring(&mut self, ring: Ring) {
        self.csr[csr::STATUS as usize] = (self.csr[csr::STATUS as usize] & !0b11) | (ring as u32);
    }
}

impl Default for Cpu {
    fn default() -> Self {
        Self::new()
    }
}
