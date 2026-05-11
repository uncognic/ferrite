#[derive(Debug, Clone)]
pub enum Operand {
    Reg(u8),// R0–R15
    FReg(u8), // F0–F15
    Imm(i32), // imm
    Float(f32), // float literal
    Name(String), // resolved later
    Str(String), // for .string directive
}


#[derive(Debug, Clone)]
pub enum Statement {
    Label(String),
    Instruction {
        line: usize,
        mnemonic: String,
        operands: Vec<Operand>,
    },
    Directive {
        line: usize,
        name: String,
        args: Vec<Operand>,
    },
    Equ {
        name: String,
        value: i32,
    }
}