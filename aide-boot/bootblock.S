    include lvo/exec_lib.i
    include exec/resident.i
    include exec/io.i
    include exec/memory.i
    include devices/trackdisk.i
    include dos/doshunks.i
    include dos/filehandler.i
    include libraries/expansionbase.i

; Custom bootblock code
; A1 = IOREq

;; Local vars in FP
IOREQ      = -4
BUFFER     = -8
NUM_HUNKS  = -12
FIRST_HUNK = -16
LAST_HUNK  = -20
HUNK_LIST  = -24
RESIDENT   = -28

ALERT_NOMEM   = $01DE0001
ALERT_BADHUNK = $01DE0002
ALERT_BADHEAD = $01DE0003
ALERT_IOERROR = $01DE0004
ALERT_NORES   = $01DE0005

; Showtime!
start:          movem.l D2-D7/A2-A4/A6,-(SP)
                move.l  4,A6
                link.w  A5,#-28
                moveq.l #0,D0
                move.l  D0,NUM_HUNKS(A5)
                move.l  D0,FIRST_HUNK(A5)
                move.l  D0,LAST_HUNK(A5)
                move.l  D0,HUNK_LIST(A5)
                move.l  D0,RESIDENT(A5)
                move.l  A1,IOREQ(A5) // Stash ioreq

.allocbuf:      ; Allocate the IO Buffer
                move.l  #ALERT_NOMEM,D7
                move.l  #512,D0
                move.l  #MEMF_CHIP!MEMF_CLEAR,D1
                jsr     _LVOAllocMem(A6)
                move.l  D0,BUFFER(A5)
                tst.l   D0
                beq     error
                bsr     relocate                // Relocate
                move.l  D0,RESIDENT(A5)
                tst.l   D0
                beq     exit
                ; Relocate succeeded, find the Resident and initialize it
                move.l  D0,A1
                move.l  #8192,D2
.findres:       cmp.w   #RTC_MATCHWORD,(A1)
                bne     .notrt
                cmp.l   RT_MATCHTAG(A1),A1
                bne     .notrt
                move.l  D0,D1
                jsr     _LVOInitResident(A6)
                tst.l   D0
                bne     exit
                move.l  #0,RESIDENT(A5)
                bra     exit

.notrt          addq.l  #2,A1
                dbra    D2,.findres
                move.l  #ALERT_NORES,D7

error:          jsr     _LVOAlert(A6)
exit:
                bsr     motorOff
                move.l  BUFFER(A5),D0           // Free the buffer if allocated
                beq     .nobuf
                move.l  D0,A1
                move.l  #512,D0
                jsr     _LVOFreeMem(A6)
.nobuf:
                move.l  HUNK_LIST(A5),D0        // Free the Hunk List if allocated
                beq     .nohunklist
                move.l  D0,A1
                move.l  NUM_HUNKS(A5),D0
                lsl.l   #2,D0
                jsr     _LVOFreeMem(A6)
.nohunklist:
                move.l  RESIDENT(A5),D0
                unlk    A5
                movem.l (SP)+,D2-D7/A2-A4/A6
                tst.l   D0                      // Did the driver load?
                bne     .boot
                moveq.l #-1,D0                  // Signal failure to strap
                rts
.boot:          moveq.l #0,D0
                lea.l   bootLide(PC),A0
                rts

; D3 = Remaining bytes, D6 = Sector number, D5 = Curr Hunk
; A4 = Hunk list, A3 = Curr hunk Addr, A2 Pointer into disk buffer
relocate:       movem.l D2-D7/A2-A4,-(SP)
                moveq.l #0,D7                   // No error
                moveq.l #0,D3                   // Buffer is empty
                moveq.l #2,D6                   // Start with Sector #2
.start:         bsr     getLong
                cmp.l   #HUNK_HEADER,D0
                bne     .relocbadhdr
                bsr     getLong                 // Strings
                tst.l   D0
                bne     .relocbadhunk           // Not supported
                bsr     getLong
                move.l  D0,NUM_HUNKS(A5)
                addi.l  #1,D0
                lsl     #2,D0                   // Allocate Hunk List
                move.l  #MEMF_ANY!MEMF_CLEAR,D1
                jsr     _LVOAllocMem(A6)
                tst.l   D0
                beq     .relocnomem
                move.l  D0,A4
                move.l  D0,HUNK_LIST(A5)        // Hunk list
                bsr     getLong
                move.l  D0,FIRST_HUNK(A5)
                bsr     getLong
                move.l  D0,LAST_HUNK(A5)
; Allocate memory for all Hunks
                moveq.l #0,D5
.allocate:      bsr     getLong
                move.l  D0,-(SP)                // Save for later
                lsl.l   #2,D0
                add.l   #8,D0                   // Add space for SegList
                move.l  #MEMF_ANY!MEMF_CLEAR,D1
                jsr     _LVOAllocMem(A6)
                tst.l   D0
                beq     .relocnomem
                move.l  D0,A0
                move.l  (SP)+,(A0)              // SegList Size
                move    D5,D1                   // Hunk number
                lsl.l   #2,D1                   // To HUNK_LIST index
                add.l   #8,D0                   // Skip Seglist Header
                move.l  D0,(A4,D1)              // Store Hunk data location
                add.l   #1,D5
                cmp.l   LAST_HUNK(A5),D5
                ble     .allocate
; Link segments
                moveq.l #0,D5
.linksegs:      move.l  D5,D0                   // Hunk number to HUNK_LIST INDEX
                lsl.l   #2,D0
                move.l  (A4,D0),A0
                move.l  4(A4,D0),D0             // Get successor Address
                lsr.l   #2,D0                   // Convert to BPTR
                move.l  D0,-4(A0)               // Store in seglist
                add.l   #1,D5
                cmp.l   NUM_HUNKS(A5),D5
                blt     .linksegs
                moveq.l #0,D5
.hunkloop:      move.l  D5,D0
                lsl.l   #2,D0
                move.l  (A4,D0),A3              // Load Hunk address
                bsr     getLong
                cmp.l   #HUNK_CODE,D0
                beq     .h_code
                cmp.l   #HUNK_DATA,D0
                beq     .h_data
                cmp.l   #HUNK_BSS,D0
                beq     .h_bss
                cmp.l   #HUNK_RELOC32,D0
                beq     .h_reloc32
                cmp.l   #HUNK_RELOC32SHORT,D0
                beq     .h_reloc32short
                cmp.l   #HUNK_DREL32,D0
                beq     .h_drel32
                cmp.l   #HUNK_SYMBOL,D0
                beq     .h_symbol
                cmp.l   #HUNK_END,D0
                beq     .hunk_end
                cmp.l   #HUNK_RELRELOC32,D0
                beq     .h_relreloc32
.hunk_unknown:  move.l  #ALERT_BADHUNK,D7
                bra     .relocbadhunk
.h_data:
.h_code:        
                bsr     getLong            // Longword count
                move.l  D0,D2
                subi.l  #1,D2
.copyloop:      bsr     getLong
                move.l  D0,(A3)+
                dbra    D2,.copyloop
                bra     .hunkloop

.h_bss:         bsr     getLong            // BSS already allocated and zeroed
                bra     .hunkloop

.h_reloc32:     bsr     getLong            // Number of offsets to relocate
                tst.l   D0                 // None?
                beq     .hunkloop          // Exit
                move.l  D0,D2
                sub.l   #1,D2
                bsr     getLong            // Get hunk number the offsets point to
                lsl.l   #2,D0
                move.l  (A4,D0.L),D4       // Hunk base into D4
.doreloc32:     bsr     getLong
                add.l   D4,(A3,D0.L)       // Add destination hunk base
                dbra    D2,.doreloc32
                bra     .h_reloc32

.h_drel32:
.h_reloc32short:
                bsr     getWord            // Number of offsets to relocate
                tst.l   D0                 // None?
                beq     .reloc32salign     // Exit
                move.l  D0,D2
                sub.l   #1,D2
                bsr     getWord            // Get hunk number the offsets point to
                lsl.l   #2,D0
                move.l  (A4,D0.L),D4       // Hunk base into D4
.doreloc32s:    bsr     getWord
                add.l   D4,(A3,D0.L)       // Add destination hunk base
                dbra    D2,.doreloc32s
                bra     .h_reloc32short
.reloc32salign: move.l  A2,D0              // Ensure the buf pointer is still long-aligned
                andi.l  #3,D0
                beq     .hunkloop
                bsr     getWord            // Get the padding word
                bra     .hunkloop

.h_relreloc32:  bsr     getWord            // Number of offsets
                tst.l   D0
                beq     .reloc32salign
                move.l  D0,D2
                sub.l   #1,D2
                bsr     getWord            // Get number the offsets point to
                lsl.l   #2,D0
                move.l  (A4,D0.L),D4
.dorelreloc32:  bsr     getWord
                move.l  D4,D1              // Delta = Dest hunk - Current hunk - Offset
                sub.l   A3,D1
                sub.l   D0,D1
                add.l   D1,(A3,D0.L)
                dbra    D2,.dorelreloc32
                bra     .h_relreloc32

.h_symbol:      bsr     getLong            // Symbol type + name length
                andi.l  #$FFFFFF,D0        // Mask off type
                beq     .hunkloop
                move.l  D0,D2
                sub.l   #1,D2
.symbol_loop:   bsr     getLong            // Consume symbol name
                dbra    D2,.symbol_loop
                bsr     getLong            // Consume symbol value
                bra     .h_symbol

.hunk_end:      cmp.l   LAST_HUNK(A5),D5
                beq     .done
                addi.l  #1,D5
                bra     .hunkloop

.relocbadhdr    move.l  #ALERT_BADHEAD,D7
                bra     .error
.relocnomem:    move.l  #ALERT_NOMEM,D7
                bra     .error
.relocbadhunk:  move.l  #ALERT_BADHUNK,D7
                bra     .error
.error:         jsr     _LVOAlert(A6)
.done:          move.l  HUNK_LIST(A5),D0
                tst.l   D0
                beq     .nohunklist
                move.l  D0,A1
                move.l  (A1),D2             // Get address of 1st hunk
                move.l  NUM_HUNKS(A5),D0
                lsl.l   #2,D0
                jsr     _LVOFreeMem(A6)
                move.l  #0,NUM_HUNKS(A5)
.nohunklist     tst.l   D7
                beq     .ok
                moveq.l #0,D2
.ok:            move.l  D2,D0
                movem.l (SP)+,D2-D7/A2-A4
                rts

getLong:        tst.l   D3
                bne     .ok
                bsr     fill
.ok             move.l  (A2)+,D0
                sub.l   #4,D3
                rts

getWord:        tst.l   D3
                bne     .ok
                bsr     fill
.ok:            clr.l   D0
                move.w  (A2)+,D0
                subi.l  #2,D3
                rts

fill:  
                move.l  BUFFER(A5),A2   // Point A2 to start of Buffer again
                bsr     getSector
                add.l   #1,D6           // Increment Sector counter
                move.l  #512,D3         // Reset Bytes Remaining
                rts

; D6 = Sector
getSector:
                move.l  IOREQ(A5),A1
                move.w  #CMD_READ,IO_COMMAND(A1)
                move.l  #512,IO_LENGTH(A1)
                move.l  BUFFER(A5),IO_DATA(A1)
                move.l  D6,D0
                lsl.l   #8,D0                   // Sector number to byte offset
                add.l   D0,D0
                move.l  D0,IO_OFFSET(A1)
                jmp     _LVODoIO(A6)            // Implicit RTS

motorOff:       move.l  IOREQ(A5),A1
                move.w  #TD_MOTOR,IO_COMMAND(A1)
                move.l  #0,IO_LENGTH(A1)
                jmp     _LVODoIO(A6)

; Boot time
;
; Here we will move the floppy boot node down in the list and call DOS
; The next highest boot priority should be booted (Hopefully the HDD!)
bootLide:       lea     (expansion,PC),A1
                moveq.l #0,D0
                jsr     _LVOOpenLibrary(A6)
                tst.l   D0
                beq     .error
                move.l  D0,A2
                cmp.w   #37,LIB_VERSION(A2)
                bcs     .fixbootpri
                bset.b  #EBB_SILENTSTART,eb_Flags(A2)
.fixbootpri:    ; Move the floppy to a lower bootPri than the HDD
                ; This will cause the system to boot from the HDD when we start DOS
                jsr     _LVOForbid(A6)
                lea.l   eb_MountList(A2),A0
                jsr     _LVORemHead(A6)         // Pop our BootNode from the mountlist
                move.l  D0,A1
                move.b  #-127,LN_PRI(A1)        // Lower the bootNode priority
                
                ; Change bootPri to -127
                move.l  bn_DeviceNode(A1),A3    
                move.l  dn_Startup(A3),D2       // FileSystemStartupMessage
                lsl.l   #2,D2                   // Convert from BPTR
                move.l  D2,A3
                move.l  fssm_Environ(A3),D2     // DosEnvVec
                lsl.l   #2,D2                   // Convert from BPTR
                move.l  D2,A3
                move.l  #-127,de_BootPri(A3)

                lea.l   eb_MountList(A2),A0
                jsr     _LVOEnqueue(A6)         // Enqueue our boot node
                jsr     _LVOPermit(A6)

.boot:          lea     (dos_name,PC),A1
                moveq.l #0,D0
                jsr     _LVOFindResident(A6)
                tst.l   D0
                beq     .error
                move.l  D0,A0
                move.l  RT_INIT(A0),A0          // Start DOS
                jsr     (A0)
.error:         moveq.l #-1,D0
                rts

expansion:  dc.b "expansion.library",0
dos_name:   dc.b "dos.library",0