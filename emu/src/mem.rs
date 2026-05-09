use crate::{device::Device, exception::Exception, isa::mmap, privilege::Ring};

pub struct Bus {
    rom: Vec<u8>,
    ram: Vec<u8>,
    devices: Vec<Box<dyn Device>>,
}

impl Bus {
    pub fn new(rom: Vec<u8>, devices: Vec<Box<dyn Device>>) -> Self {
        let ram_size = (mmap::RAM_END - mmap::RAM_START + 1) as usize;
        Self {
            rom,
            ram: vec![0; ram_size],
            devices,
        }
    }

    pub fn tick_devices(&mut self) -> Option<u32> {
        for dev in &mut self.devices {
            if let Some(cause) = dev.tick() {
                return Some(cause);
            }
        }
        None
    }

    // read

    pub fn read8(&mut self, addr: u32, ring: Ring, epc: u32) -> Result<u8, Exception> {
        self.check_read(addr, ring, epc)?;
        Ok(self.raw_read8(addr))
    }

    pub fn read16(&mut self, addr: u32, ring: Ring, epc: u32) -> Result<u16, Exception> {
        if addr & 1 != 0 {
            return Err(Exception::fault_mem(epc));
        }
        self.check_read(addr, ring, epc)?;
        Ok(u16::from_le_bytes([
            self.raw_read8(addr),
            self.raw_read8(addr + 1),
        ]))
    }

    pub fn read32(&mut self, addr: u32, ring: Ring, epc: u32) -> Result<u32, Exception> {
        if addr & 3 != 0 {
            return Err(Exception::fault_mem(epc));
        }
        self.check_read(addr, ring, epc)?;
        Ok(u32::from_le_bytes([
            self.raw_read8(addr),
            self.raw_read8(addr + 1),
            self.raw_read8(addr + 2),
            self.raw_read8(addr + 3),
        ]))
    }

    // write
    pub fn write8(&mut self, addr: u32, val: u8, ring: Ring, epc: u32) -> Result<(), Exception> {
        self.check_write(addr, ring, epc)?;
        self.raw_write8(addr, val);
        Ok(())
    }

    pub fn write16(&mut self, addr: u32, val: u16, ring: Ring, epc: u32) -> Result<(), Exception> {
        if addr & 1 != 0 {
            return Err(Exception::fault_mem(epc));
        }
        self.check_write(addr, ring, epc)?;
        let b = val.to_le_bytes();
        self.raw_write8(addr, b[0]);
        self.raw_write8(addr + 1, b[1]);
        Ok(())
    }

    pub fn write32(&mut self, addr: u32, val: u32, ring: Ring, epc: u32) -> Result<(), Exception> {
        if addr & 3 != 0 {
            return Err(Exception::fault_mem(epc));
        }
        self.check_write(addr, ring, epc)?;
        let b = val.to_le_bytes();
        self.raw_write8(addr, b[0]);
        self.raw_write8(addr + 1, b[1]);
        self.raw_write8(addr + 2, b[2]);
        self.raw_write8(addr + 3, b[3]);
        Ok(())
    }

    // fetch
    pub fn fetch32(&mut self, addr: u32, epc: u32) -> Result<u32, Exception> {
        if addr & 3 != 0 {
            return Err(Exception::fault_mem(epc));
        }
        match region(addr) {
            Region::Rom | Region::Ram => Ok(u32::from_le_bytes([
                self.raw_read8(addr),
                self.raw_read8(addr + 1),
                self.raw_read8(addr + 2),
                self.raw_read8(addr + 3),
            ])),
            Region::Mmio => Err(Exception::fault_mem(epc)),
        }
    }

    // access checks

    fn check_read(&self, addr: u32, ring: Ring, epc: u32) -> Result<(), Exception> {
        match region(addr) {
            Region::Rom | Region::Ram => Ok(()),
            Region::Mmio => {
                if ring.is_supervisor() {
                    Ok(())
                } else {
                    Err(Exception::fault_priv(epc))
                }
            }
        }
    }

    fn check_write(&self, addr: u32, ring: Ring, epc: u32) -> Result<(), Exception> {
        match region(addr) {
            Region::Rom => Err(Exception::fault_mem(epc)),
            Region::Ram => Ok(()),
            Region::Mmio => {
                if ring.is_supervisor() {
                    Ok(())
                } else {
                    Err(Exception::fault_priv(epc))
                }
            }
        }
    }

    // raw access
    fn raw_read8(&mut self, addr: u32) -> u8 {
        match region(addr) {
            Region::Rom => self
                .rom
                .get((addr - mmap::ROM_BASE) as usize)
                .copied()
                .unwrap_or(0),
            Region::Ram => self
                .ram
                .get((addr - mmap::RAM_BASE) as usize)
                .copied()
                .unwrap_or(0),
            Region::Mmio => {
                let aligned = addr & !3;
                let off = (addr & 3) as usize;
                self.devices
                    .iter_mut()
                    .find(|d| d.contains(aligned))
                    .map(|d| d.read(aligned).to_le_bytes()[off])
                    .unwrap_or(0)
            }
        }
    }

    fn raw_write8(&mut self, addr: u32, val: u8) {
        match region(addr) {
            Region::Rom => {}
            Region::Ram => {
                let idx = (addr - mmap::RAM_BASE) as usize;
                if let Some(slot) = self.ram.get_mut(idx) {
                    *slot = val;
                }
            }
            Region::Mmio => {
                let aligned = addr & !3;
                let off = (addr & 3) as usize;
                if let Some(dev) = self.devices.iter_mut().find(|d| d.contains(aligned)) {
                    let mut word = dev.read(aligned).to_le_bytes();
                    word[off] = val;
                    dev.write(aligned, u32::from_le_bytes(word));
                }
            }
        }
    }
}

enum Region {
    Rom,
    Ram,
    Mmio,
}

fn region(addr: u32) -> Region {
    match addr {
        mmap::ROM_BASE..=mmap::ROM_END => Region::Rom,
        mmap::RAM_BASE..=mmap::RAM_END => Region::Ram,
        mmap::MMIO_BASE..=mmap::MMIO_END => Region::Mmio,
        _ => unreachable!(),
    }
}
