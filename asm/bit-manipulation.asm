    .include "tn85def.inc"

    .cseg
    .org    0x00
    
    ; Immediate instructions cannot be used on registers 0 through 15
    ldi     r16, 0x55   ; Load the value 85 into register 16
    mov     r0, r16     ; Load the valye of register 16 into register 0
    
    ; 16-bit values can be moved using one instruction
    ldi     r17, 0x12
    ldi     r16, 0x34

    movw    r1:r0, r17:r16  ; Move the value 0x1234 into registers 0 and 1
    
    ; Logical operations
    ; <inst>i only work with registers 16 through 31
    ldi     r16, 0b01010101
    andi    r16, 0b00001111 ; Result = 0b00000101 (0x05)
    
    ldi     r17, 0b11110000
    ori     r17, 0b00001111 ; Result = 0b11111111 (0xFF)
    
    ; Instructions without i suffix can be used with registers (not immediate)
    ldi     r16, 0b01010101
    ldi     r17, 0b10101010
    and     r16, r17        ; Result = 0xFF
    
    ; Toggle bits
    ldi     r16, 0b00000011 
    com     r16             ; One's compliment. Result = 0b11111100 (252)
    
    ldi     r17, 5
    neg     r17             ; Two's compliment. Result = -5
   
    ; Addition and subtraction
    ldi     r24, 0xFE
    ldi     r25, 0xFF
    adiw    r24, 0x01       ; 0xFFFE + 0x01 = 0xFFFF
    
    ldi     r28, 0xAA
    ldi     r29, 0x55
    sbiw    r28, 0x10       ; 0x55AA - 0x10 = 0x559A 

    ; Bit shifts
    ldi     r16, 0b10101010
    lsl     r16             ; Left shift = 0b01010100
    lsr     r16             ; Right shift = 0b00101010
    
    ldi     r16, 0b10101010
    ldi     r17, 0b10101010
    lsl     r16             ; Left shift, carry 1
    rol     r17             ; Left shift, shift whatever is in carry instead of 0. in this case 1

    ; Bit manipulation
    ldi     r16, 0xF0
    sbr     r16, 0x0F       ; Set bits in register 16 to 0x0F. Result = 0xFF
    cbr     r16, 0xF0       ; Clear bits in register 16. Result = 0x0F

    ser     r16             ; Set all bits in register 16. Result = 0xFF
    clr     r16             ; Clear all bits in register 16. Result = 0x00

    ldi     r16, 0x0F
    swap    r16             ; Swaps the lower 4 bits with the upper 4 bits. Result = 0xF0


loop:
    rjmp    loop
