# Ferrite ISA manual
- 32-bit word size
- 32-bit instruction width
- Little Endian

--- 

## Table of Contents
1. [Overview](#1-overview)
2. [Registers](#2-registers)
3. [Memory Model](#3-memory-model)
4. [Instruction Encoding](#4-instruction-encoding)
5. [Instruction Reference](#5-instruction-reference)
   - [Integer Arithmetic](#51-integer-arithmetic)
   - [Single-Register Operations](#52-single-register-operations)
   - [Load and Store](#53-load-and-store)
   - [Floating-Point](#54-floating-point)
   - [Branches](#55-branches)
   - [Jumps](#56-jumps)
   - [System](#57-system)
   - [CSR Access](#58-csr-access)
6. [Pseudoinstructions](#6-pseudoinstructions)
7. [Calling Convention](#7-calling-convention)
8. [Privilege Model](#8-privilege-model)
9. [Exceptions and Interrupts](#9-exceptions-and-interrupts)
10. [Memory Map](#10-memory-map)
11. [MMIO Devices](#11-mmio-devices)
12. [Assembly Directives](#12-assembly-directives)

---

## 1. Overview
Ferrite is a simple 32-bit RISC-style architecture designed for educational purposes. 

**Stuff:**
- No flags register: Instead of a separate compare instruction that sets hidden flags, branch instructions embed their own comparison. `JEQ R1, R2, label` compares and jumps in one step.
- Privilege rings: Ring 0, the supervisor and Ring 1, the user.

--- 

## 2. Registers
 
### General-Purpose Registers
 
Ferrite has 16 general-purpose 32-bit integer registers, `R0` through `R15`. They can all be used freely in arithmetic and memory instructions
 
| Register | Alias  | Role |
|----------|--------|------|
| R0       | `zero` | Hardwired zero, like /dev/null |
| R1       | `ra`   | Return address. Set by `CALL`, read by `RET`. |
| R2       | `sp`   | Stack pointer. Points to the last used word on the stack. Grows downward. |
| R3       | `fp`   | Frame pointer. Marks the base of the current stack frame. |
| R4       | `rv`   | Integer return value. |
| R5       | `a0`   | Argument 0. |
| R6       | `a1`   | Argument 1. |
| R7       | `a2`   | Argument 2. |
| R8       | `a3`   | Argument 3. |
| R9       | `a4`   | Argument 4. |
| R10      | `t0`   | Temporary. Caller-saved. |
| R11      | `t1`   | Temporary. Caller-saved. |
| R12      | `t2`   | Temporary. Caller-saved. |
| R13      | `t3`   | Temporary. Caller-saved. |
| R14      | `s0`   | Saved register. Callee-saved. |
| R15      | `s1`   | Saved register. Callee-saved. |

> **R0 is always zero.** This gives several useful pseudoinstructions for free: `MOV`, `CLR`, `NEG`, `JZ`, `JNZ`

### Floating-Point Registers
 
Ferrite has 16 general-purpose 32-bit floating-point registers, `F0` through `F15`. They hold IEEE 754 single-precision values.
 
| Register | Alias  | Role |
|----------|--------|------|
| F0       | `fa0`  | FP argument 0 / return value. |
| F1       | `fa1`  | FP argument 1. |
| F2       | `fa2`  | FP argument 2. |
| F3       | `fa3`  | FP argument 3. |
| F4       | `ft0`  | FP temporary. Caller-saved. |
| F5       | `ft1`  | FP temporary. Caller-saved. |
| F6       | `ft2`  | FP temporary. Caller-saved. |
| F7       | `ft3`  | FP temporary. Caller-saved. |
| F8       | `fs0`  | FP saved. Callee-saved. |
| F9       | `fs1`  | FP saved. Callee-saved. |
| F10      | `fs2`  | FP saved. Callee-saved. |
| F11      | `fs3`  | FP saved. Callee-saved. |
| F12–F15  | -      | General floating-point use. |
 

### Control and Status Registers (CSRs)
 
CSRs are accessed with `CSRR` and `CSRW`. They are only accessible from Ring 0. Attempting to read or write a CSR from Ring 1 raises `EXC_FAULT_PRIV`.
 
| ID | Name         | Description |
|----|--------------|-------------|
| 0  | `STATUS`     | Current privilege ring in bits [1:0]. 0 = Ring 0, 1 = Ring 1. |
| 1  | `IVT`        | Base address of the interrupt vector table. |
| 2  | `CAUSE`      | Exception cause code. Written by the CPU on every trap. |
| 3  | `EPC`        | Exception PC. The address of the instruction that caused the trap. |
| 4  | `ESAVE`      | Scratch register for use by the trap handler. |

---

## 3. Memory Model
The address space is 32 bits, allowing for a maximum of 4 GiB of memory. It is byte-addressed and little-endian. A 32-bit word stored at address `A` has its least significant byte at `A`, and its most significant byte at `A+3`.

### Alignment
 
| Access size | Required alignment |
|-------------|--------------------|
| Byte (`LB`/`SB`) | None |
| Halfword (`LH`/`SH`) | 2-byte boundary |
| Word (`LW`/`SW`) | 4-byte boundary |
 
Misaligned word or halfword accesses raise `EXC_FAULT_MEM`.

### Memory Map
 
| Region | Range | Description |
|--------|-------|-------------|
| ROM | `0x00000000`–`0x0000FFFF` | Boot firmware, 64 KiB, Read-only, Ring 0 only. |
| RAM | `0x00010000`–`0x7FFFFFFF` | General use, 2 GiB, Read-write, Ring 0 and 1. |
| MMIO | `0x80000000`–`0xFFFFFFFF` | Memory-mapped I/O, Ring 0 only. |
 
The CPU begins execution at address `0x00000000`. The stack by convention starts at the top of RAM (`0x7FFFFFFF`) and grows downward.
 
Writing to ROM raises `EXC_FAULT_MEM`. Accessing MMIO from Ring 1 raises `EXC_FAULT_PRIV`.'
---
## 4. Instruction Encoding
 
Every instruction is exactly 32 bits. The top 6 bits are always the opcode.

### "M-bit"
For arithmetic and load/store instructions, bit 17 is the M-bit. It controls whether the second source operand is a register or an immediate.
 
- M = 0 -> R-format: the second source is a register in bits [16:13].
- M = 1 -> I-format: the second source is a 17-bit signed immediate in bits [16:0].
This is why `ADD R1, R2, R3` and `ADD R1, R2, 42` share the same opcode: the M-bit distinguishes them. The assembler sets it automatically.
 
### Encoding Formats
- R-format: two source regs
- I-format: one source reg, one immediate, m bit set
- S format: store (no dest reg), the destination reg is actually a source
- B format: conditional branch
- J format: unconditional jump
- U format: upper immediate (load into upper bits of register)

### Immediates
All immediates are sign-extended to 32 bits at decode time

Branch and jump offsets are PC-relative and measured in bytes from the address of the branch instruction itself.

---

## 5. Instruction Reference
### 5.1 Integer Arithmetic
All arithmetic instructions support unified R/I operand syntax. The assembler selects R-format or I-format based on whether the last operand is a register or a number.
 
---
 
#### ADD
 
```
ADD rd, rs1, rs2
ADD rd, rs1, imm
```
 
```asm
ADD R3, R1, R2        ; R3 = R1 + R2
ADD R3, R1, 10        ; R3 = R1 + 10
ADD R1, R1, -1        ; R1 = R1 - 1  (equivalent to DEC R1)
```

#### SUB
 
```
SUB rd, rs1, rs2
SUB rd, rs1, imm
```
```asm
SUB R3, R1, R2        ; R3 = R1 - R2
SUB R3, R1, 4         ; R3 = R1 - 4
```

#### MUL
 
```
MUL rd, rs1, rs2
MUL rd, rs1, imm
```
 
```asm
MUL R1, R2, R3        ; R1 = R2 * R3
MUL R1, R1, 4         ; R1 = R1 * 4
```
 
---
 
#### DIV
 
```
DIV rd, rs1, rs2
DIV rd, rs1, imm
```

```asm
DIV R1, R4, 2         ; R1 = R4 / 2
```
---
#### MOD
 
```
MOD rd, rs1, rs2
MOD rd, rs1, imm
```
```asm
MOD R1, R2, 8         ; R1 = R2 % 8
```
---
#### AND (bitwise)
 
```
AND rd, rs1, rs2
AND rd, rs1, imm
```

```asm
AND R1, R2, 0xFF      ; R1 = low byte of R2
AND R1, R1, R2        ; R1 = R1 & R2
```
---
#### OR (bitwise)
 
```
OR rd, rs1, rs2
OR rd, rs1, imm
```
 
```asm
OR R1, R1, 0x80000000 ; set bit 31
```
 
---
 
#### XOR (bitwise)
 
```
XOR rd, rs1, rs2
XOR rd, rs1, imm
```
 
```asm
XOR R1, R1, R1        ; R1 = 0  (clear register)
XOR R1, R1, 0xFF      ; flip low 8 bits
```
 
---
 
#### SHL (shift left logical)
 
```
SHL rd, rs1, rs2
SHL rd, rs1, imm
```
 
```asm
SHL R1, R1, 2         ; R1 = R1 * 4
```
 
---
 
#### SHR (shift right logical)
 
```
SHR rd, rs1, rs2
SHR rd, rs1, imm
```
 
```asm
SHR R1, R1, 1         ; R1 = R1 / 2  (unsigned)
```
 
---
 
#### SAR (shift right arithmetic)
 
```
SAR rd, rs1, rs2
SAR rd, rs1, imm
```
 
```asm
SAR R1, R1, 1         ; R1 = R1 / 2  (signed)
```
 
---
 
### 5.2 Single-Register Operations
 
---
 
#### NOT (bitwise NOT)
 
```
NOT rd, rs1
```
 
```asm
NOT R1, R2            ; R1 = bitwise inverse of R2
```
 
---
 
#### INC
 
```
INC rd
```
 
```asm
INC R1                ; R1 = R1 + 1
```
 
---
 
#### DEC
 
```
DEC rd
```
 
```asm
DEC R1                ; R1 = R1 - 1
```
 
---
 
#### NEG
 
```
NEG rd
```
 
```asm
NEG R1                ; R1 = -R1
```
 
---

### 5.3 Load and Store
Memory addresses are computed as `base + offset` where base is a register and offset is a signed 17-bit immediate

---
 
#### LW (load word)
 
```
LW rd, rs1, imm
```
 
```asm
LW R1, sp, 0          ; load word at top of stack
LW R2, R3, 8          ; load word 8 bytes past R3
```
 
---

#### LH - load halfword
 
```
LH rd, rs1, imm
```
 
```asm
LH R1, R2, 0          ; load 16-bit value at R2
```
 
---
#### LB - load byte
 
```
LB rd, rs1, imm
```
 
```asm
LB R1, R2, 3          ; load byte at R2+3
```
 
---
#### SW - store word
 
```
SW rs2, rs1, imm
```
 
```asm
SW R1, sp, 0          ; store R1 to top of stack
SW R4, R3, 8          ; store R4 to R3+8
```
 
---
#### SH - store halfword
 
```
SH rs2, rs1, imm
```
 
`mem16[rs1 + imm] = rs2[15:0]`. Stores the low 16 bits. Address must be 2-byte aligned.
 
---
 
#### SB - store byte
 
```
SB rs2, rs1, imm
```
 
`mem8[rs1 + imm] = rs2[7:0]`. Stores the low 8 bits. No alignment requirement.
 
---
 
#### LUI - load upper immediate
 
```
LUI rd, imm22
```
 
`rd = imm22 << 10`. Loads a 22-bit value into the upper portion of a register, zeroing the lower 10 bits. Used as the first step of the `LI` pseudoinstruction to load 32-bit constants.
 
```asm
LUI R1, 0x200         ; R1 = 0x00080000
```
 
---
### 5.4 Floating-Point
All floating-point operations follow IEEE 754 single-precision rules
#### FADD, FSUB, FMUL, FDIV
 
```
FADD fd, fs1, fs2
FSUB fd, fs1, fs2
FMUL fd, fs1, fs2
FDIV fd, fs1, fs2
```
 
```asm
FADD F0, F1, F2       ; F0 = F1 + F2
FDIV F0, F0, F1       ; F0 = F0 / F1
```
 
---
 
#### FNEG - floating negation
 
```
FNEG fd, fs1
```
 
`fd = −fs1`. Flips the sign bit.
 
---
 
#### FABS - floating absolute value
 
```
FABS fd, fs1
```
 

---
 
#### FSQRT - floating square root
 
```
FSQRT fd, fs1
```
 
---
 
#### FCVT.FI - convert integer to float
 
```
FCVT.FI fd, rs1
```
 
```asm
MOV  R1, 42
FCVT.FI F0, R1        ; F0 = 42.0
```
 
---
 
#### FCVT.IF - convert float to integer
 
```
FCVT.IF rd, fs1
```
 
```asm
FCVT.IF R1, F0        ; R1 = (int)F0
```
 
---
 
#### FLW - load float
 
```
FLW fd, rs1, imm
```
 
---
 
#### FSW - store float
 
```
FSW fs2, rs1, imm
```
 
---
 
#### FJEQ, FJLT, FJGT - floating point branch
 
```
FJEQ fs1, fs2, label
FJLT fs1, fs2, label
FJGT fs1, fs2, label
```
 
```asm
FJLT F0, F1, less     ; jump to 'less' if F0 < F1
```
 
---

### 5.5 Branches
Integer branch instructions compare two registers and jump if the condition is true. The target is a label or a PC-relative byte offset.
 
There is no separate compare instruction. The comparison is embedded in the branch.
 
---

#### JEQ - jump if equal
 
```
JEQ rs1, rs2, label
```
 
```asm
JEQ R1, R2, match     ; jump to 'match' if R1 == R2
JEQ R1, R0, zero      ; jump if R1 == 0  (or use JZ pseudoinstruction)
```
 
---


#### JNE - jump if not equal
 
```
JNE rs1, rs2, label
```
 
Jump if `rs1 != rs2`.
 
---
 
#### JLT - jump if less than signed
 
```
JLT rs1, rs2, label
```
 
Jump if `rs1 < rs2` as signed 32-bit integers.
 
---
 
#### JGT - jump if greater than signed
 
```
JGT rs1, rs2, label
```
 
Jump if `rs1 > rs2` as signed 32-bit integers.
 
---
 
#### JLE - jump if less than or equal signed
 
```
JLE rs1, rs2, label
```
 
Jump if `rs1 <= rs2` as signed 32-bit integers.
 
---
 
#### JGE - jump if greater than or equal signed
 
```
JGE rs1, rs2, label
```
 
Jump if `rs1 >= rs2` as signed 32-bit integers.
 
---
 
#### JLTU - jump if less than unsigned
 
```
JLTU rs1, rs2, label
```
 
Jump if `rs1 < rs2` as unsigned 32-bit integers.
 
---
 
#### JGTU - jump if greater than unsigned
 
```
JGTU rs1, rs2, label
```
 
Jump if `rs1 > rs2` as unsigned 32-bit integers.
 
---

### 5.6 Jumps 
---
#### J - jump
```
J rd, label
J label         (pseudoinstruction: J R0, label)
```
 
`rd = PC + 4, PC = PC + offset`. Unconditional jump. Saves the return address in `rd`. If `rd` is `R0` the return address is discarded.
 
```asm
J my_function         ; jump, no return address saved  (uses R0)
J R1, my_function     ; jump, return address in R1
```
 
---

#### JR - jump register
```
JR rd, rs1, imm
RET             (pseudoinstruction: JR R0, R1, 0)
```
 
`rd = PC + 4,  PC = rs1 + imm`. Jump to an address held in a register plus an offset. Used for function returns and computed jumps.
 
```asm
JR R0, R1, 0          ; jump to address in R1 (return)
JR R0, R3, 0          ; jump to function pointer in R3
```
 
---

### 5.7 System
 
---

#### SYSCALL
```
SYSCALL
```
 
Triggers a software trap from Ring 1 to Ring 0. The CPU saves the current PC to `CSR_EPC`, sets `CSR_CAUSE` to `0` (`EXC_SYSCALL`), switches to Ring 0, and jumps to the address stored in `IVT[0]`. This is meant for if you write an operating system for Ferrite.
 
By convention, the system call number is placed in `R4` (`rv`) before `SYSCALL`, and arguments follow in `a0`–`a4`.
 
```asm
MOV R4, 1            ; syscall number 1 (write, by UNIX convention)
MOV a0, R5           ; first argument
SYSCALL
```

---

#### SYSRET
 
```
SYSRET
```
 
Returns from a Ring 0 trap handler to Ring 1. Restores `PC` from `CSR_EPC` and switches back to Ring 1. Only valid from Ring 0.
 
---
 
#### HALT
 
```
HALT
```
 
Stops the CPU. Only valid from Ring 0. Raises `EXC_FAULT_PRIV` if executed from Ring 1.

---
 
### 5.8 CSR Access
 
CSR instructions are only valid from Ring 0.
 
---

#### CSRR - Read CSR
 
```
CSRR rd, CSR_NAME
```
 
`rd = csr`. Reads the named control register into `rd`.
 
```asm
CSRR R1, STATUS       ; R1 = CSR_STATUS
CSRR R1, EPC          ; R1 = saved exception PC
```
 
---

#### CSRW - write csr
 
```
CSRW CSR_NAME, rs1
```
 
`csr = rs1`. Writes `rs1` into the named control register.
 
```asm
CSRW IVT, R1          ; set interrupt vector table base
CSRW EPC, R2          ; overwrite saved PC (e.g. to skip faulting instruction)
```

---

## 6. Pseudoinstructions
 
Pseudoinstructions are expanded by the assembler. They produce real instructions but let you write cleaner code.
 
| psuedoinstr | expands to | effect |
|-------------------|------------|--------|
| `NOP` | `ADD R0, R0, R0` | No operation. |
| `MOV rd, rs` | `ADD rd, R0, rs` | `rd = rs` |
| `MOV rd, imm` | `ADD rd, R0, imm` | `rd = imm` (small immediates only) |
| `LI rd, imm32` | `LUI rd, upper` + `ADD rd, rd, lower` | Load any 32-bit constant. |
| `CLR rd` | `ADD rd, R0, R0` | `rd = 0` |
| `NEG rd, rs` | `SUB rd, R0, rs` | `rd = -rs` |
| `JZ rs, label` | `JEQ rs, R0, label` | Jump if `rs == 0`. |
| `JNZ rs, label` | `JNE rs, R0, label` | Jump if `rs != 0`. |
| `J label` | `J R0, label` | Unconditional jump, no return address. |
| `CALL label` | `J ra, label` | Call function at label. Return address in `ra`. |
| `RET` | `JR R0, ra, 0` | Return to address in `ra`. |

### LI and large immediates
 
Any 32-bit immediate can be loaded with `LI`. The assembler splits it into two instructions:
```asm
LI R1, 0x80000000
; expands to:
;   LUI R1, 0x200000    ; upper 22 bits shifted left by 10
;   ADD R1, R1, 0       ; lower 10 bits (zero in this case)
```
For values that fit in 17 bits (−65536 to 65535), `MOV rd, imm` is sufficient and produces a single instruction.

## 7. Calling Convention
This convention is enforced by software, firmware, the OS, and compilers. The hardware has no knowledge of it.

### Argument Passing
Integer arguments are passed in `a0` to `a4` (R5–R9), left to right. If there are more than five arguments, the extras are pushed onto the stack right to left before the call.
 
Floating-point arguments are passed in `fa0` to`fa3` (F0–F3).

### Return Values
A single integer return value goes in `rv` (R4). A single float return value goes in `fa0` (F0). If a function returns a struct or multiple values, the caller passes a pointer in `a0` and the callee writes the result there.

### Register Preservation
 
| Registers | Responsibility |
|-----------|----------------|
| `ra`, `sp`, `fp` | Callee-saved. |
| `s0`–`s1` (R14–R15) | Callee-saved. Must be restored before returning. |
| `fs0`–`fs3` (F8–F11) | Callee-saved. |
| `a0`–`a4`, `rv` | Caller-saved. May be clobbered by the callee. |
| `t0`–`t3` (R10–R13) | Caller-saved. May be clobbered by the callee. |
| `ft0`–`ft3` (F4–F7) | Caller-saved. May be clobbered by the callee. |

### Stack Frame Layout
The stack grows downward. `sp` always points to the last valid word. A typical function prologue and epilogue:
```asm
my_func:
    ; save ra and fp, set up frame
    SUB  sp, sp, 8
    SW   ra, sp, 4
    SW   fp, sp, 0
    MOV  fp, sp
 
    ; body
 
    ; restore and return
    MOV  sp, fp
    LW   fp, sp, 0
    LW   ra, sp, 4
    ADD  sp, sp, 8
    RET
```
 
## 8. Privilege Model
Ferrite has two privilege rings. The current ring is stored in `CSR_STATUS[1:0]`.
- Ring 0: supervisor. Can access all instructions and memory regions. Used for OS kernels and firmware.
- Ring 1: user. RAM access only, no CSR no MMIO

### Ring Transitions
The only way into Ring 0 from Ring 1 is `SYSCALL`. The only way back to Ring 1 is `SYSRET`. There is no other mechanism. Hardware interrupts also enter Ring 0.
 
### Privilege Violations
Any instruction that requires Ring 0 when executed from Ring 1 immediately raises `EXC_FAULT_PRIV`. The trap handler runs in Ring 0 and may terminate the offending process.
 
--- 
## 9. Exceptions and Interrupts
 
Exceptions and interrupts use the same mechanism. When any of them occur, the CPU:
 
1. Saves the faulting `PC` into `CSR_EPC`.
2. Writes the cause code into `CSR_CAUSE`.
3. Switches to Ring 0.
4. Reads the handler address from the IVT: `mem32[CSR_IVT + cause × 4]`.
5. Jumps to that address.

### Exception Cause Codes
 
| Code | Name | Cause |
|------|------|-------|
| 0 | `EXC_SYSCALL` | `SYSCALL` instruction executed. |
| 1 | `EXC_FAULT_MEM` | Invalid memory access, unmapped address, or misaligned access. |
| 2 | `EXC_FAULT_PRIV` | Privileged instruction executed from Ring 1. |
| 3 | `EXC_DIV_ZERO` | `DIV` or `MOD` with a zero divisor. |
| 4 | `EXC_INVALID` | Undefined opcode. |
| 5–15 | (reserved) | |
| 16 | `INT_UART_RX` | UART received a byte. |
| 17+ | (reserved) | |

### Interrupt Vector Table
 
The IVT is an array of 32-bit handler addresses in RAM. Entry `N` holds the address of the handler for cause `N`. Firmware sets it up at boot:
 
```asm
    ; Set up IVT at address 0x10000
    LI   R1, 0x00010000
    CSRW IVT, R1
 
    ; Write handler addresses
    LI   R2, syscall_handler
    SW   R2, R1, 0            ; cause 0: SYSCALL
 
    LI   R2, mem_fault_handler
    SW   R2, R1, 4            ; cause 1: EXC_FAULT_MEM
 
    ; etc
```
 
`CSR_IVT` defaults to `0x00000000` at reset. Any unhandled early exception will jump back to the reset vector at address 0, which is a visible failure rather than silent corruption.

### Returning from a Handler
 
```asm
my_handler:
    ; ... handle the exception ...
    SYSRET                    ; restore PC from EPC, return to Ring 1
```

For exceptions where the faulting instruction should be skipped, increment `CSR_EPC` by 4 before returning:
 
```asm
    CSRR R1, EPC
    ADD  R1, R1, 4
    CSRW EPC, R1
    SYSRET
```
## 10. Memory Map
- ROM (64kb firmware, read only): 0x00000000 - 0x00010000
- RAM (2gb, general use, heap grows up, stack grows down): 0x00010000 - 0x7FFFFFFF
- MMIO (ring 0 only): 0x80000000 - 0xFFFFFFFF

---
 
## 11. MMIO Devices
 
MMIO registers are accessed with regular `LW`/`SW` instructions. All MMIO is Ring 0 only.

### UART
 
The UART provides a simple serial interface.
 
| Address | Name | Access | Description |
|---------|------|--------|-------------|
| `0x80000000` | `UART_TX` | Write | Write a byte to transmit. Only the low 8 bits are used. |
| `0x80000004` | `UART_RX` | Read | Read the next received byte. Returns `0xFF` if no data is available. |
| `0x80000008` | `UART_STATUS` | Read | Bit 0: RX ready (a byte is waiting). Bit 1: TX ready (safe to write). |

#### Transmitting a byte:
 
```asm
    LI   R1, 0x80000000   ; UART_TX
    MOV  R2, 65           ; 'A'
    SW   R2, R1, 0
```

#### Receiving a byte (polling):
 
```asm
    LI   R1, 0x80000000   ; UART base
wait:
    LW   R2, R1, 8        ; read UART_STATUS
    AND  R2, R2, 1        ; check RX ready bit
    JZ   R2, wait         ; loop until ready
    LW   R3, R1, 4        ; read UART_RX
```

## 12. Assembly Directives
 
Directives are assembler commands that do not emit instructions directly.
 
| Directive | Arguments | Description |
|-----------|-----------|-------------|
| `.word` | `value, ...` | Emit one or more 32-bit words. |
| `.half` | `value, ...` | Emit one or more 16-bit halfwords. |
| `.byte` | `value, ...` | Emit one or more bytes. |
| `.string` | `"text"` | Emit a null-terminated string, padded to a word boundary. |
| `.org` | `address` | Set the current address. |
| `.align` | `n` | Advance to the next `n`-byte boundary. |
| `.entry` | `label` | Set the entry point for the program. |
### Examples
 
```asm
message .string "Hello, Ferrite!" ; strings can also be declared with a label
                                  ; like this:
                                  ; message:
                                  ;     .string "Hello, Ferrite!"
 
table:
    .word 0, 1, 2, 3, 4
 
config:
    .byte 0xFF
    .half 0x1234
    .word 0xDEADBEEF
```

Labels placed immediately before a directive mark the address of the data and can be referenced by instructions:
 
```asm
    LI   R1, message      ; R1 = address of the string
    LB   R2, R1, 0        ; R2 = 'H'
```

## 13. Memory Layout and the Stack
RAM is a flat array of bytes. RAM spans from `0x00010000` to `0x7FFFFFFF`. From the CPU's perspective it is a single flat array of bytes with no inherent structure. There is no heap region, no stack region, no separation of any kind enforced by hardware. All of that is imposed by firmware and the OS.

LW, LH, LB, SW, SH, and SB operate on this address space directly. You give them an address, they read or write it, memory is memory.

The stack starts at the top of RAM and grows downard. `sp` always points to the last used word. To push something, you decrement `sp` and then store. To pop, you load and then increment `sp`.
```asm
; push R1 onto the stack
SUB  sp, sp, 4
SW   R1, sp, 0

; pop into R1 from the stack
LW   R1, sp, 0
ADD  sp, sp, 4
```