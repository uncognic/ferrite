use crate::isa::cause;

/// CPU exception or hw interrupt
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Exception {
    pub cause: u32,
    /// PC of insturctioon
    pub epc: u32,
}

impl Exception {
    pub fn syscall(epc: u32) -> Self {
        Self {
            cause: cause::SYSCALL,
            epc,
        }
    }
    pub fn fault_mem(epc: u32) -> Self {
        Self {
            cause: cause::FAULT_MEM,
            epc,
        }
    }
    pub fn fault_priv(epc: u32) -> Self {
        Self {
            cause: cause::FAULT_PRIV,
            epc,
        }
    }
    pub fn div_zero(epc: u32) -> Self {
        Self {
            cause: cause::DIV_ZERO,
            epc,
        }
    }
    pub fn invalid(epc: u32) -> Self {
        Self {
            cause: cause::INVALID,
            epc,
        }
    }
}
