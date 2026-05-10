; 02_arithmetic.asm
; exercises every integer arithmetic instruction
; results are written to uart as raw bytes

.entry main
UART_TX .equ 0x80000000

main:
    LI R15, UART_TX    ; load UART_TX address

    MOV R1, 10
    ADD R1, R1, 32     ; R1 = R1 + 32
    SW R1, R15, 0      ; send 42

    ADD R1, R1, -2
    SW R1, R15, 0      ; send 40

    MOV R1, 50
    SUB R1, R1, 8
    SW R1, R15, 0      ; send 42

    MOV R1, 6
    MUL R1, R1, 7
    SW R1, R15, 0      ; send 42

    MOV R1, 84
    DIV R1, R1, 2
    SW R1, R15, 0      ; send 42

    MOV R1, 85
    MOD R1, R1, 43
    SW R1, R15, 0      ; send 42

    MOV R1, 0xFF
    AND R1, R1, 0x2A
    SW R1, R15, 0      ; send 42

    MOV R1, 0x28
    OR R1, R1, 0x02
    SW R1, R15, 0      ; send 42

    MOV R1, 0x6F
    XOR R1, R1, 0x45
    SW R1, R15, 0      ; send 42

    MOV R1, 21
    SHL R1, R1, 1
    SW R1, R15, 0      ; send 42

    MOV R1, 84
    SHR R1, R1, 1
    SW R1, R15, 0      ; send 42

    MOV R1, -84
    SAR R1, R1, 1
    SW R1, R15, 0      ; send -42

    LI R1, 0xFFFFFFD5
    NOT R1, R1
    SW R1, R15, 0      ; send 42

    MOV R1, 41
    INC R1
    SW R1, R15, 0      ; send 42

    MOV R1, 43
    DEC R1
    SW R1, R15, 0      ; send 42

    MOV R1, -42
    NEG R1, R1
    SW R1, R15, 0      ; send 42

    HALT



