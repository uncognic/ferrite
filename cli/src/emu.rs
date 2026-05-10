use ferrite_emu::{
    cpu::{Cpu, StepResult},
    device::{Device, Uart},
    mem::Bus,
};
use std::{env, fs, process};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: ferrite [--trace] <rom.bin>");
        process::exit(1);
    }

    let trace = args.contains(&"--trace".to_string());
    let path = args
        .iter()
        .find(|a| !a.starts_with("--") && *a != &args[0])
        .unwrap_or_else(|| {
            eprintln!("usage: ferrite [--trace] <rom.bin>");
            process::exit(1);
        });

    let rom = fs::read(path).unwrap_or_else(|e| {
        eprintln!("error: could not read '{}': {}", path, e);
        process::exit(1);
    });

    let bus = &mut Bus::new(rom, vec![Box::new(Uart::new()) as Box<dyn Device>]);
    let cpu = &mut Cpu::new();

    loop {
        if trace {
            print_trace(cpu);
        }
        match cpu.step(bus) {
            StepResult::Ok => {}
            StepResult::Halted => {
                if trace {
                    print_trace(cpu);
                }
                eprintln!("\n[ferrite] halted at pc={:#010x}", cpu.pc());
                break;
            }
        }
    }
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
