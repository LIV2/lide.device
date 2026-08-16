; Copyright (C) 2026 Matthew Harlum <matt@harlum.net>
; This code is licensed under the GNU General Public License version 2 only.
;
; The ZX0 data compression format and algorithm was designed by Einar Saukas.
; https://github.com/einar-saukas/ZX0

; To load from byte-wide rom set STRIDE to 2
        IFND STRIDE
STRIDE = 1
        ENDIF

        public  _decompress
; In:  a0 = Compressed data
;      a1 = Destination
_decompress:
        movem.l d2-d4/a2,-(sp)
        moveq.l #0,d2   ; Bit Index
        moveq.l #-1,d3  ; Offset
; Copy literal
lit:    bsr.s   elias           ; Get Literal length
        subq.l  #1,d0
.lcopy: move.b  (a0),(a1)+
        add.l   #STRIDE,a0
        dbra    d0,.lcopy
        bsr.s   get_bit
        bne.s   .newof          ; New Offset
; Copy from last offset
        bsr.s   elias           ; Get copy length
        bra.s   .copy
; Copy from new offset
.newof: bsr.s   msb             ; Offset MSB
        cmpi.l  #256,d0
        beq     .done           ; EOF
        lsl.l   #7,d0           ; MSB * 128
        moveq.l #0,d3
        move.b  (a0),d3         ; LSB
        add.l   #STRIDE,a0
        move.b  d3,d4
        lsr.b   #1,d3
        sub.l   d0,d3           ; Offset = (LSB >> 1) - MSB * 128
        moveq.l #1,d0
        btst    #0,d4           ; Leftover bit
        bne.s   .len            ; Set = end of copy length
        bsr     eres            ; Clear = get rest of copy length
.len:   addq.l  #1,d0           ; Copy length is Elias + 1
.copy:  lea.l   0(a1,d3.l),a2   ; Copy source = Dest + Offset
        subq.l  #1,d0
.cloop: move.b  (a2)+,(a1)+
        dbra    d0,.cloop
        bsr.s   get_bit
        beq.s   lit             ; Literal
        bra     .newof          ; New Offset
.done:  movem.l (sp)+,d2-d4/a2
        rts

; Get next bit of the bitstream
; Returns Z clear if the bit was set
get_bit:
        subq.l  #1,d2
        bpl.s   .get
        move.b  (a0),d1         ; Fetch next byte
        add.l   #STRIDE,a0
        moveq.l #7,d2           ; Reset bit index
.get:   btst    d2,d1
        rts

; Decode interlaced Elias-Gamma value
elias:  moveq.l #1,d0
eloop:  bsr.s   get_bit         ; Control bit
        bne.s   edone           ; Set = end of value
eres:   add.l   d0,d0
        bsr.s   get_bit         ; Value bit
        beq.s   eloop
        addq.l  #1,d0
        bra     eloop
edone:  rts

; Decode interlaced Elias-Gamma value with inverted value bits
msb:    moveq.l #1,d0
.loop:  bsr.s   get_bit         ; Control bit
        bne.s   .done           ; Set = end of value
        add.l   d0,d0
        bsr.s   get_bit         ; Value bit (inverted)
        bne.s   .loop
        addq.l  #1,d0
        bra     .loop
.done:  rts
