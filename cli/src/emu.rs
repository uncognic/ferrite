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

use ferrite_emu::{
    cpu::{Cpu, StepResult},
    device::{Device, Uart},
    mem::Bus,
};
use std::{env, fs, process};

fn main() {
    let args: Vec<String> = env::args().collect();

    let trace = args.contains(&"--trace".to_string());
    let regs = args.contains(&"--regs".to_string());
    let firmware = flag_value(&args, "--firmware");
    let program = flag_value(&args, "--program");

    // single positional argument loads the ROM as firmware
    let (rom, ram_image) = match (&firmware, &program) {
        (Some(fw), Some(prog)) => {
            let rom = load_file(fw);
            let ram = load_file(prog);
            (rom, Some(ram))
        }
        (Some(fw), None) => (load_file(fw), None),
        (None, None) => {
            // legacy
            let path = args.iter()
                .skip(1)
                .find(|a| !a.starts_with("--"))
                .unwrap_or_else(|| {
                    eprintln!("usage: ferrite [--firmware <fw.bin>] [--program <prog.bin>] [--trace] [--regs]");
                    process::exit(1);
                });
            (load_file(path), None)
        }
        (None, Some(_)) => {
            eprintln!("error: --program requires --firmware");
            process::exit(1);
        }
    };

    let bus = &mut Bus::new(
        rom,
        ram_image,
        vec![Box::new(Uart::new()) as Box<dyn Device>],
    );
    let cpu = &mut Cpu::new();

    loop {
        if trace {
            use std::io::Write;
            let _ = std::io::stdout().flush();
            print_trace(cpu);
        }
        match cpu.step(bus) {
            StepResult::Ok => {}
            StepResult::Halted => {
                use std::io::Write;
                let _ = std::io::stdout().flush();
                if trace || regs {
                    print_regs(cpu);
                }
                eprintln!("[ferrite] halted at pc={:#010x}", cpu.pc());
                break;
            }
        }
    }
}

fn flag_value<'a>(args: &'a [String], flag: &str) -> Option<&'a String> {
    args.windows(2).find(|w| w[0] == flag).map(|w| &w[1])
}

fn load_file(path: &str) -> Vec<u8> {
    fs::read(path).unwrap_or_else(|e| {
        eprintln!("error: could not read '{}': {}", path, e);
        process::exit(1);
    })
}

fn print_trace(cpu: &Cpu) {
    eprint!("pc={:#010x} ", cpu.pc());
    for (i, &val) in cpu.gpr().iter().enumerate() {
        if val != 0 {
            eprint!("R{i}={:#010x} ", val);
        }
    }
    eprintln!();
}

fn print_regs(cpu: &Cpu) {
    eprintln!("--- registers ---");
    for (i, &val) in cpu.gpr().iter().enumerate() {
        eprintln!("  R{i:<2} = {:#010x}  ({})", val, val as i32);
    }
    for (i, &val) in cpu.fpr().iter().enumerate() {
        if val != 0.0 {
            eprintln!("  F{i:<2} = {}", val);
        }
    }
}
