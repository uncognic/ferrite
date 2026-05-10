; 03_loads_stores.asm
; exercises all load and store widths
; writes a word to ram then reads it bac as word, halfword and byte

.entry main

UART_TX .equ 0x80000000
SCRATCH .equ 0x00020000     ; a safe address in RAM to use as scratch space

main:
    LI R15, UART_TX    ; load UART_TX address

    ; SW / LW
    LI R1, SCRATCH
    LI R2, 0xDEADBEEF
    SW R2, R1, 0       ; store full word
    LW R3, R1, 0       ; load full word
    SW R3, R15, 0      ; send 0xEF

    ; SH / LH
    LI R1, SCRATCH
    LI R2, 0x1234
    SH R2, R1, 0       ; store halfword
    LH R3, R1, 0       ; load halfword
    SW R3, R15, 0      ; send 0x34

    ; SB / LB
    LI R1, SCRATCH
    MOV R2, 0xAB
    SB R2, R1, 0       ; store byte
    LB R3, R1, 0       ; load byte
    SW R3, R15, 0      ; send 0xAB

    ; load with positive offset
    LI R1, SCRATCH
    MOV R2, 99
    SB R2, R1, 4      ; store byte at SCRATCH+4
    LB R3, R1, 4      ; load byte from SCRATCH+4
    SW R3, R15, 0     ; send 99

    ; load with negative offset
    LI R1, SCRATCH
    ADD R1, R1, 8     ; R1 = SCRATCH + 8
    MOV R2, 67
    SB R2, R1, -4     ; store byte at SCRATCH+4
    LB R3, R1, -4     ; load byte from SCRATCH+4
    SW R3, R15, 0     ; send 67

    ; read from .word data
    LI R1, data       ; load address of data
    LW R2, R1, 0      ; load word from data
    SW R2, R15, 0     ; send 0xAA

    LW R2, R1, 4      ; load word from data+4
    SW R2, R15, 0     ; send 0xBB

    HALT

.align 4
data:
    .word 0xAA, 0xBB
