    include libraries/configvars.i
    include exec/resident.i
    include exec/memory.i
    include lvo/exec_lib.i
    include boards.i
    include alerts.i

    IFND WORDWIDE
DRIVEROFFSET = $2000
BANKOFFSET   = $10000
STRIDE       = 2
    ELSE
DRIVEROFFSET = $1000
BANKOFFSET   = $8000
STRIDE       = 1
    ENDIF

    XDEF STRIDE
    
    IFND WORDWIDE
  incbin "obj/diagnibbles"
    ELSE
  incbin "obj/diagarea-word"
    ENDIF

;; Registers used:
;; A0: BoardBase, A1: ConfigDev, A2: BoardBase, A3: ConfigDev, D2: Rom Alignment
Stage2  move.l  a0,a2                         ; BoardBase
        move.l  a1,a3                         ; ConfigDev
        moveq   #0,d2
        moveq.l #0,d1
        move.w  cd_Rom+er_InitDiagVec(a1),d1  ; Get rom alignment
        cmp.w   #1,d1                         ; AT-Bus with odd address?
        beq     .odd
        cmp.w   #$100,d1                      ; WinUAE AT-Bus with ROM at odd address (despite DiagVec...)
        bne     .even
.odd    addq.l  #1,d2
.even 
        and.w   #$F000,d1                     ; Add any other offset
        add.l   d1,d2
        cmp.l   #(BANKOFFSET*2),cd_BoardSize(a3) ; 128K Board?
        bcs.s   .loaddriver
.loadcdfs
        move.l  a2,a1                         ; Get BoardBase
        add.l   d2,a1
        add.l   #(BANKOFFSET*2)-(STRIDE*4),a1 ; Point to the end of the second bank
        moveq.l #3,d3
.getid  lsl.l   #8,d0                         ; Look for the ROM trailer which should not be on the second bank
        move.b  (a1),d0
        addq.l  #STRIDE,a1
        dbra    d3,.getid
        cmp.l   #'LIDE',d0 
        beq.s   .loaddriver                   ; ROM trailer found. so the banks are mirrored :(
        move.l  #BANKOFFSET,d0                ; Reloc offset to second bank
        bsr     CallReloc                     ; Load CDFS if it is there, ignoring the return code as it might not be there

        IFND WORDWIDE
        ; Load romtags from other banks if supported (currently only RIDE)
        cmp.w   #OAHR_MFR_ID,cd_Rom+er_Manufacturer(a3)
        bne.s   .loaddriver
        cmp.b   #RIDE_PRD_ID,cd_Rom+er_Product(a3)
        bne.s   .loaddriver
        moveq.l #1,d5
        move.b  #$C0,d4             ; Start with Bank #4
        move.l  a2,a4
        add.l   #RIDE_BANKREG,a4
.bank   move.b  (a4),d1
        and.b   #$3F,d1             ; Preserve other bits
        or.b    d4,d1
        move.b  d1,(a4)             ; Select bank, the rom will dissapear from the lower 64K window now
        move.l  #BANKOFFSET,d0      ; So point to the upper 64K window where the ROM is always mapped
        bsr.s   CallReloc           ; Load something if it is there, ignoring the return code as it might not be there
        sub.l   #$40,d4
        dbra    d5,.bank
        and.b   #$3F,(a4)           ; Point back at Bank #0
        add.l   #BANKOFFSET,d2      ; Set Offset to upper window
        ENDIF

.loaddriver
        move.l  #DRIVEROFFSET,d0
        ;; Fall through to CallReloc ;;
        ;; RTS will go back to bootldr ;;

; CallReloc: Add Alignment to Offset and call _relocate
; Inputs: D0: Offset, D2: Alignment, A2: BoardBase
; Returns: 0 on success
CallReloc
        add.l   d2,d0
        move.l  a2,a0
        IFD WORDWIDE
        add.l   d0,a0
        moveq.l #0,d0
        ENDIF
        moveq.l #0,d7           ; Clear error number
        bsr.w   _relocate
        tst.l   d0
        bne.s   InitRT
        moveq.l #-1,d0
        rts

; InitRT: Search for a ROMTAG and InitResident() it
; Inputs: D0: Address to search
; Returns: 0 on success
InitRT  move.l  #$1000,d3            ; Search the first 4096 words for a Romtag
        move.l  d0,a1
.findrt cmpi.w  #RTC_MATCHWORD,(a1)
        bne.s   .not_rt
        cmp.l   RT_MATCHTAG(a1),a1   ; Check that the RT_MATCHTAG is correct
        beq.s   .found
.not_rt addq.l  #2,a1
        dbra    d3,.findrt
        move.l  #AN_STAGE2|AG_NORT,d7   ; Set Alert
        moveq.l #-1,d0
        rts
* Found the driver, initialize it
.found  
        moveq.l  #0,d1
        jsr      _LVOInitResident(A6)
        moveq.l  #0,d0
        rts

  section DEVICE
  incbin "obj/driver"