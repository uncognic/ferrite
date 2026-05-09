; print a string over UART using a loop

    LI R1, 0x80000000   ; UART_TX
    LI R2, message      ; pointer into string data

loop:
    LB R3, R2, 0        ; load next character
    JZ R3, done         ; null terminator
    SW R3, R1, 0        ; write to UART_TX
    INC R2              ; advance pointer
    J loop

done:
    HALT

message:
    .string "Hello, Ferrite!"