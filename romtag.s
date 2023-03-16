    .include "exec/resident.i"
    .include "lvo/exec_lib.i"

            moveq.l #-1,d0
            rts

Romtag:     dc.w $4AFC
            dc.l Romtag
            dc.l EndSkip
            dc.b RTF_COLDSTART
            dc.b 41
            dc.b 0
            dc.b 0
            dc.l Name
            dc.l Name
            dc.l init
