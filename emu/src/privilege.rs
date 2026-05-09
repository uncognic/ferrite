#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Ring {
    Supervisor = 0,
    User = 1,
}

impl Ring {
    pub fn from_u32(val: u32) -> Self {
        match val & 0b11 {
            0 => Ring::Supervisor,
            _ => Ring::User,
        }
    }

    pub fn is_supervisor(self) -> bool {
        self == Ring::Supervisor
    }
}
