    include lvo/exec_lib.i
    include libraries/expansionbase.i
    include exec/resident.i

; Custom bootblock code
;
; OS 3.2 has a couple of bugs when booting a 1.3 style boot-block (i.e without EBB_SILENTSTART set)
; echo stalls until the enter key is pressed, and gibberish spews out in the CLI after our startup-sequence completes
; This has been reported by other users here: https://eab.abime.net/showthread.php?t=115917
;
; lideflash needs to run on kick 1.3 and also 3.2
; To support both, set EBB_SILENTSTART if kick supports it
start:  lea     (exp_name,pc),a1
        moveq.l #37,d0
        jsr     _LVOOpenLibrary(a6)
        tst.l   d0
        beq     boot                           ; Kick 1.3, no silentstart
        movea.l d0,a1
        bset.b  #EBB_SILENTSTART,(eb_Flags,A1) ; Set SILENTSTART to stop OS 3.2 from wigging out
        jsr     _LVOCloseLibrary(a6)
boot:   lea     (dos_name,pc),a1
        moveq.l #0,d0
        jsr     _LVOFindResident(A6)
        tst.l   d0
        beq     error
        move.l  d0,a0
        move.l  RT_INIT(A0),A0
        moveq.l #0,d0
        rts

error:  moveq.l #-1,d0
        rts

exp_name:
    DC.B "expansion.library",0
dos_name:
    DC.B "dos.library",0