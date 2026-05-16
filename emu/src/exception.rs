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
