use ferrite_emu::{cpu::Cpu, cpu::StepResult, device::Device, device::Uart, mem::Bus};
use std::{env, fs, process};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <rom_file>", args[0]);
        process::exit(1);
    }

    let rom = fs::read(&args[1]).unwrap_or_else(|err| {
        eprintln!("Failed to read ROM file '{}': {}", args[1], err);
        process::exit(1);
    });

    let bus = &mut Bus::new(rom, vec![Box::new(Uart::new()) as Box<dyn Device>]);
    let cpu = &mut Cpu::new();

    loop {
        match cpu.step(bus) {
            StepResult::Ok => {}
            StepResult::Halted => {
                eprintln!("[ferrite] halted at pc={:#010x}", cpu.pc());
                break;
            }
        }
    }
}
