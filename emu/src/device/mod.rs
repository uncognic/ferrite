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
            print!("{}", (val as u8) as char);
            let _ = std::io::Write::flush(&mut std::io::stdout());
        }
    }

    fn tick(&mut self) -> Option<u32> {
        self.pending_irq.take()
    }
}
