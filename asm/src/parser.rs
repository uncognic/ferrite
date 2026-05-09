use crate::{
    ast::{Operand, Statement},
    error::AsmError,
    token::Token,
};

pub struct Parser {
    tokens: Vec<(Token, usize)>,
    pos: usize,
}

impl Parser {
    pub fn new(tokens: Vec<(Token, usize)>) -> Self {
        Self { tokens, pos: 0 }
    }

    pub fn parse(mut self) -> Result<Vec<Statement>, AsmError> {
        let mut statements = Vec::new();
        loop {
            while self.peek() == &Token::Newline {
                self.advance();
            }
            if self.peek() == &Token::Eof {
                break;
            }

            while let Token::Ident(_) = self.peek() {
                if self.peek_ahead(1) == &Token::Colon {
                    let name = self.expect_ident()?;
                    self.advance();
                    statements.push(Statement::Label(name));
                    while self.peek() == &Token::Newline {
                        self.advance();
                    }
                    if self.peek() == &Token::Eof {
                        return Ok(statements);
                    }
                } else {
                    break;
                }
            }

            match self.peek().clone() {
                Token::Newline | Token::Eof => continue,

                Token::Directive(name) => {
                    let line = self.current_line();
                    self.advance();
                    let args = self.parse_operand_list()?;
                    statements.push(Statement::Directive { line, name, args });
                    self.expect_eol()?;
                }

                Token::Ident(_) => {
                    let line = self.current_line();
                    let mnemonic = self.expect_ident()?.to_ascii_uppercase();
                    let operands = self.parse_operand_list()?;
                    statements.push(Statement::Instruction {
                        line,
                        mnemonic,
                        operands,
                    });
                    self.expect_eol()?;
                }

                other => {
                    return Err(AsmError::new(
                        self.current_line(),
                        format!("unexpected token {:?}", other),
                    ));
                }
            }
        }
        Ok(statements)
    }

    fn parse_operand_list(&mut self) -> Result<Vec<Operand>, AsmError> {
        let mut ops = Vec::new();
        // If next token starts an operand, parse it.
        while self.is_operand_start() {
            ops.push(self.parse_operand()?);
            if self.peek() == &Token::Comma {
                self.advance();
            } else {
                break;
            }
        }
        Ok(ops)
    }

    fn parse_operand(&mut self) -> Result<Operand, AsmError> {
        let line = self.current_line();
        match self.peek().clone() {
            Token::Reg(n) => {
                self.advance();
                Ok(Operand::Reg(n))
            }
            Token::FReg(n) => {
                self.advance();
                Ok(Operand::FReg(n))
            }
            Token::Int(n) => {
                self.advance();
                if n < i32::MIN as i64 || n > u32::MAX as i64 {
                    return Err(AsmError::new(
                        line,
                        format!("immediate {} out of 32-bit range", n),
                    ));
                }
                Ok(Operand::Imm(n as i32))
            }
            Token::Ident(s) => {
                self.advance();
                Ok(Operand::Name(s))
            }
            other => Err(AsmError::new(
                line,
                format!("expected operand, got {:?}", other),
            )),
        }
    }

    fn is_operand_start(&self) -> bool {
        matches!(
            self.peek(),
            Token::Reg(_) | Token::FReg(_) | Token::Int(_) | Token::Ident(_)
        )
    }

    fn peek(&self) -> &Token {
        &self.tokens[self.pos].0
    }

    fn peek_ahead(&self, offset: usize) -> &Token {
        let idx = (self.pos + offset).min(self.tokens.len() - 1);
        &self.tokens[idx].0
    }

    fn current_line(&self) -> usize {
        self.tokens[self.pos].1
    }

    fn advance(&mut self) -> &Token {
        let tok = &self.tokens[self.pos].0;
        if self.pos + 1 < self.tokens.len() {
            self.pos += 1;
        }
        tok
    }

    fn expect_ident(&mut self) -> Result<String, AsmError> {
        let line = self.current_line();
        match self.peek().clone() {
            Token::Ident(s) => {
                self.advance();
                Ok(s)
            }
            other => Err(AsmError::new(
                line,
                format!("expected identifier, got {:?}", other),
            )),
        }
    }

    fn expect_eol(&mut self) -> Result<(), AsmError> {
        match self.peek() {
            Token::Newline | Token::Eof => {
                self.advance();
                Ok(())
            }
            other => Err(AsmError::new(
                self.current_line(),
                format!("expected end of line, got {:?}", other),
            )),
        }
    }
}
