; Copyright (C) 2026 Matthew Harlum <matt@harlum.net>
; This code is licensed under the GNU General Public License version 2 only.
    include exec/resident.i
    include exec/nodes.i
    include lvo/exec_lib.i
    include tmp/version.i

; Kick module
; Decompress + Relocate the attached driver to RAM
; 
; AmigaPCI uses extended ROM for the IDE driver
; Running the driver from ROM is slow so instead lets relocate it to RAM


; Setup STRIDE for zx0 decompressor
STRIDE = 1
        public STRIDE
        align 2

Romtag: dc.w    RTC_MATCHWORD      ; UWORD RT_MATCHWORD
        dc.l    Romtag             ; APTR  RT_MATCHTAG
        dc.l    _endskip           ; APTR  RT_ENDSKIP
        dc.b    RTF_COLDSTART      ; UBYTE RT_FLAGS
        dc.b    40                 ; UBYTE RT_VERSION
        dc.b    NT_DEVICE          ; UBYTE RT_TYPE
        dc.b    10                 ; BYTE  RT_PRI
        dc.l    Name               ; APTR  RT_NAME
        dc.l    _idstring          ; APTR  RT_IDSTRING
        dc.l    _init              ; APTR  RT_INIT
Name:   dc.b    'lide.device',0    ; Name string

_init:  lea.l   _driver(PC),a0
        moveq.l #0,d0
        bsr     _relocate
        tst.l   d0
        beq.s   .nort
        move.l  d0,a1
        move.l  #4096,d1
.search:cmp.w   #RTC_MATCHWORD,(a1)
        bne.s   .notrt
        cmp.l   RT_MATCHTAG(a1),a1
        beq.s   .found
.notrt: addq.l  #2,a1
        dbra    d1,.search
        bra.s   .nort
.found: move.l  0,d1
        jsr     _LVOInitResident(a6)
.nort:  moveq.l #0,d0
        rts

    align 2
_driver:
        incbin obj/rom-driver