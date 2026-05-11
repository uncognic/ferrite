; 06_loops.asm
; commoon loop patterns
; sends loop iteration over UART

.entry main
UART_TX .equ 0x80000000

main:
    LI R15, UART_TX    ; load UART_TX address

    ; count from 0 to 4
    CLR R1
    MOV R2, 5
count_up:
    SW R1, R15, 0         ; send current count
    INC R1
    JLT R1, R2, count_up  ; loop until R1 == R2


    ; count from 4 down to 1
    MOV R1, 4
count_down:
    SW R1, R15, 0         ; send current count
    DEC R1
    JNZ R1, count_down     ; loop while R1 != 0

    ; double R1 until it is bigger than or equal to 100
    MOV R1, 1
    MOV R2, 100
double_up:
    JGE R1, R2, double_done ; loop until R1 >= R2
    SW R1, R15, 0           ; send current value
    SHL R1, R1, 1           ; R1 = R1 * 2
    J double_up
double_done:
    SW R1, R15, 0           ; send final value of R1 (should be 128)

    ; do-while
    ; sends 5 even if condition is already false
    MOV R1, 5
do_while:
    SW R1, R15, 0         ; send current value
    DEC R1
    JNZ R1, do_while      ; loop until R1 == 0

    ; nested loops
    MOV R1, 1           ; inner counter
    MOV R2, 5           ; limit
    MOV R3, 2           ; multiplier

nested:
    MUL R4, R1, R3       ; R4 = R1 * R3
    SW R4, R15, 0        ; send current value of R4
    INC R1
    JLE R1, R2, nested     ; loop until R1 > R2

    HALT

