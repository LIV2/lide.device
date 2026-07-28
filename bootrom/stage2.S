    include libraries/configvars.i
    include exec/resident.i
    include exec/memory.i
    include lvo/exec_lib.i
    include boards.i

    IFND WORDWIDE
DRIVEROFFSET = $2000
    ELSE
DRIVEROFFSET = $1000
    ENDIF


Stage2: bra.s   Go

BoardBase:   dc.l 0
MyConfigDev: dc.l 0

Go:     lea.l   BoardBase(PC),a3
        move.l  a0,(a3)
        move.l  a1,4(a3)
        moveq   #0,d2
        moveq.l #0,d1
        move.w  cd_Rom+er_InitDiagVec(a1),d1  ; Get rom offset
        cmp.w   #1,d1                         ; AT-Bus with odd address?
        beq     .odd
        cmp.w   #$100,d1                      ; WinUAE AT-Bus with ROM at odd address (despite DiagVec...)
        bne     .even
.odd    add.l   #1,d2
.even 
        and.l   #$F000,d1                     ; Add any other offset
        add.l   d1,d2
        cmp.l   #$20000,cd_BoardSize(a1)      ; 128K Board?
        bne     .loaddriver
.loadcdfs
        move.l  BoardBase(PC),a0
        move.l  a0,a1
        add.l   d2,a1
        add.l   #$1FFF8,a1                    ; Point to the end of the second bank
        moveq.l #3,d3
.getid: lsl.l   #8,d0                         ; Look for the ROM trailer which should not be on the second bank
        move.b  (a1),d0
        add.l   #2,a1
        dbra    d3,.getid
        cmp.l   #'LIDE',d0 
        beq     .loaddriver

        IFND WORDWIDE

        move.l  #$10000,d0                    ; Reloc offset to second bank
        add.l   d2,d0                         ; Add the odd offset if needed

        ELSE

        moveq.l #0,d0
        add.l   d2,a0
        add.l   #$8000,a0      

        ENDIF
        bsr     _relocate                     ; BoardBase already in A0
        tst.l   d0
        beq.s   .skipcdfs
        bsr     InitRT

.skipcdfs:

        IFND WORDWIDE
        ; Load romtags from other banks if supported (currently only RIDE)
        move.l  MyConfigDev(PC),a1
        cmp.w   #OAHR_MFR_ID,cd_Rom+er_Manufacturer(a1)
        bne     .loaddriver
        cmp.b   #RIDE_PRD_ID,cd_Rom+er_Product(a1)
        bne     .loaddriver
        moveq.l #1,d3
        move.b  #$C0,d4             ; Start with Bank #4
.bank:  move.l  BoardBase(PC),a1
        move.l  a1,a0               ; BoardBase for _reloc
        add.l   #RIDE_BANKREG,a1
        move.b  (a1),d1
        and.b   #$3F,d1             ; Preserve other bits
        or.b    d4,d1
        move.b  d1,(a1)             ; Select bank, the rom will dissapear from the lower 64K window now
        move.l  #$10000,d0          ; So point to the upper 64K window where the ROM is always mapped
        bsr     _relocate           ; A0 = Base, D0 = Offset (Must be non-zero)
        tst.l   d0
        beq.s   .nort
        bsr     InitRT
.nort:  sub.l   #$40,d4
        dbra    d3,.bank
        move.l  BoardBase(PC),a1
        add.l   #RIDE_BANKREG,a1
        and.b   #$3F,(a1)           ; Point back at Bank #0
        add.l   #$10000,d2          ; Set Offset to upper window
        ENDIF

.loaddriver:
        move.l  BoardBase(PC),a0

        IFND WORDWIDE

        move.l  #DRIVEROFFSET,d0
        add.l   d2,d0               ; Add offset if AT-Bus with odd address

        ELSE

        move.l  #0,d0
        add.l   d2,a0
        add.l   #DRIVEROFFSET,a0

        ENDIF
        bsr     _relocate
        tst.l   d0
        beq.s   .err
        bsr     InitRT
        tst.l   d0
        bne.s   .err
        rts

.err    moveq.l #-1,d0
        rts

* Search for the driver Romtag
InitRT
        movem.l d2/d3/a1,-(sp)
        move.l  #$1000,d3            ; Search the first 4096 words for a Romtag
        move.l  d0,a1
.findrt cmpi.w  #$4AFC,(a1)
        bne.s   .not_rt
        cmp.l   RT_MATCHTAG(a1),a1   ; Check that the RT_MATCHTAG is correct
        beq.s   .found
.not_rt addq.l  #2,a1
        dbra    d3,.findrt
        movem.l (sp)+,d2/d3/a1
        moveq.l #-1,d0
        rts

* Found the driver, initialize it
.found  
        moveq.l  #0,d1
        jsr      _LVOInitResident(A6)
.done
        movem.l (sp)+,d2/d3/a1
        moveq.l #0,d0
        rts

    include reloc.S