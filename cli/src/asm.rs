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
