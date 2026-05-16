; 10_stack.asm

.entry main
UART_TX .equ 0x80000000

; double a0 (argument 0) and store result in return register rv
double:
    SHL rv, a0, 1
    RET

; return n * 4 in rv, needs stack frame to save ra and a0
quad:
    SUB sp, sp, 12 ; allocate stack
    SW ra, sp, 8   ; save return address
    SW a0, sp, 4   ; save argument n
    SW fp, sp, 0   ; save old frame pointer
    MOV fp, sp     ; set new frame pointer

    MOV s0, a0     ; save n in s0

    CALL double    ; rv = n * 2
    MOV a0, rv     ; prepare argument for double again
    CALL double    ; rv = n*2

    MOV sp, fp     ; restore stack pointer
    LW fp, sp, 0   ; restore old frame pointer
    LW s0, sp, 4   ; restore n
    LW ra, sp, 8   ; restore return address
    ADD sp, sp, 12 ; deallocate stack

    RET            ; return to ra

fibonacci:
    ; n <= 1
    MOV t0, 1             ; compare n with 1
    JLE a0, t0, fib_base  ; if n <= 1, jump to base case
 
    SUB sp, sp, 16 ; allocate stack frame
    SW ra, sp, 12  ; save return address
    SW s0, sp, 8   ; save n
    SW s1, sp, 4   ; save s1
    SW fp, sp, 0   ; save old frame pointer
    MOV fp, sp     ; set new frame pointer
 
    MOV s0, a0     ; save n
 
    SUB a0, s0, 1  ; prepare arg
    CALL fibonacci ; rv = fib(n-1)
    MOV s1, rv     ; s1 = rv

    SUB   a0, s0, 2    ; prepare arg
    CALL  fibonacci    ; rv = fib(n-2)
    ADD   rv, rv, s1   ; rv = rv + s1
 
    MOV sp, fp         ; restore stack pointer
    LW fp, sp, 0       ; restore old frame pointer
    LW s1, sp, 4       ; restore s1
    LW s0, sp, 8       ; restore n
    LW ra, sp, 12      ; restore return address
    ADD sp, sp, 16     ; deallocate stack
    RET                ; return to ra
 
fib_base:
    MOV rv, a0 ; return n for n=0 or n=1
    RET        ; return

main:
    LI sp, 0x7FFFFFFC ; initialize stack pointer to top of memory
    LI R15, UART_TX   ; set R15 to UART
    
    ; test double
    MOV a0, 21
    CALL double
    SW rv, R15, 0     ; send 42

    ; test quad
    mov a0, 10
    CALL quad
    SW rv, R15, 0     ; send 40

    ; test fibonacci to 7
    CLR t1
    MOV t2, 8
fib_loop:
    MOV a0, t1
    CALL fibonacci
    SW rv, R15, 0     ; send fib(n)
    INC t1
    JLT t1, t2, fib_loop

    HALT