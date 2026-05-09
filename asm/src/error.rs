use std::fmt;

#[derive(Debug, Clone)]
pub struct AsmError {
    pub line: usize,
    pub msg: String,
}

impl AsmError {
    pub fn new(line: usize, msg: impl Into<String>) -> Self {
        Self {
            line,
            msg: msg.into(),
        }
    }
}

impl fmt::Display for AsmError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "line {}: {}", self.line, self.msg)
    }
}

impl std::error::Error for AsmError {}