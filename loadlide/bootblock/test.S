    include exec/execbase.i
    include exec/lists.i
    include exec/resident.i
    include lvo/exec_lib.i
    include libraries/expansionbase.i

start:
        lea     (ExpName,PC),a1
        moveq.l #0,D0
        jsr     _LVOOpenLibrary(A6)
        tst.l   D0
        move.l  D0,A2
        bne     .ok
        move.l  #$B16B00B5,D7
        jsr     _LVOAlert(A6)
.ok:    jsr     _LVOForbid(A6)               
.l1     lea.l   eb_MountList(A2),A0
        jsr     _LVORemHead(A6)
        move.l  D0,A1
        lea.l   eb_MountList(A2),A0
        jsr     _LVOAddTail(A6)
        jsr     _LVOPermit(A6)
boot:   lea     (dos_name,pc),a1
        moveq.l #0,d0
        jsr     _LVOFindResident(A6)
        tst.l   d0
        beq     error
        move.l  d0,a0
        move.l  RT_INIT(A0),A0
        jsr     (A0)
        moveq.l #0,d0
        rts

error:  moveq.l #-1,d0
        rts

ExpName: DC.B "expansion.library",0

dos_name: DC.B "dos.library",0