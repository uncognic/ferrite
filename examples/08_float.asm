; 08_float.asm
; exercises floating-point instructions
.entry main

UART_TX .equ 0x80000000
SCRATCH .equ 0x00020000

f_6 .word 6.0
f_7 .word 7.0
f_2 .word 2.0
f_neg42 .word -42.0
f_1764 .word 1764.0
f_1 .word 1.0
f_pi .word 3.14159
f_e .word 2.71828

main:
    LI R15, UART_TX

    ; FADD: 20.0 + 22.0 = 42.0
    LI R1, SCRATCH
    MOV R2, 20
    FCVT.FI F0, R2
    MOV R2, 22
    FCVT.FI F1, R2
    FADD F2, F0, F1
    FCVT.IF R2, F2
    SW R2, R15, 0       ; send 42

    ; FSUB: 50.0 - 8.0 = 42.0
    MOV R2, 50
    FCVT.FI F0, R2
    MOV R2, 8
    FCVT.FI F1, R2
    FSUB F2, F0, F1
    FCVT.IF R2, F2
    SW  R2, R15, 0       ; send 42

    ; FMUL: 6.0 * 7.0 = 42.0
    LI R1, f_6
    FLW F0, R1, 0
    LI R1, f_7
    FLW F1, R1, 0
    FMUL F2, F0, F1
    FCVT.IF R2, F2
    SW R2, R15, 0       ; send 42

    ; FDIV: 84.0 / 2.0 = 42.0
    MOV R2, 84
    FCVT.FI F0, R2
    LI R1, f_2
    FLW F1, R1, 0
    FDIV F2, F0, F1
    FCVT.IF R2, F2
    SW R2, R15, 0       ; send 42

    ; FNEG: -(-42.0) = 42.0
    LI R1, f_neg42
    FLW F0, R1, 0
    FNEG F0, F0
    FCVT.IF R2, F0
    SW R2, R15, 0       ; send 42

    ; FABS: |-42.0| = 42.0
    LI R1, f_neg42
    FLW F0, R1, 0
    FABS F0, F0
    FCVT.IF R2, F0
    SW R2, R15, 0       ; send 42

    ; FSQRT: sqrt(1764.0) = 42.0
    LI R1, f_1764
    FLW F0, R1, 0
    FSQRT F0, F0
    FCVT.IF R2, F0
    SW R2, R15, 0       ; send 42

    ; store pi to RAM then load it back
    LI R1, f_pi
    FLW F0, R1, 0        ; F0 = 3.14159
    LI R1, SCRATCH
    FSW F0, R1, 0        ; store to RAM
    FLW F1, R1, 0        ; load back
    FCVT.IF R2, F1
    SW R2, R15, 0       ; send 3 (truncated)

    ; FCVT.FI / FCVT.IF roundtrip
    MOV R2, 99
    FCVT.FI F0, R2         ; F0 = 99.0
    FCVT.IF R2, F0         ; R2 = 99
    SW R2, R15, 0       ; send 99

    ; FJEQ: pi == pi should branch
    LI R1, f_pi
    FLW F0, R1, 0
    FLW F1, R1, 0
    FJEQ F0, F1, fjeq_pass
    MOV R2, 0
    J fjeq_done
fjeq_pass:
    MOV R2, 1
fjeq_done:
    SW R2, R15, 0       ; send 1

    ; FJLT: e < pi should branch
    LI R1, f_e
    FLW F0, R1, 0
    LI R1, f_pi
    FLW F1, R1, 0
    FJLT F0, F1, fjlt_pass
    MOV R2, 0
    J fjlt_done
fjlt_pass:
    MOV R2, 1
fjlt_done:
    SW R2, R15, 0       ; send 1

    ; FJGT: pi > e should branch
    LI R1, f_pi
    FLW F0, R1, 0
    LI R1, f_e
    FLW F1, R1, 0
    FJGT F0, F1, fjgt_pass
    MOV R2, 0
    J fjgt_done
fjgt_pass:
    MOV R2, 1
fjgt_done:
    SW R2, R15, 0       ; send 1

    HALT