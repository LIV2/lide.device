;
; Copyright (C) 2025 Matthew Harlum <matt@harlum.net>
; This code is licensed under the GNU General Public License version 2 only.
;
    include lvo/exec_lib.i
    include exec/io.i
    include exec/memory.i
    include exec/resident.i
    include devices/trackdisk.i
    include dos/doshunks.i
    include dos/filehandler.i
    include libraries/expansionbase.i

    section .text,code

;******************************************************************************
;
; LOADSEG.S - Amiga Executable Hunk Loader and Relocator
;
; Complete hunk-based executable loader that reads, processes, and relocates
; Amiga executable files from streaming input sources. Supports all standard
; hunk types, multiple relocation formats, and creates AmigaDOS segment lists.
;
; SUPPORTED HUNK TYPES:
; • HUNK_CODE/DATA: Executable code and initialized data
; • HUNK_BSS: Uninitialized data (zero-filled)
; • HUNK_RELOC32: 32-bit absolute relocations
; • HUNK_RELOC32SHORT: 16-bit word-based relocations
; • HUNK_DREL32/HUNK_RELRELOC32: Data and PC-relative relocations
; • HUNK_SYMBOL: Symbol tables (parsed but not stored)
;
; CALLBACK INTERFACE:
;
; The loader uses a callback-based input system for maximum flexibility.
; The caller provides a read function that supplies 32-bit values on demand:
;
; READ FUNCTION SPECIFICATION:
;   INPUTS:
;     A4.L - Opaque read handle (caller-defined state structure)
;     A6.L - exec.library base (preserved from main call)
;
;   OUTPUTS:
;     D0.L - Next 32-bit value from input stream
;
;   CALLING CONVENTION:
;     - Function called via JSR (A1)
;     - Must preserve all registers except D0/D1/A0/A1
;     - Handle endianness conversion if required
;     - Manage buffering, error handling, and stream positioning
;
; READ HANDLE STRUCTURE:
;   Completely opaque to relocate.S - caller defines contents.
;   Typical implementations include:
;   • File position/offset tracking
;   • Buffer pointers and remaining byte counts
;   • Device I/O request structures
;   • Error state and EOF indicators
;   • Block/sector addressing information
;
; USAGE PATTERN:
;   lea.l   MyReadFunc(pc),a0    ; Read function address
;   move.l  MyHandle,a2          ; Read handle pointer
;   bsr     _loadseg            ; Load and relocate
;   ; D0 now contains BPTR to segment list or 0 on error
;
; SEGLIST ALLOCATION AND SETUP:
;
; Step 1: Memory Allocation (per hunk)
;   Size needed = (hunk_size * 4) + 8 bytes for header
;
;   Allocated Block Layout:
;   +---------------------------------------+
;   | +0: Hunk size (longwords)             | <- AllocMem returns this address
;   | +4: BPTR to next segment              |
;   | +8: Start of hunk data                | <- Stored in hunk list array
;   |     (code/data gets loaded here)      |
;   |     ...                               |
;   +---------------------------------------+
;
; Step 2: Segment Linking
;   The BPTR at +4 is calculated to point to the +4 offset of the NEXT segment:
;
;   Segment 0:            Segment 1:            Segment 2:
;   +-----------------+   +-----------------+   +-----------------+
;   | +0: Size0       |   | +0: Size1       |   | +0: Size2       |
;   | +4: BPTR -------+-->| +4: BPTR -------+-->| +4: 0 (end)     |
;   | +8: Code0       |   | +8: Code1       |   | +8: Code2       |
;   |     ...         |   |     ...         |   |     ...         |
;   +-----------------+   +-----------------+   +-----------------+
;
; Step 3: Return Value Adjustment
;   Internal hunk list points to +8 offsets, but AmigaDOS expects segments
;   to start at +4. The return BPTR is adjusted by subtracting 1:
;
;   return_BPTR = (segment_addr + 8) >> 2 - 1
;
;   When caller converts back: abs_addr = return_BPTR << 2
;   This gives: abs_addr = segment_addr + 4
;
; FINAL SEGLIST FORMAT (AmigaDOS compatible):
;
;   What caller sees at returned BPTR address:
;   +---------------------------------------+
;   | -4: Segment size (longwords)          | <- UnLoadSeg reads this
;   |  0: BPTR to next segment              | <- BPTR points here
;   | +4: Relocated code/data starts here   | <- Execution entry point
;   |     ...                               |
;   +---------------------------------------+
;
; This format is fully compatible with:
; * DOS UnLoadSeg() for memory cleanup
; * DOS Execute() for program execution
; * Manual JSR 4(segment_absolute_addr) for direct calls
;
; EXECUTION:
;   First segment typically contains program entry point.
;   DOS can execute via: Execute(seglist, input, output)
;   Or manually: JSR 4(seglist_absolute_address)
;
; MEMORY OWNERSHIP:
;   Caller responsible for freeing segment list when no longer needed.
;   Use DOS UnLoadSeg() or manual FreeMem() of each segment.
;
;******************************************************************************

;; Local vars in FP
NUM_HUNKS  = -4
FIRST_HUNK = NUM_HUNKS-4
LAST_HUNK  = FIRST_HUNK-4
HUNK_LIST  = LAST_HUNK-4
READ_FUNC  = HUNK_LIST-4
WORD_CACHE = READ_FUNC-4
FLAGS      = WORD_CACHE-2
FP_END     = FLAGS-2

F_WCACHE = 1

    xdef _loadseg

;******************************************************************************
; _loadseg - Load and relocate Amiga executable hunks from storage device
;
; This function reads an Amiga executable file from a storage device and
; performs memory allocation, loading, and relocation of all hunks to create
; a runnable program segment list.
;
; INPUTS:
;   A0.L - Pointer to read function that reads 4 bytes into D0
;   A2.L - Opaque read handle structure used by the read function to
;          maintain state (file position, buffer pointers, device context)
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - BPTR to first segment (success) or 0 (failure)
;
; REGISTER USAGE DURING EXECUTION:
;   D2.L - Loop counter for data copying and relocation operations
;   D4.L - Base address of target hunk during relocations
;   D5.L - Current hunk number being processed (0-based index)
;   D7.L - Error flag (0 = no error, non-zero = error occurred)
;   A2.L - Pointer to allocated hunk list array
;   A3.L - Pointer to current hunk's data area (where code/data loads)
;   A4.L - Copy of read handle (preserved from input A2)
;   A5.L - Frame pointer for accessing local stack variables
;
; DESCRIPTION:
;   Processes the following hunk types:
;   - HUNK_HEADER: File header with hunk count and size info
;   - HUNK_CODE/HUNK_DATA: Executable code and initialized data
;   - HUNK_BSS: Uninitialized data (zero-filled)
;   - HUNK_RELOC32: 32-bit absolute relocations
;   - HUNK_RELOC32SHORT: 16-bit word-based relocations
;   - HUNK_DREL32: 32-bit data relocations
;   - HUNK_RELRELOC32: PC-relative relocations
;   - HUNK_SYMBOL: Symbol table entries (skipped)
;   - HUNK_END: End of hunk marker
;******************************************************************************
_loadseg:      movem.l D2-D7/A2-A5,-(SP)
                link.w  a5,#FP_END
                move.l  #0,HUNK_LIST(a5)
                move.l  #0,WORD_CACHE(a5)
                move.l  a0,READ_FUNC(a5)
                move.w  #0,FLAGS(a5)
                move.l  a2,a4                   // Read Handle
                moveq.l #0,D7                   // No error

.start:         bsr     getLong
                cmp.l   #HUNK_HEADER,D0
                bne     .error
                bsr     getLong                 // Strings
                tst.l   D0
                bne     .error                  // Not supported
                bsr     getLong
                move.l  D0,NUM_HUNKS(A5)
                addi.l  #1,D0
                lsl     #2,D0                   // Allocate Hunk List
                move.l  #MEMF_ANY!MEMF_CLEAR,D1
                jsr     _LVOAllocMem(A6)
                tst.l   D0
                beq     .error
                move.l  D0,A2
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
                beq     .error
                move.l  D0,A0
                move.l  (SP)+,(A0)              // SegList Size
                move.l  D5,D1                   // Hunk number
                lsl.l   #2,D1                   // To HUNK_LIST index
                add.l   #8,D0                   // Skip Seglist Header
                move.l  D0,(A2,D1)              // Store Hunk data location
                add.l   #1,D5
                cmp.l   LAST_HUNK(A5),D5
                ble     .allocate

; Link segments
                moveq.l #0,D5
.linksegs:      move.l  D5,D0                   // Hunk number to HUNK_LIST INDEX
                lsl.l   #2,D0
                move.l  (A2,D0),A0
                move.l  4(A2,D0),D0             // Get successor Address
                lsr.l   #2,D0                   // Convert to BPTR
                move.l  D0,-4(A0)               // Store in seglist
                add.l   #1,D5
                cmp.l   NUM_HUNKS(A5),D5

                blt     .linksegs

                moveq.l #0,D5
.hunkloop:      move.l  D5,D0
                lsl.l   #2,D0
                move.l  (A2,D0),A3              // Load Hunk address
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
.hunk_unknown:  bra     .error
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
                move.l  (A2,D0.L),D4       // Hunk base into D4
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
                move.l  (A2,D0.L),D4       // Hunk base into D4
.doreloc32s:    bsr     getWord
                add.l   D4,(A3,D0.L)       // Add destination hunk base
                dbra    D2,.doreloc32s
                bra     .h_reloc32short
.reloc32salign: btst.b  #F_WCACHE,FLAGS(a5)
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
                move.l  (A2,D0.L),D4
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

.error:         moveq   #-1,D7

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
                bra.s   .exit
.ok:            move.l  D2,D0
                lsr.l   #2,D0
                sub.l   #1,D0
.exit:          unlk    A5
                movem.l (SP)+,D2-D7/A2-A5
                rts

;******************************************************************************
; getLong - Read a 32-bit longword from the input stream
;
; This function calls the user-supplied read function to fetch the next
; 32-bit value from the input stream. The read function is expected to
; handle the actual I/O operation and endian conversion if necessary.
;
; INPUTS:
;   READ_FUNC(A5) - Pointer to user-supplied read function
;   A4.L - Opaque read handle structure passed to read function,
;          contains state information needed for I/O operations
;
; OUTPUTS:
;   D0.L - The 32-bit value read from the stream
;
; NOTES:
;   - Read function receives handle in A4 and returns longword in D0
;   - Read function handles all I/O details and state management
;******************************************************************************
getLong:        move.l  READ_FUNC(a5),a1
                jsr     (a1)
                ;; Returns in D0
                rts

;******************************************************************************
; getWord - Read a 16-bit word from the input stream with caching
;
; This function implements word-level reading by caching longwords and
; returning them as two separate 16-bit words. It uses a flag to track
; whether the cached word contains valid data.
;
; INPUTS:
;   READ_FUNC(A5) - Pointer to user-supplied read function
;   FLAGS(A5) - Flag byte with F_WCACHE bit for cache state
;   WORD_CACHE(A5) - Cached 16-bit word storage
;
; OUTPUTS:
;   D0.L - The 16-bit value read (zero-extended to 32 bits)
;
; ALGORITHM:
;   1. Toggle the F_WCACHE flag bit
;   2. If flag was clear (no cached data):
;      - Read new longword via getLong
;      - Store low word in cache
;      - Return high word in D0
;   3. If flag was set (cached data available):
;      - Return cached low word in D0
;      - Clear cache flag
;
; NOTES:
;   - Assumes big-endian longword format (high word first)
;   - Cache state is maintained across multiple calls
;   - Used primarily for HUNK_RELOC32SHORT and similar word-based hunks
;******************************************************************************
getWord:        bchg.b  #F_WCACHE,FLAGS(a5)
                bne     .cached
                bsr     getLong
                move.w  d0,WORD_CACHE(a5)
                clr.w   d0
                swap    d0
                rts
.cached         clr.l   d0
                move.w  WORD_CACHE(a5),d0
                rts