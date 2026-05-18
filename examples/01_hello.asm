; 01_hello.asm
; print a string over UART using a loop

.entry main
message .string "Hello, Ferrite!"
UART_TX .equ 0x80000000

main:
    LI R1, UART_TX      ; load UART_TX address
    LI R2, message      ; pointer into string data

loop:
    LB R3, R2, 0        ; load next character
    JZ R3, done         ; null terminator
    SW R3, R1, 0        ; write to UART_TX
    INC R2              ; advance pointer
    J loop              ; repeat

done:
    MOV R3, 10          ; load newline character
    SW R3, R1, 0        ; write newline to UART_TX
    HALT
