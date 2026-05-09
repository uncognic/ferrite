; print "Hello, Ferrite!" over UART then halt

        LI   R1, 0x80000000   ; R1 = UART_TX address

        MOV  R2, 72           ; 'H'
        SW   R2, R1, 0
        MOV  R2, 101          ; 'e'
        SW   R2, R1, 0
        MOV  R2, 108          ; 'l'
        SW   R2, R1, 0
        MOV  R2, 108          ; 'l'
        SW   R2, R1, 0
        MOV  R2, 111          ; 'o'
        SW   R2, R1, 0
        MOV  R2, 44           ; ','
        SW   R2, R1, 0
        MOV  R2, 32           ; ' '
        SW   R2, R1, 0
        MOV  R2, 70           ; 'F'
        SW   R2, R1, 0
        MOV  R2, 101          ; 'e'
        SW   R2, R1, 0
        MOV  R2, 114          ; 'r'
        SW   R2, R1, 0
        MOV  R2, 114          ; 'r'
        SW   R2, R1, 0
        MOV  R2, 105          ; 'i'
        SW   R2, R1, 0
        MOV  R2, 116          ; 't'
        SW   R2, R1, 0
        MOV  R2, 101          ; 'e'
        SW   R2, R1, 0
        MOV  R2, 33           ; '!'
        SW   R2, R1, 0
        MOV  R2, 10           ; '\n'
        SW   R2, R1, 0

        HALT