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

use crate::isa::cause::INT_UART_RX;
use crate::isa::mmap;

// device trait
pub trait Device: Send {
    fn base(&self) -> u32;
    fn size(&self) -> u32;
    fn contains(&self, addr: u32) -> bool {
        addr >= self.base() && addr < self.base() + self.size()
    }
    fn read(&mut self, addr: u32) -> u32;
    fn write(&mut self, addr: u32, val: u32);
    fn tick(&mut self) -> Option<u32> {
        None
    }
}
// UART
pub struct Uart {
    rx_buf: std::collections::VecDeque<u8>,
    pending_irq: Option<u32>,
}

impl Uart {
    pub fn new() -> Self {
        Self {
            rx_buf: std::collections::VecDeque::new(),
            pending_irq: None,
        }
    }

    pub fn push_rx(&mut self, byte: u8) {
        self.rx_buf.push_back(byte);
        self.pending_irq = Some(INT_UART_RX);
    }
}

impl Default for Uart {
    fn default() -> Self {
        Self::new()
    }
}

impl Device for Uart {
    fn base(&self) -> u32 {
        mmap::UART_TX
    }
    fn size(&self) -> u32 {
        return 12;
    }
    fn read(&mut self, addr: u32) -> u32 {
        match addr {
            mmap::UART_RX => self.rx_buf.pop_front().unwrap_or(0xFF) as u32,
            mmap::UART_STATUS => {
                let rx_ready = u32::from(!self.rx_buf.is_empty());
                rx_ready | (1 << 1) // tx always ready
            }
            _ => 0,
        }
    }

    fn write(&mut self, addr: u32, val: u32) {
        if addr == mmap::UART_TX {
            use std::io::Write;
            let _ = std::io::stdout().write_all(&[val as u8]);
            let _ = std::io::stdout().flush();
        }
    }

    fn tick(&mut self) -> Option<u32> {
        self.pending_irq.take()
    }
}
