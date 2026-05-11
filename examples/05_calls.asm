; 05_calls.asm
; demonstrates function calls, return values and The Stack

.entry main
UART_TX .equ 0x80000000

; print_char: print the character in a0 to UART
print_char:
    LI    R14, UART_TX
    SW    a0, R14, 0
    RET

; print_string: print the null-terminated string pointed to by a0
    SUB sp, sp, 12 ; reserve space for 3 registers on the stack
    SW ra, sp, 8   ; save return address
    SW fp, sp, 4   ; save frame pointer
    SW s0, sp, 0   ; save s0
    MOV fp, sp     ; set frame pointer for this function
    MOV s0, a0     ; save pointer to string in s0
ps_loop:
    LB a0, s0, 0     ; load next character
    JZ a0, ps_done   ; null terminator
    CALL print_char  ; print character
    INC s0           ; advance pointer
    J ps_loop        ; repeat

ps_done:
    MOV sp, fp     ; restore stack pointer
    LW s0, sp, 0   ; restore s0
    LW fp, sp, 4   ; restore frame pointer
    LW ra, sp, 8   ; restore return address
    ADD sp, sp, 12 ; deallocate stack space
    RET

; add: returns a0 + a1 in rv
add:
    ADD rv, a0, a1
    RET

main:
    LI sp, 0x7FFFFFFC ; initialize stack pointer

    ; call add(10,32)
    MOV a0, 10
    MOV a1, 32
    CALL add          ; rv should now be 42

    LI R14, UART_TX
    SW rv, R14, 0     ; print result of add

    ; send newline
    MOV a0, 10
    CALL print_char

    ; print string with print_char in a loop
    LI R1, UART_TX
    LI R2, greeting
loop:
    LB R3, R2, 0    ; load next character
    JZ R3, done     ; null terminator
    SW R3, R1, 0    ; write to UART_TX
    INC R2          ; advance pointer
    J loop          ; repeat
done:
    HALT

.align 4
greeting .string "Hello from Ferrite!"