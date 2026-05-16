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

#[derive(Debug, Clone)]
pub enum Operand {
    Reg(u8),      // R0–R15
    FReg(u8),     // F0–F15
    Imm(i32),     // imm
    Float(f32),   // float literal
    Name(String), // resolved later
    Str(String),  // for .string directive
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
    },
}
