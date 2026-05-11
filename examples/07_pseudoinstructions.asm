; 07_pseudoinstructions.asm
; nop, mov, li, clr, neg, jz, jnz, call, ret, j

.entry main
UART_TX .equ 0x80000000

; simple function to test call and ret
returns_42:
    MOV rv, 42
    RET

main:
    LI R15, UART_TX    ; load UART_TX address

    NOP
    NOP
    NOP
    MOV R1, 1
    SW R1, R15, 0      ; send 1

    ; MOV reg to reg
    MOV R1, 99
    MOV R2, R1
    SW R2, R15, 0      ; send 99

    ; MOV immediate
    MOV R1, 123
    SW R1, R15, 0      ; send 123

    ; LI (load immediate)
    LI R1, 0x80000000
    SHR R1, R1, 24     ; R1 should now be 0x80
    SW R1, R15, 0      ; send 0x80

    ; LI negative
    LI R1, -1
    ADD R1, R1, 2      ; R1 should now be 1
    SW R1, R15, 0      ; send 1

    ; CLR
    MOV R1, 0xFF
    CLR R1
    SW R1, R15, 0      ; send 0

    ; NEG 
    MOV R2, 42
    NEG R1, R2         ; R1 = -R2
    ADD R1, R1, 84     ; R1 should now be 42
    SW R1, R15, 0      ; send 42

    ; JZ
    CLR R1
    JZ R1, jz_taken
    MOV R2, 0
    J jz_done
jz_taken:
    MOV R2, 1
jz_done:
    SW R2, R15, 0      ; send 1

    ; JNZ
    MOV R1, 5
    JNZ R1, jnz_taken
    MOV R2, 0
    J jnz_done
jnz_taken:
    MOV R2, 1
jnz_done:
    SW R2, R15, 0      ; send 1

    ; CALL and RET
    LI sp, 0x7FFFFFFC  ; initialize stack pointer
    CALL returns_42    ; should return 42 in rv
    SW rv, R15, 0      ; send result of function call

    HALT

