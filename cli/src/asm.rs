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

use std::{env, fs, path::Path, process};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: fasm [--nologo] <input.asm> [output.bin]");
        process::exit(1);
    }

    let nologo = args.contains(&"--nologo".to_string());
    if !nologo {
        eprintln!("--- ferrite assembler {} ---", env!("CARGO_PKG_VERSION"));
    }

    let input = &args[1];
    let output = if args.len() >= 3 {
        args[2].clone()
    } else {
        Path::new(input)
            .with_extension("bin")
            .to_string_lossy()
            .into_owned()
    };

    let source = fs::read_to_string(input).unwrap_or_else(|e| {
        eprintln!("error: could not read '{}': {}", input, e);
        process::exit(1);
    });

    let binary = ferrite_asm::assemble(&source).unwrap_or_else(|e| {
        eprintln!("{}:{}", input, e);
        process::exit(1);
    });

    fs::write(&output, &binary).unwrap_or_else(|e| {
        eprintln!("error: could not write '{}': {}", output, e);
        process::exit(1);
    });

    eprintln!("{} -> {} ({} bytes)", input, output, binary.len());
}
