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

use crate::{error::AsmError, token::Token};

pub struct Lexer {
    src: Vec<char>,
    pos: usize,
    line: usize,
}

impl Lexer {
    pub fn new(src: &str) -> Self {
        Self {
            src: src.chars().collect(),
            pos: 0,
            line: 1,
        }
    }

    pub fn tokenize(mut self) -> Result<Vec<(Token, usize)>, AsmError> {
        let mut out = Vec::new();
        loop {
            self.skip_whitespace_inline();

            // check for end of file
            if self.pos >= self.src.len() {
                out.push((Token::Eof, self.line));
                break;
            }
            let ch = self.src[self.pos];
            match ch {
                // comments
                ';' | '#' => {
                    self.skip_line();
                }
                '/' => {
                    if self.peek_next() == Some('/') {
                        self.skip_line();
                    }
                }

                '\n' => {
                    out.push((Token::Newline, self.line));
                    self.line += 1;
                    self.pos += 1;
                }

                '\r' => {
                    self.pos += 1;
                } // windows moment

                ',' => {
                    out.push((Token::Comma, self.line));
                    self.pos += 1;
                }

                '"' => {
                    let (s, line) = self.read_string()?;
                    out.push((Token::Str(s), line));
                }

                '.' => {
                    self.pos += 1; //skip dot
                    let name = self.read_ident();
                    out.push((Token::Directive(name.to_ascii_lowercase()), self.line));
                }

                '0'..='9' | '-' => {
                    let (tok, line) = self.read_number()?;
                    out.push((tok, line));
                }

                'a'..='z' | 'A'..='Z' | '_' => {
                    let line = self.line;
                    let ident = self.read_ident();

                    // if next is colon this is a label
                    self.skip_whitespace_inline();
                    if self.pos < self.src.len() && self.src[self.pos] == ':' {
                        self.pos += 1;
                        out.push((Token::Ident(ident), line));
                        out.push((Token::Colon, line));
                    } else {
                        // register
                        if let Some(tok) = resolve_register(&ident) {
                            out.push((tok, line));
                        } else {
                            out.push((Token::Ident(ident), line));
                        }
                    }
                }

                other => {
                    return Err(AsmError::new(
                        self.line,
                        format!("unexpected character '{}'", other),
                    ));
                }
            }
        }
        Ok(out)
    }

    fn skip_whitespace_inline(&mut self) {
        while self.pos < self.src.len() && matches!(self.src[self.pos], ' ' | '\t') {
            self.pos += 1;
        }
    }

    fn skip_line(&mut self) {
        while self.pos < self.src.len() && self.src[self.pos] != '\n' {
            self.pos += 1;
        }
    }

    fn peek_next(&self) -> Option<char> {
        self.src.get(self.pos + 1).copied()
    }

    fn read_ident(&mut self) -> String {
        let start = self.pos;
        while self.pos < self.src.len()
            && (self.src[self.pos].is_alphanumeric()
                || self.src[self.pos] == '_'
                || self.src[self.pos] == '.')
        {
            self.pos += 1;
        }
        self.src[start..self.pos].iter().collect()
    }

    fn read_number(&mut self) -> Result<(Token, usize), AsmError> {
        let line = self.line;
        let negative = self.src[self.pos] == '-';
        if negative {
            self.pos += 1;
        }
        if self.pos >= self.src.len() || !self.src[self.pos].is_ascii_digit() {
            return Err(AsmError::new(line, "expected digit after '-'"));
        }

        if self.src[self.pos] == '0' && self.pos + 1 < self.src.len() {
            match self.src[self.pos + 1] {
                'x' | 'X' => {
                    self.pos += 2;
                    let s: String = self.src[self.pos..]
                        .iter()
                        .take_while(|c| c.is_ascii_hexdigit())
                        .collect();
                    self.pos += s.len();
                    let n = i64::from_str_radix(&s, 16)
                        .map_err(|_| AsmError::new(line, format!("invalid hex literal '{}'", s)))?;
                    return Ok((Token::Int(if negative { -n } else { n }), line));
                }
                'b' | 'B' => {
                    self.pos += 2;
                    let s: String = self.src[self.pos..]
                        .iter()
                        .take_while(|&&c| c == '0' || c == '1')
                        .collect();
                    self.pos += s.len();
                    let n = i64::from_str_radix(&s, 2).map_err(|_| {
                        AsmError::new(line, format!("invalid binary literal '{}'", s))
                    })?;
                    return Ok((Token::Int(if negative { -n } else { n }), line));
                }
                _ => {}
            }
        }

        // read integer part
        let int_part: String = self.src[self.pos..]
            .iter()
            .take_while(|c| c.is_ascii_digit())
            .collect();
        self.pos += int_part.len();

        // check for decimals
        if self.pos < self.src.len() && self.src[self.pos] == '.' {
            self.pos += 1; // consume .
            let frac_part: String = self.src[self.pos..]
                .iter()
                .take_while(|c| c.is_ascii_digit())
                .collect();
            self.pos += frac_part.len();

            // exponent
            let mut exp_part = String::new();
            if self.pos < self.src.len() && matches!(self.src[self.pos], 'e' | 'E') {
                exp_part.push('e');
                self.pos += 1;
                if self.pos < self.src.len() && matches!(self.src[self.pos], '+' | '-') {
                    exp_part.push(self.src[self.pos]);
                    self.pos += 1;
                }
                let digits: String = self.src[self.pos..]
                    .iter()
                    .take_while(|c| c.is_ascii_digit())
                    .collect();
                self.pos += digits.len();
                exp_part.push_str(&digits);
            }

            let s = format!("{}.{}{}", int_part, frac_part, exp_part);
            let f: f64 = s
                .parse()
                .map_err(|_| AsmError::new(line, format!("invalid float literal '{}'", s)))?;
            return Ok((Token::Float(if negative { -f } else { f }), line));
        }

        // int
        let n: i64 = int_part
            .parse()
            .map_err(|_| AsmError::new(line, format!("invalid integer '{}'", int_part)))?;
        Ok((Token::Int(if negative { -n } else { n }), line))
    }
    fn read_string(&mut self) -> Result<(String, usize), AsmError> {
        let line = self.line;
        self.pos += 1; // skip opening quote
        let mut s = String::new();
        loop {
            if self.pos >= self.src.len() {
                return Err(AsmError::new(line, "unterminated string literal"));
            }
            match self.src[self.pos] {
                '"' => {
                    self.pos += 1;
                    break;
                }
                '\\' => {
                    self.pos += 1;
                    let esc = match self.src.get(self.pos) {
                        Some('n') => '\n',
                        Some('t') => '\t',
                        Some('r') => '\r',
                        Some('0') => '\0',
                        Some('\\') => '\\',
                        Some('"') => '"',
                        other => {
                            return Err(AsmError::new(
                                line,
                                format!("unknown escape '\\{}'", other.unwrap_or(&'?')),
                            ));
                        }
                    };
                    s.push(esc);
                    self.pos += 1;
                }
                c => {
                    s.push(c);
                    self.pos += 1;
                }
            }
        }
        Ok((s, line))
    }
}
fn resolve_register(name: &str) -> Option<Token> {
    let lower = name.to_ascii_lowercase();

    // numbered
    if let Some(rest) = lower.strip_prefix('r') {
        if let Ok(n) = rest.parse::<u8>() {
            if n < 16 {
                return Some(Token::Reg(n));
            }
        }
    }
    if let Some(rest) = lower.strip_prefix('f') {
        if let Ok(n) = rest.parse::<u8>() {
            if n < 16 {
                return Some(Token::FReg(n));
            }
        }
    }

    // aliases
    let reg = match lower.as_str() {
        "zero" => 0,
        "ra" => 1,
        "sp" => 2,
        "fp" => 3,
        "rv" => 4,
        "a0" => 5,
        "a1" => 6,
        "a2" => 7,
        "a3" => 8,
        "a4" => 9,
        "t0" => 10,
        "t1" => 11,
        "t2" => 12,
        "t3" => 13,
        "s0" => 14,
        "s1" => 15,
        _ => return try_freg_alias(&lower),
    };
    Some(Token::Reg(reg))
}

fn try_freg_alias(lower: &str) -> Option<Token> {
    let n = match lower {
        "fa0" => 0,
        "fa1" => 1,
        "fa2" => 2,
        "fa3" => 3,
        "ft0" => 4,
        "ft1" => 5,
        "ft2" => 6,
        "ft3" => 7,
        "fs0" => 8,
        "fs1" => 9,
        "fs2" => 10,
        "fs3" => 11,
        _ => return None,
    };
    Some(Token::FReg(n))
}
