; 16-bit arithmatic

    .include "tn85def.inc"

    .def num1L = r16    ; Define lower byte of number 1 as r16
    .def num1H = r17    ; Define higher byte of number 1 as r17
    .def num2L = r18    ; Define lower byte of number 2 as r18
    .def num2H = r19    ; Define higher byte of number 2 as r19

    .cseg
    .org 0x00
    
    ; Load values into registers
    ldi     num1L, 0x34
    ldi     num1H, 0x12
    ldi     num2L, 0xCD
    ldi     num2H, 0xAB

    add     num1L, num2L    ; Add lower bytes
    adc     num1H, num2H    ; Add higher bytes with carry

loop:
    rjmp    loop

