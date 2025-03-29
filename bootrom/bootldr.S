; Based on rom.S from Open Source A4091.device by Stefan Reinauer & Chris Hooper 
    include libraries/configvars.i
    include exec/resident.i
    include lvo/exec_lib.i
    include boards.i

VERSION      = 1
REVISION     = 1
        IFND WORDWIDE
DRIVEROFFSET = $2000
        ELSE
DRIVEROFFSET = $1000
        ENDIF

*******  DiagStart  **************************************************
DiagStart
        ; This is the DiagArea structure whose relative offset from
        ; your board base appears as the Init Diag vector in your
        ; autoconfig ID information.  This structure is designed
        ; to use all relative pointers (no patching needed).
        IFND WORDWIDE
        dc.b    DAC_NIBBLEWIDE+DAC_CONFIGTIME  ; da_Config
        ELSE
        dc.b    DAC_WORDWIDE+DAC_CONFIGTIME    ; da_Config
        ENDIF
        dc.b    0                              ; da_Flags
        dc.w    EndCopy-DiagStart              ; da_Size
        dc.w    DiagEntry-DiagStart            ; da_DiagPoint
        dc.w    BootEntry-DiagStart            ; da_BootPoint
        dc.w    DevName-DiagStart              ; da_Name
        dc.w    0                              ; da_Reserved01
        dc.w    0                              ; da_Reserved02


*******  DiagEntry  **************************************************
**********************************************************************
*
*   success = DiagEntry(BoardBase,DiagCopy, configDev)
*   d0                  a0         a2       a3
*
*   Called by expansion architecture to relocate any pointers
*   in the copied diagnostic area.   We will patch the romtag.
*   If you have pre-coded your MakeDosNode packet, BootNode,
*   or device initialization structures, they would also need
*   to be within this copy area, and patched by this routine.
*
**********************************************************************
* These are the calling conventions for the Diag routine
*
* A7 -- points to at least 2K of stack
* A6 -- ExecBase
* A5 -- ExpansionBase
* A3 -- your board's ConfigDev structure
* A2 -- Base of diag/init area that was copied
* A0 -- Base of your board
*
* The Diag routine shall return a non-zero value in D0 for success.
* If this value is NULL, then the diag/init area that was copied
* will be returned to the free memory pool.
*
DiagEntry
        movem.l d2-d7/a2-a6,-(sp)

        ; Patch up Romtag Resident Structure in memory
        lea.l   Romtag(pc),a4                       ; find Romtag in memory
        move.l  a2,d0
        add.l   d0,RT_MATCHTAG(a4)                  ; pointer to itself
        add.l   d0,RT_ENDSKIP(a4)
        add.l   d0,RT_NAME(a4)
        add.l   d0,RT_IDSTRING(a4)
        add.l   d0,RT_INIT(a4)
        move.l  a0,BoardBase-Romtag(a4)             ; Save board base for later
        move.l  a3,MyConfigDev-Romtag(a4)           ; Save ConfigDev for later
        cmp.w   #$082c,cd_Rom+er_Manufacturer(a3)
        bne     .skip
        ; For AT-Bus devices change the manufacturer/prod id to prevent Oktapussy meddling with us
        move.w  #$0A1C,cd_Rom+er_Manufacturer(a3)   ; Manufacturer: A1K.org
        move.b  #$7D,cd_Rom+er_Product(a3)          ; Product: Matzes IDE-Controller
.skip   movem.l (sp)+,d2-d7/a2-a6
        moveq.l #1,d0
        rts

*******  BootEntry  **************************************************
**********************************************************************

BootEntry
        lea     DosName(PC),a1          ; 'dos.library',0
        jsr     _LVOFindResident(a6)    ; find the DOS resident tag
        tst.l   d0
        beq.s   .BadBoot
        move.l  d0,a0                   ; in order to bootstrap
        move.l  RT_INIT(A0),a0          ; set vector to DOS INIT
        jsr     (a0)                    ; and initialize DOS
.BadBoot
        rts

**********************************************************************
*   Chainloader
*   This module will:
*     - Load a driver from ROM.
*     - Initialize it
**********************************************************************
Romtag
        dc.w    RTC_MATCHWORD      ; UWORD RT_MATCHWORD
        dc.l    Romtag             ; APTR  RT_MATCHTAG
        dc.l    EndCopy            ; APTR  RT_ENDSKIP
        dc.b    RTF_COLDSTART      ; UBYTE RT_FLAGS
        dc.b    VERSION            ; UBYTE RT_VERSION
        dc.b    0                  ; UBYTE RT_TYPE
        dc.b    0                  ; BYTE  RT_PRI
        dc.l    DevName            ; APTR  RT_NAME
        dc.l    DevName            ; APTR  RT_IDSTRING
        dc.l    Init               ; APTR  RT_INIT

BoardBase:   dc.l 0
MyConfigDev: dc.l 0

Init:   movem.l d2-d7/a2-a6,-(sp)
        move.l  MyConfigDev(PC),a1
        moveq.l #0,d1
        move.w  cd_Rom+er_InitDiagVec(a1),d1  ; Get rom offset
        moveq   #0,d2
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
        beq.s   .ok

.err
        *******  Show checkered purple failure screen **************************
        movem.l d2-d3,-(sp)
        move.w  #$0020,d2
.fc1
        move.w  #$ffff,d3
.fc2
        move.w  #$0000,$dff180               ; black
        move.w  #$0f0c,$dff180               ; purple
        dbra    d3,.fc2
        dbra    d2,.fc1
        movem.l (sp)+,d2-d3
        *******  End checkered purple failure screen **************************

 .ok:   moveq.l #0,d0
        movem.l (sp)+,d2-d7/a2-a6
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

*******  Strings referenced in Diag Copy area  **************************
*************************************************************************
DevName
        dc.b    'Chainloader',0                 ; Name string
        align 1
DosName
        dc.b    'dos.library',0                 ; DOS library name
        align 1     ; word align

    include reloc.S
EndCopy:
