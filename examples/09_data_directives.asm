; 09_data_directives.asm

.entry main
UART_TX .equ 0x80000000

WIDTH .equ 80
HEIGHT .equ 24
AREA .equ 1920 ; 80 * 24

main:
    LI R15, UART_TX

    ; read .word data
    LI R1, word_data
    LW R2, R1, 0  ; R2 = *R1 + 0
    SW R2, R15, 0 ; send 0xAA
    LW R2, R1, 4  ; R2 = *R1 + 4
    SW R2, R15, 0 ; send 0xBB

    ; read .half data
    LI R1, half_data
    LH R2, R1, 0  ; R2 = *R1 + 0
    SW R2, R15, 0 ; send 0x12, lower byte only
    LH R2, R1, 2  ; R2 = *R1 + 2
    SW R2, R15, 0 ; send 0x56, lower byte of 0x5678

    ; read .byte data
    LI R1, byte_data
    LB R2, R1, 0  ; R2 = *R1 + 0
    SW R2, R15, 0 ; send 1
    LB R2, R1, 1  ; R2 = *R1 + 1
    SW R2, R15, 0 ; send 2
    LB R2, R1, 2  ; R2 = *R1 + 2
    SW R2, R15, 0 ; send 3

    ; .equ directive
    MOV R1, AREA
    SW R1, R15, 0 ; send lower byte of 1920

    ; read .string and print it character by character

    LI R1, greeting
loop:
    LB R2, R1, 0    ; load character
    JZ R2, end_loop ; if null terminator, exit loop
    SW R2, R15, 0   ; send character
    INC R1          ; move to next character
    J loop
end_loop:
    ; write 0xCA to a specific RAM address at runtime
    LI R1, 0x00030000
    MOV R2, 0xCA
    SW R2, R1, 0
    LW R3, R1, 0
    SW R3, R15, 0    ; send 0xCA
    HALT
 

.align 4
word_data:
    .word 0xAA, 0xBB
 
.align 2
half_data:
    .half 0x1234, 0x5678
 
byte_data:
    .byte 1, 2, 3
 
.align 4
greeting .string "Hi!\n"
 