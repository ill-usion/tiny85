.include "tn85def.inc"

.org 0x0000
    rjmp reset

reset:
    ldi r16, (1<<PB0)
    out DDRB, r16

loop:
    sbi PORTB, PB0
    rcall delay
    cbi PORTB, PB0
    rcall delay
    rjmp loop

delay:
    ldi r18, 255
d1:
    ldi r19, 255
d2:
    dec r19
    brne d2
    dec r18
    brne d1
    ret
