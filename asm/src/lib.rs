mod ast;
mod encoder;
mod error;
mod lexer;
mod parser;
mod token;

pub use error::AsmError;

/// assemble ferrite source into a binary starting at address 0x0
pub fn assemble(source: &str) -> Result<Vec<u8>, AsmError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let stmts = parser::Parser::new(tokens).parse()?;
    let bytes = encoder::Encoder::new(stmts).encode()?;
    Ok(bytes)
}
