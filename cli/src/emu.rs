use ferrite_emu::{
    cpu::{Cpu, StepResult},
    device::{Device, Uart},
    mem::Bus,
};
use std::{env, fs, process};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: ferrite <rom.bin>");
        process::exit(1);
    }

    let rom = fs::read(&args[1]).unwrap_or_else(|e| {
        eprintln!("error: could not read '{}': {}", args[1], e);
        process::exit(1);
    });

    let bus = &mut Bus::new(rom, vec![Box::new(Uart::new()) as Box<dyn Device>]);
    let cpu = &mut Cpu::new();

    loop {
        match cpu.step(bus) {
            StepResult::Ok => {}
            StepResult::Halted => {
                eprintln!("\n[ferrite] halted at pc={:#010x}", cpu.pc());
                break;
            }
        }
    }
}
