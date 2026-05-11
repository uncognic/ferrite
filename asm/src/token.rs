#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    Ident(String), // labels and stuff
    Reg(u8),       // R0-R15
    FReg(u8),      // F0–F15
    Int(i64),      // numeric literal
    Float(f64),    // floating-point literal
    Str(String),   // string literal for .string directive
    Comma,         // comma
    Colon,
    Directive(String), // .word, .byte, .org
    Newline,
    Eof,
}
