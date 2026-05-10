; 04_branches.asm
; exercises all branch instructions
; each test sends P or F to UART

.entry main
UART_TX .equ 0x80000000
PASS .equ 80 ; ASCII 'P'
FAIL .equ 70 ; ASCII 'F'
NEWLINE .equ 10

main:
    LI R15, UART_TX    ; load UART_TX address

    ; JEQ
    MOV R1, 42
    MOV R2, 42
    JEQ R1, R2, jeq_pass
    J jeq_fail
jeq_pass:
    MOV R3, PASS
    J jeq_done
jeq_fail:
    MOV R3, FAIL
    J jeq_done
jeq_done:
    SW R3, R15, 0     ; send result

    ; JNE
    MOV R1, 42
    MOV R2, 43
    JNE R1, R2, jne_pass
    J jne_fail
jne_pass:
    MOV R3, PASS
    J jne_done
jne_fail:
    MOV R3, FAIL
    J jne_done
jne_done:
    SW R3, R15, 0     ; send result

    ; JLT
    MOV R1, -1
    MOV R2, 1
    JLT R1, R2, jlt_pass
    J jlt_fail
jlt_pass:
    MOV R3, PASS
    J jlt_done
jlt_fail:
    MOV R3, FAIL
    J jlt_done
jlt_done:
    SW R3, R15, 0     ; send result

    ; JGT
    MOV R1, 5
    MOV R2, 3
    JGT R1, R2, jgt_pass
    J jgt_fail
jgt_pass:
    MOV R3, PASS
    J jgt_done
jgt_fail:
    MOV R3, FAIL
    J jgt_done
jgt_done:
    SW R3, R15, 0     ; send result

    ; JLE
    MOV R1, 2
    MOV R2, 2
    JLE R1, R2, jle_pass
    J jle_fail
jle_pass:
    MOV R3, PASS
    J jle_done
jle_fail:
    MOV R3, FAIL
    J jle_done
jle_done:
    SW R3, R15, 0     ; send result

    ; JGE
    MOV R1, 4
    MOV R2, 3
    JGE R1, R2, jge_pass
    J jge_fail
jge_pass:
    MOV R3, PASS
    J jge_done
jge_fail:
    MOV R3, FAIL
    J jge_done
jge_done:
    SW R3, R15, 0     ; send result

    ; JLTU
    MOV R1, 1
    LI R2, 0xFFFFFFFF
    JLTU R1, R2, jltu_pass
    J jltu_fail

jltu_pass:
    MOV   R3, PASS
    J     jltu_done
jltu_fail:
    MOV   R3, FAIL
jltu_done:
    SW    R3, R15, 0

    ; JGTU
    LI    R1, 0xFFFFFFFF
    MOV   R2, 1
    JGTU  R1, R2, jgtu_pass
    J     jgtu_fail

jgtu_pass:
    MOV   R3, PASS
    J     jgtu_done
jgtu_fail:
    MOV   R3, FAIL
jgtu_done:
    SW    R3, R15, 0

    ; JZ
    CLR   R1
    JZ    R1, jz_pass
    J     jz_fail
jz_pass:
    MOV   R3, PASS
    J     jz_done
jz_fail:
    MOV   R3, FAIL
jz_done:
    SW    R3, R15, 0
 
    MOV   R1, 1
    JNZ   R1, jnz_pass
    J     jnz_fail
jnz_pass:
    MOV   R3, PASS
    J     jnz_done
jnz_fail:
    MOV   R3, FAIL
jnz_done:
    SW    R3, R15, 0

    ; newline
    MOV   R3, NEWLINE
    SW    R3, R15, 0

    HALT