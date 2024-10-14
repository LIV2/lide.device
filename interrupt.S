    INCLUDE "lvo/exec_lib.i"
    INCLUDE "exec/types.i"
    INCLUDE "idetask.i"

    PUBLIC _irq_handler

_irq_handler:   move.l  a6,-(sp)
                moveq.l #0,d0
                movea.l INTREG(a1),a0
                move.b  (a0),d1
                and.b   INTMASK(a1),d1
                beq     exit
                movea.l DRIVESTATUS(A1),a0
                tst.b   (a0)
                movea.l SYSBASE(a1),a6
                move.l  SIGMASK(a1),d0
                movea.l TASK(a1),a1
                jsr     _LVOSignal(a6)
                moveq   #1,d0
exit:           move.l  (sp)+,a6
                tst.l   d0
                rts