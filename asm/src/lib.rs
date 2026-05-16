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
