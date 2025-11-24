;
; Copyright (C) 2025 Matthew Harlum <matt@harlum.net>
; This code is licensed under the GNU General Public License version 2 only.
;
    include exec/devices.i
    include exec/io.i
    include exec/nodes.i
    include exec/memory.i
    include exec/ports.i
    include exec/resident.i
    include exec/types.i
    include lvo/exec_lib.i
    include lvo/expansion_lib.i
    include dos/filehandler.i
    include devices/hardblocks.i
    include devices/trackdisk.i
    include libraries/expansion.i
    include libraries/expansionbase.i
    include resources/filesysres.i

    section .text,code

; Frame pointer vars
MB_CD           EQU -4                      ; ConfigDev
MB_DEVICE       EQU MB_CD-4                 ; Device Name
MB_IOREQ        EQU MB_DEVICE-IOSTD_SIZE    ; IORequest structure (48 bytes)
MB_MP           EQU MB_IOREQ-MP_SIZE        ; MsgPort structure (34 bytes)
MB_BLOCKSHIFT   EQU MB_MP-2
MB_BLOCKSIZE    EQU MB_BLOCKSHIFT-4         ; Blocksize
MB_FSHB         EQU MB_BLOCKSIZE-4          ; FileSysHeaderBlock
MB_FSRES        EQU MB_FSHB-4
MB_SIZE         EQU -(MB_FSRES)             ; Total frame size


    STRUCT READHANDLE, 0
        APTR  MOUNTBASE ; Pointer to mount base frame (A5) for accessing MB_BLOCKSIZE
        APTR  RH_BUF    ; Pointer to allocated buffer for block data
        APTR  RH_IOREQ  ; IORequest structure for disk I/O operations
        ULONG RH_SECTOR ; Current/next LSEG block number on disk
        ULONG RH_INDEX  ; Byte offset within current block buffer
        ULONG RH_BYTES  ; Bytes remaining in current block
        LABEL RH_SIZE


    XDEF _mount

;******************************************************************************
; _mount - Mount all partitions on a specified device
;
; Main entry point for the mounting system. This function scans all units
; on the specified device, looking for Rigid Disk Blocks (RDB) that contain
; partition information. For each unit found:
; 1. Opens the device unit
; 2. Searches for RDB in the first 16 blocks
; 3. Processes all partitions defined in the RDB
; 4. Creates DOS nodes and adds them to the system mount list
;
; If no ConfigDev is provided (A1=NULL), creates a fake ConfigDev with
; diagnostic boot area to satisfy strap requirements.
;
; The function handles both Kickstart 1.3 and 2.x+ mounting protocols.
;
; INPUTS:
;   A0.L - Device name (null-terminated string, e.g., "scsi.device")
;   A1.L - ConfigDev structure pointer (from expansion board, NULL to create fake)
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - Status code (0 = success, negative = error)
;
; REGISTER USAGE DURING EXECUTION:
;   D2.L - Current unit number being processed
;   D3.L - Return code accumulator
;   A2.L - Device name string pointer
;   A3.L - ConfigDev structure pointer
;   A4.L - ExpansionBase pointer (when expansion.library is open)
;   A5.L - Frame pointer for local variable access
;   A6.L - SysBase (exec.library base)
;
; STACK FRAME LAYOUT:
;   MB_CD(-4):          ConfigDev pointer
;   MB_DEVICE(-8):      Device name pointer
;   MB_IOREQ(-56):      IOStdReq structure for device I/O
;   MB_MP(-90):         MsgPort structure for async I/O
;   MB_BLOCKSHIFT(-92): Block size shift value for address calculations
;   MB_BLOCKSIZE(-96):  Device block size in bytes
;   MB_FSHB(-100):      First filesystem header block pointer
;   MB_FSRES(-104):     FileSystem.resource pointer
;******************************************************************************
_mount:         movem.l d2-d7/a2-a6,-(sp)
                move.l  a0,a2
                move.l  a1,a3
                move.l  #0,a4                               ; Clear ExpansionBase
                moveq   #-3,d3                              ; Default return code -1
                move.l  4,a6                                ; SysBase

                link.w  a5,#-MB_SIZE
                move.l  #((MB_SIZE-4)/4)-1,d0               ; Zero out MountBase
                lea.l   MB_CD(a5),a0
.zero           clr.l   -(a0)
                dbra    d0,.zero

                move.l  a2,MB_DEVICE(a5)                    ; Save Device name

                ; Initialize MsgPort
                lea.l   MB_MP(a5),a0
                bsr.w   InitMP
                tst.b   d0
                bmi.s   .end

                ; Init IOReq
                lea.l   MB_IOREQ(a5),a0
                lea.l   MB_MP(a5),a1
                bsr.w   InitIO

                ; Open Expansion
                lea.l   Expansion(pc),a1
                moveq   #0,d0
                jsr     _LVOOpenLibrary(a6)
                tst.l   d0
                beq.s   .cleanup_mp                         ; If failed cleam up and exit
                move.l  d0,a4                               ; ExpansionBase to a4

                move.l  a3,d0                               ; Was a configDev provided?
                beq     .noconfigdev
                move.l  a3,MB_CD(a5)
                bra     .start

.noconfigdev:   bsr     FakeConfigDev                       ; No configDev, create a fake one
                move.l  a1,MB_CD(a5)

.start:         moveq   #-1,d2                              ; Start scanning units
.unitloop:      add.l   #1,d2                               ; Start at unit 0
                ; Open the device
                move.l  a2,a0                               ; Device name
                move.l  d2,d0                               ; Unit
                lea.l   MB_IOREQ(a5),a1                     ; IOStdReq
                clr.l   d1                                  ; Flags
                jsr     _LVOOpenDevice(a6)
                tst.b   d0
                beq.s   .OpenOK
                cmp.b   #TDERR_BadUnitNum,d0                ; No unit here but there are others
                beq.s   .unitloop
                bra.s   .cleanup_exp                        ; Got some other error, exit now

.OpenOK         move.l  d2,d0                               ; Unit number
                bsr.w   ProcessRDB
                move.l  d0,d3                               ; Stash the return code

                ; Close the device
                lea.l   MB_IOREQ(a5),a1
                jsr     _LVOCloseDevice(a6)
                bra.s   .unitloop

.cleanup_exp:   move.l  a4,d0
                tst.l   d0
                beq.s   .cleanup_mp
                move.l  a4,a1
                jsr     _LVOCloseLibrary(a6)

.cleanup_mp:    move.b  MB_MP+MP_SIGBIT(a5),d0
                tst.b   d0
                bmi.s   .end
                jsr     _LVOFreeSignal(a6)

.end:           move.l  d3,d0
                unlk a5
                movem.l (sp)+,d2-d7/a2-a6
                rts

;******************************************************************************
; FakeConfigDev - Create synthetic ConfigDev with diagnostic boot area
;
; Creates a fake ConfigDev structure for devices without autoconfig ROMs.
; Required because strap needs a boot point provided via the ConfigDev.
; Allocates ConfigDev and diagnostic area, copies boot code that initializes
; DOS via resident tag lookup, and configures ROM structure.
;
; INPUTS:
;   A4.L - expansion.library base
;   A6.L - exec.library base
;
; OUTPUTS:
;   A1.L - ConfigDev pointer (NULL on failure)
;******************************************************************************
FakeConfigDev:  movem.l a2-a3,-(sp)
                exg     a4,a6                               ; ExpansionBase into A6
                jsr     _LVOAllocConfigDev(a6)              ; Get a new ConfigDev struct
                exg     a4,a6                               ; Restore SysBase
                tst.l   d0
                beq.s   .exit
                move.l  d0,a2

                move.l  #DiagEnd-DiagStart,d0               ; Allocate space for DiagArea
                move.l  #MEMF_PUBLIC+MEMF_CLEAR,d1
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq.s   .enomem

                move.l  d0,a3
                move.l  d0,a1                               ; Copy DiagStart to DiagArea
                lea.l   DiagStart(pc),a0
                move.l  #DiagEnd-DiagStart,d0
                jsr     _LVOCopyMem(a6)

                move.b  #ERTF_DIAGVALID,cd_Rom+er_Type(a2)
                move.l  a3,cd_Rom+er_Reserved0c(a2)         ; This field used by Expansion to find the DiagArea

                move.l  a2,a1

.exit:          movem.l (sp)+,a2-a3
                rts

.enomem:        exg     a4,a6                               ; ExpansionBase into A6
                move.l  a2,a0
                jsr     _LVOFreeConfigDev(a6)
                exg     a4,a6                               ; Restore SysBase
                move.l  #0,a1
                bra.s   .exit

DiagStart:      dc.b    DAC_CONFIGTIME      ; da_Config
                dc.b    0                   ; da_Flags
                dc.w    0                   ; da_Size
                dc.w    0                   ; da_DiagPoint
                dc.w    BootEntry-DiagStart ; da_BootPoint
                dc.w    0                   ; da_Name
                dc.w    0                   ; da_Reserved01
                dc.w    0                   ; da_Reserved02

BootEntry:      lea     DosName(PC),a1          ; 'dos.library',0
                jsr     _LVOFindResident(a6)    ; find the DOS resident tag
                tst.l   d0
                beq.s   .BadBoot
                move.l  d0,a0                   ; in order to bootstrap
                move.l  RT_INIT(A0),a0          ; set vector to DOS INIT
                jsr     (a0)                    ; and initialize DOS
.BadBoot        rts

DosName:        DC.B "dos.library",0
DiagEnd:

;******************************************************************************
; GetSectorSize - Determine device sector size and calculate shift value
;
; Queries the device for its geometry information to determine the sector
; size and calculates the corresponding shift value for efficient address
; calculations. This function:
;
; 1. Issues TD_GETGEOMETRY command to the device
; 2. Extracts the sector size from the geometry structure
; 3. Calculates the shift value (log2 of sector size)
; 4. Stores both values in the stack frame for later use
;
; The shift value is used to convert block numbers to byte offsets
; efficiently using left shift instead of multiplication.
;
; INPUTS:
;   A5.L - Frame pointer for accessing local variables
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - Sector size in bytes (0 on failure)
;
; SIDE EFFECTS:
;   Updates MB_BLOCKSIZE(A5) with sector size
;   Updates MB_BLOCKSHIFT(A5) with shift value (stored as word)
;******************************************************************************
GetSectorSize:  lea.l   MB_IOREQ(a5),a1
                move.w  #TD_GETGEOMETRY,IO_COMMAND(a1)
                move.l  #dg_SIZEOF,IO_LENGTH(a1)
                sub.l   #dg_SIZEOF,sp                   ; Allocate Geometry buffer from stack
                move.l  sp,IO_DATA(a1)                  ;
                jsr     _LVODoIO(a6)
                tst.l   d0
                bne     .error
                move.l  dg_SectorSize(sp),d0
                move.l  d0,MB_BLOCKSIZE(a5)
                ; Get BlockShift value
                moveq   #-1,d1
.loop           addq.l  #1,d1
                lsr.l   d0
                bne.s   .loop
                move.w  d1,MB_BLOCKSHIFT(a5)
                move.l  dg_SectorSize(sp),d0                
.exit:          add.l   #dg_SIZEOF,sp
                rts
.error:         moveq   #0,d0
                bra     .exit

;******************************************************************************
; InitMP - Initialize a message port structure
;
; Sets up a message port for asynchronous I/O operations. The message port
; allows the system to signal our task when I/O operations complete.
; Steps performed:
; 1. Initialize the message list as empty
; 2. Set port type and flags for signal-based notification
; 3. Set the signal task to the current task
; 4. Allocate a signal bit for notifications
;
; If signal allocation fails, the function cleans up and returns an error.
;
; INPUTS:
;   A0.L - Message port structure to initialize
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.B - Signal number allocated (negative on failure)
;******************************************************************************
InitMP:
                move.l  a2,-(sp)
                move.l  a0,a2

                move.b  #-1,MP_SIGBIT(a2)                   ; Safe default -1

                lea.l   MP_MSGLIST(a2),a0
                NEWLIST a0
                move.b  #NT_MSGPORT,LN_TYPE(a2)
                move.b  #PA_SIGNAL,MP_FLAGS(a2)

                sub.l   a1,a1                              ; Set mp_Sigtask to our task
                jsr     _LVOFindTask(a6)
                beq.s   .fail
                move.l  d0,MP_SIGTASK(a2)

                move.b  #-1,d0                              ; Allocate next free signal
                jsr     _LVOAllocSignal(a6)
                move.b  d0,MP_SIGBIT(a2)
                tst.b   d0
                bpl.s   .ok
.fail:          moveq   #-1,d0
.ok:            move.l  (sp)+,a2
                rts

;******************************************************************************
; InitIO - Initialize an I/O request structure
;
; Prepares an IORequest structure for use with device I/O operations.
; This is a simple setup function that configures the basic message
; fields required for all I/O operations:
; - Sets the node type to identify this as a message
; - Links the reply port for asynchronous completion notification
; - Sets the message length to the standard IOStdReq size
;
; INPUTS:
;   A0.L - IORequest structure to initialize
;   A1.L - Message port for replies
;
; OUTPUTS:
;   None
;******************************************************************************
InitIO:         move.b  #NT_MESSAGE,LN_TYPE(a0)
                move.l  a1,MN_REPLYPORT(a0)
                move.w  #IOSTD_SIZE,MN_LENGTH(a0)
                rts

;******************************************************************************
; GetBlock - Read and validate a disk block with variable block size support
;
; Performs low-level disk block reading with optional validation and
; support for variable block sizes. This function handles the conversion
; from block numbers to byte offsets using the device's actual block size
; and can validate both block type and checksum:
;
; 1. Converts block number to byte offset using MB_BLOCKSHIFT word value
; 2. Issues synchronous read command using actual device block size
; 3. If block type specified, verifies first long matches expected type
; 4. If block type specified, performs standard Amiga checksum validation
;
; The checksum algorithm sums all longs in the block; result should be 0.
;
; INPUTS:
;   D0.L - Block number to read
;   D1.L - Expected block type ('RDSK', 'PART', etc.) or 0 to skip check
;   A0.L - Buffer to read data into (must be at least block size bytes)
;   A1.L - IORequest structure to use for the operation
;   A5.L - Frame pointer for accessing block size information
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - 0 on success, non-zero on error
;
; ALGORITHM:
;   1. Convert block number to byte offset using shift operation
;   2. Set up IORequest for CMD_READ with device block size
;   3. Perform synchronous I/O via DoIO
;   4. If block type specified, verify header matches
;   5. If block type specified, calculate and verify checksum
;   6. Return 0 for success, -1 for any error
;******************************************************************************
GetBlock:       movem.l a2-a3/d2-d3,-(sp)
                move.l  a0,a2                    ; Buffer
                move.l  a1,a3                    ; IOReq
                move.l  d0,d2                    ; BlockNum
                move.l  d1,d3                    ; BlockType

                move.w  #CMD_READ,IO_COMMAND(a1)
                move.l  MB_BLOCKSIZE(a5),IO_LENGTH(a1)
                clr.l   d0
                move.w  MB_BLOCKSHIFT(a5),d0
                lsl.l   d0,d2                               ; Sector to Byte offset
                move.l  d2,IO_OFFSET(a1)
                move.l  a2,IO_DATA(a1)
                jsr     _LVODoIO(a6)
                tst.b   d0
                bne.s   .err

                tst.l   d3                                  ; Looking for a particular block type?
                beq.s   .end                                ; no
                cmp.l   (a2),d3                             ; Block type match?
                bne.s   .err
                moveq   #0,d0
                move.l  4(a2),d1                            ; SummedLongs
                beq.s   .end
                subq.l  #1,d1
.cloop:         add.l   (a2)+,d0                            ; Do the checksum
                dbra    d1,.cloop
                tst.l   d0                                  ; d0 = 0 if checksum correct, return 0
                bne.s   .err
.end            movem.l (sp)+,a2-a3/d2-d3
                rts

.err            moveq   #-1,d0
                bra.s   .end

;******************************************************************************
; ProcessRDB - Process the Rigid Disk Block on a unit
;
; Locates and processes the Rigid Disk Block (RDB) on a disk unit.
; The RDB contains the partition table and filesystem information for
; the disk. This function:
;
; 1. Determines the device's block size using GetSectorSize
; 2. Allocates a buffer sized to match the device's block size
; 3. Searches blocks 0-15 for a valid RDB (block type 'RDSK')
; 4. If found, extracts the filesystem header list pointer
; 5. Calls ProcessPart to handle all partitions on the disk
; 6. Cleans up the allocated buffer
;
; The RDB_LOCATION_LIMIT constant defines how many blocks to search.
;
; INPUTS:
;   D0.L - Unit number to process
;   A5.L - Frame pointer for accessing local variables
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - 0 on success, negative on error
;
; REGISTER USAGE:
;   D2.L - Loop counter for RDB search (counts down from RDB_LOCATION_LIMIT)
;   D3.L - Current block number being checked
;   D4.L - Unit number (preserved from input)
;   A2.L - Pointer to allocated block-sized RDB buffer
;******************************************************************************
ProcessRDB:     movem.l d2-d4/a2,-(sp)
                move.l  d0,d4                               ; Unit number

                bsr     GetSectorSize                       ; Get SectorSize into d0
                tst.l   d0
                beq.s   .enomem

                ; move.l  #512,d0                             ; Allocate buffer for RDB block
                move.l  #MEMF_ANY+MEMF_CLEAR,D1
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq.s   .enomem

                move.l  d0,a2                               ; RDB Buffer
                move.l  #RDB_LOCATION_LIMIT,d2
                moveq   #-1,d3                              ; Start searching for RDB at block 0

.loop:          addq.l  #1,d3
                lea.l   MB_IOREQ(a5),a1
                move.l  a2,a0                               ; Buffer
                move.l  #'RDSK',d1                          ; Block type
                move.l  d3,d0                               ; Sector
                bsr.w   GetBlock
                tst.b   d0
                dbeq    d2,.loop                            ; Not found, search up to RDB_LOCATION_LIMIT
                tst.b   d0
                bne.s   .skippart                           ; Failed to find any RDB, skip this unit
                move.l  rdb_FileSysHeaderList(a2),MB_FSHB(a5)
                bsr.s   ProcessPart

.skippart:      move.l  a2,a1
                move.l  MB_BLOCKSIZE(a5),d0
                jsr     _LVOFreeMem(a6)
                moveq   #0,d0
.end:           movem.l (sp)+,d2-d4/a2
                rts
.enomem         moveq.l #-1,d0
                bra.s   .end

;******************************************************************************
; ProcessPart - Process all partition blocks on a disk
;
; Walks through the linked list of partition blocks and mounts each
; valid partition. For each partition found:
;
; 1. Reads the partition block ('PART') from disk using device block size
; 2. Checks if NOMOUNT flag is set (skip if so)
; 3. Extracts and validates the partition name (max 30 chars)
; 4. Checks for duplicate names and makes unique if needed
; 5. Creates a parameter packet for MakeDosNode
; 6. Calls MakeDosNode to create the DOS device node
; 7. Calls MountPart to add the partition to the mount list
;
; The function handles both bootable and non-bootable partitions and
; now uses the device's actual block size for memory allocation.
;
; INPUTS:
;   A2.L - RDB buffer containing partition list head pointer
;   D4.L - Unit number (passed from ProcessRDB)
;   A5.L - Frame pointer for accessing local variables
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - 0 on success, negative on error
;
; REGISTER USAGE:
;   D2.L - Current partition block number (follows linked list)
;   D4.L - DosNode pointer (result from MakeDosNode)
;   D5.L - Unit number (preserved from ProcessRDB)
;   A3.L - Pointer to allocated block-sized partition buffer
;
; PARAMETER PACKET FORMAT:
;   The function constructs a parameter packet in the reserved area
;   before pb_Environment by overwriting 16 bytes of reserved space:
;   [0]: Pointer to drive name (C string)
;   [4]: Pointer to device name
;   [8]: Unit number
;   [12]: Flags (always 0)
;******************************************************************************
ProcessPart:    movem.l a2-a3/d2-d5,-(sp)
                move.l  d4,d5                               ; Unit number
                move.l  MB_BLOCKSIZE(a5),d0                             ; Allocate buffer for PartBlocks
                move.l  #MEMF_ANY+MEMF_CLEAR,D1
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq.w   .enomem
                move.l  d0,a3                               ; PART buffer
                move.l  rdb_PartitionList(a2),d2
.partloop:      cmp.l   #-1,d2                              ; End of the list?
                beq.w   .endpart

                move.l  d2,d0                               ; Get the next PartBlock
                move.l  #'PART',d1                          ; Block type
                lea.l   MB_IOREQ(a5),a1
                move.l  a3,a0                               ; Buffer
                bsr.w   GetBlock
                tst.l   d0
                bne.s   .freemem

                btst.b  #PBFB_NOMOUNT,pb_Flags+3(a3)        ; Don't mount if this is set
                bne.s   .nextpart

                ; got a block, do something with it
                lea.l   pb_Environment-16(a3),a0            ; Construct a ParamPacket by overwriting the reserved memory before pb_Environment
                lea.l   pb_DriveName(a3),a1                 ; Get Drivename (BSTR)
                move.b  (a1),d0
                cmp.b   #30,d0                              ; Truncate the partition name if longer than 30 chars
                blo.s   .notrunc
                move.b  #30,(a1)

.notrunc:       bsr.w   CheckDupe                           ; Check for other partition(s) with the same name, make the name unique if so
                moveq   #0,d0
                move.b  (a1)+,d0                            ; Convert to C str
                move.b  #0,0(a1,d0.w)                       ; Null Terminate
                move.l  a1,(a0)                             ; Set drive name in param packet
                move.l  MB_DEVICE(a5),4(a0)                 ; Set Device name
                move.l  d5,8(a0)                            ; Set the unit number
                move.l  #0,12(a0)                           ; Set flags

                exg.l   a4,a6                               ; ExpansionBase into A6
                jsr     _LVOMakeDosNode(a6)
                exg.l   a4,a6                               ; Restore SysBase
                move.l  d0,d4                               ; DosNode in d4
                tst.l   d0                                  ; If MakeDosNode failed then exit
                beq.s   .error
                bsr.s   MountPart                           ; Mount the partition now

.nextpart:      move.l  pb_Next(a3),d2
                bra.s   .partloop

.error:         moveq.l #-1,d2
                bra.s   .freemem

.endpart:       moveq   #0,d2
.freemem:       move.l  MB_BLOCKSIZE(a5),d0                             ; Free the PART buffer
                move.l  a3,a1
                jsr     _LVOFreeMem(a6)
.end:           move.l  d2,d0
                movem.l (sp)+,a2-a3/d2-d5
                rts

.enomem:        moveq   #-1,d2
                bra.s   .end

;******************************************************************************
; MountPart - Mount a single partition
;
; Completes the mounting process for a single partition by:
;
; 1. Looking up the filesystem handler using the partition's DOS type
; 2. If found, patching filesystem-specific fields into the DOS node
; 3. Adding the partition to the appropriate system mount list
;
; The function handles different Kickstart versions:
; - Kickstart 2.x+: Uses AddBootNode/AddDosNode expansion calls
; - Kickstart 1.3: Manually creates BootNode and adds to mount list
;
; Bootable partitions are added with their boot priority, while
; non-bootable partitions get priority -128.
;
; INPUTS:
;   A3.L - Partition block buffer containing partition info
;   D4.L - DosNode pointer (created by MakeDosNode)
;   A4.L - Pointer to expansion.library base
;   A5.L - Frame pointer for accessing local variables
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - 0 on success, negative on error
;
; FILESYSTEM PATCHING:
;   Uses fse_PatchFlags to determine which fields to copy from FileSysEntry
;   to DosNode. Each bit in PatchFlags corresponds to a field offset:
;   - Bit 0: dn_Type (filesystem type)
;   - Bit 1: dn_Task (filesystem task)
;   - Bit 2: dn_Lock (filesystem lock)
;   - Etc. (implementation dependent)
;******************************************************************************
MountPart:      move.l  pb_Environment+de_DosType(a3),d0
                bsr.w   FindFS                              ; Look for the FS in FileSystem.resource
                tst.l   d0
                beq.s   .nofs                               ; If not found, leave these fields blank for the ROM FS to be used
                ; Patch in the FS
                ; d0 = FSE, d4 = DosNode
.PatchFlags:    move.l  d0,a0                               ; FileSysEntry from FileSystem.resource
                move.l  d4,a1                               ; Our DosNode
                move.l  fse_PatchFlags(a0),d0
                lea.l   fse_Type(a0),a0
                lea.l   dn_Type(a1),a1
                moveq   #0,d1

.patch:         bclr.l   d1,d0                              ; If the bit was set in PatchFlags
                beq.s   .no
                move.l  (a0),(a1)                           ; Copy the field from the FileSysEntry to the DosNode

.no:            addq.b  #1,d1
                add.l   #4,a0
                add.l   #4,a1
                tst.l   d0                                  ; Exit when all PatchFlags are done
                bne.s   .patch

.nofs:          cmp.w   #36,LIB_VERSION(a4)
                blo.s   .MountKick1_3                       ; For Kick 1.3 mount the old fashioned way

.MountKick2_x:  exg.l   a4,a6                               ; Expansionbase into A6
                move.l  pb_Environment+de_BootPri(a3),d0    ; Boot priority
                move.l  d4,a0                               ; DosNode
                move.l  #ADNF_STARTPROC,d1                  ; Flags
                move.l  MB_CD(a5),a1                        ; ConfigDev
                btst.b  #PBFB_BOOTABLE,pb_Flags+3(a3)       ; Bootable partition?
                beq.s   .notboot
                jsr     _LVOAddBootNode(a6)
                exg.l   a4,a6                               ; Restore SysBase
                bra.s   .ok

.notboot:       moveq   #-128,d0                            ; Not bootable
                jsr     _LVOAddDosNode(a6)
                exg.l   a4,a6                               ; Restore SysBase
                bra.s   .ok

.MountKick1_3:
                move.l  #BootNode_SIZEOF,d0                 ; Manually allocate a BootNode
                move.l  #MEMF_PUBLIC+MEMF_CLEAR,d1
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq.s   .enomem
                move.l  d0,a0                               ; BootNode into A0
                move.l  d4,bn_DeviceNode(a0)                ; DosNode
                move.b  #NT_BOOTNODE,LN_TYPE(a0)
                move.b  pb_Environment+de_BootPri(a3),LN_PRI(a0) ; Boot priority
                move.l  MB_CD(a5),LN_NAME(a0)                    ; Link in the configdev
                jsr     _LVOForbid(a6)
                move.l  a0,a1                               ; Add to ExpansionBase mountlist
                lea.l   eb_MountList(a4),a0
                jsr     _LVOEnqueue(a6)
                jsr     _LVOPermit(a6)
                bra.s   .ok

.enomem:        moveq   #-1,d0
                rts
.ok:            moveq   #0,d0
                rts

;******************************************************************************
; CheckDupe - Check for duplicate partition names and make unique
;
; Prevents partition name conflicts by checking the existing mount list
; for duplicate names. If a duplicate is found:
;
; 1. Calls FixDupe to modify the name (adds .1, .2, .3, etc.)
; 2. Restarts the search to ensure the new name is also unique
; 3. Continues until a unique name is found
;
; The function walks through the expansion library's mount list,
; comparing each existing partition name using case-insensitive matching.
; Names are stored as BSTR (length-prefixed strings).
;
; INPUTS:
;   A1.L - BSTR partition name to check and potentially modify
;   A4.L - Pointer to expansion.library base
;
; OUTPUTS:
;   None (modifies name in place if needed)
;
; ALGORITHM:
;   1. Walk through eb_MountList linked list
;   2. For each BootNode, get its DeviceNode and name
;   3. Convert BPTR name to absolute pointer
;   4. Compare names using CompareBstr (case-insensitive)
;   5. If match found, call FixDupe and restart search
;   6. Continue until no matches found
;******************************************************************************
CheckDupe:      movem.l a0/a2,-(sp)
.restart:       lea.l   eb_MountList(a4),a2     ; A0 points to LH_HEAD
.loop:          move.l  LN_SUCC(a2),a2
                tst.l   LN_SUCC(a2)             ; LN_SUCC = NULL? end of list - no match
                beq.s   .end
                ;; Have a BootNode in a2
                move.l  bn_DeviceNode(a2),a0    ; get DeviceNode
                move.l  dn_Name(a0),d0
                lsl.l   #2,d0                   ; BPTR to PTR
                move.l  d0,a0
                bsr.s   CompareBstr             ; Compare Partition Name with this MountList node
                tst.l   d0
                bne.s   .loop                   ; Didn't match check the next one
                bsr.s   FixDupe                 ; Try to make the name unique
                bra.s   .restart                ; Check again
.end            movem.l (sp)+,a0/a2
                rts

;******************************************************************************
; FixDupe - Make a duplicate partition name unique
;
; Modifies a partition name to make it unique by appending a numeric
; suffix. The algorithm:
;
; 1. If name doesn't end with '.', adds '.1' to the end
; 2. If name already ends with '.digit', increments the digit
; 3. Handles name length limits (max 30 characters total)
; 4. Truncates original name if needed to fit suffix
;
; Examples: "Work" → "Work.1" → "Work.2" → "Work.3" etc.
;          "VeryLongPartitionNameHere" → "VeryLongPartitionNameH.1"
;
; INPUTS:
;   A1.L - BSTR partition name to modify
;
; OUTPUTS:
;   None (modifies name in place)
;
; ALGORITHM:
;   1. Get current string length from BSTR
;   2. Check if last character is '.' (previous modification)
;   3. If not, add 2 to length for ".0" suffix
;   4. If length exceeds 30, truncate to 30
;   5. Set new length and append ".1" or increment existing digit
;******************************************************************************
FixDupe:        movem.l d2/a1,-(sp)
                moveq   #0,d2
                move.b  (a1),d2           ; Get length
                cmp.b   #'.',-1(a1,d2.w)  ; Already attempted before?
                beq.s   .inc
                add.b   #2,d2             ; Increase string size for .x extension
                cmp.b   #30,d2            ; If the string is 30 chars or more truncate it
                bls.s   .init
                move.b  #30,d2

.init:          move.b  d2,(a1)
                move.b  #'.',-1(a1,d2.w)
                move.b  #'0',0(a1,d2.w)

.inc            add.b   #1,0(a1,d2.w)
                movem.l (sp)+,d2/a1
                rts

;******************************************************************************
; CompareBstr - Compare two BSTR strings (case insensitive)
;
; Performs case-insensitive comparison of two BSTR (length-prefixed)
; strings. The comparison algorithm:
;
; 1. First compares the length bytes - if different, strings don't match
; 2. If lengths are equal, compares each character after converting to
;    uppercase using the ToUpper function
; 3. Returns immediately on first character mismatch
; 4. Empty strings (length 0) are considered equal
;
; Used primarily for checking partition name conflicts in the mount list.
;
; INPUTS:
;   A0.L - First BSTR to compare
;   A1.L - Second BSTR to compare
;
; OUTPUTS:
;   D0.L - 0 if strings match, non-zero if different
;******************************************************************************
CompareBstr:    movem.l d2-d3/a0-a1,-(sp)
                move.b  (a0)+,d3
                move.b  (a1)+,d1
                cmp.b   d3,d1               ; Compare length
                bne.s   .false
                tst.b   d3                  ; Length == 0?
                beq.s   .true

                sub.b   #1,d3
.compare:       move.b  (a0)+,d0            ; Compare each char in a case-insensitive fashion
                bsr.s   ToUpper
                move.b  d0,d2
                move.b  (a1)+,d0
                bsr.s   ToUpper
                cmp.b   d0,d2
                bne.s   .false
                dbra    d3,.compare

.true:          moveq   #0,d0
.done:          movem.l (sp)+,d2-d3/a0-a1
                rts
.false:         moveq   #-1,d0
                bra.s   .done

;******************************************************************************
; ToUpper - Convert a character to uppercase
;
; Simple ASCII character case conversion utility. Converts lowercase
; letters (a-z) to their uppercase equivalents (A-Z) by subtracting
; the difference between 'a' and 'A' (32).
;
; Characters outside the lowercase range are returned unchanged.
; Used by CompareBstr for case-insensitive string comparisons.
;
; INPUTS:
;   D0.B - Character to convert
;
; OUTPUTS:
;   D0.B - Uppercase character
;******************************************************************************
ToUpper:        cmp.b   'a',d0
                blo.s   .done
                cmp.b   'z',d0
                bhi.s   .done
                sub.b   #'a'-'A',d0         ; Make Uppercase
.done:          rts

;******************************************************************************
; OpenFSR - Open or create the FileSystem.resource
;
; Ensures the FileSystem.resource exists in the system. This resource
; maintains a list of all available filesystem handlers. The function:
;
; 1. First attempts to open an existing FileSystem.resource
; 2. If not found, allocates memory for a new resource structure
; 3. Initializes the resource with proper node type and empty file list
; 4. Sets up the resource name string in the allocated memory
; 5. Adds the new resource to the system resource list
;
; This ensures filesystem handlers can be registered and found by DOS.
;
; INPUTS:
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - FileSystem.resource pointer (NULL on failure)
;
; MEMORY LAYOUT:
;   Allocated structure contains:
;   [0 to FileSysResource_SIZEOF): FileSysResource structure
;   [FileSysResource_SIZEOF+): Resource name string ("FileSystem.resource")
;******************************************************************************
OpenFSR:        movem.l a2,-(sp)
                lea.l   FileSysRes(PC),a1
                jsr     _LVOOpenResource(a6)
                move.l  d0,a2
                tst.l   d0
                bne.s   .done

                ;; If FileSystem.resource was not found create it
                move.l  #FileSysResource_SIZEOF+FsrLen,d0   ; Allocate with room for name string
                move.l  #MEMF_PUBLIC+MEMF_CLEAR,d1
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq.w   .err
                move.l  d0,a1                               ; FileSystem.resource into a1
                move.b  #NT_RESOURCE,LN_TYPE(a1)
                lea.l   fsr_FileSysEntries(a1),a2           ; Init FileSysEntries list
                NEWLIST a2
                lea.l   FileSysResource_SIZEOF(a1),a2       ; Point to the name string after the resource struct
                move.l  a2,LN_NAME(a1)
                move.l  a2,fsr_Creator(a1)

                lea.l   FileSysRes(pc),a0
.strcpy:        move.b  (a0)+,(a2)+                         ; Copy name string
                bne.s   .strcpy
                move.l  a1,a2
                jsr     _LVOAddResource(a6)                 ; Add it to the system
                move.l  a2,d0

.done:          movem.l (sp)+,a2
                rts
.err:           moveq.l #-1,d0
                bra.s   .done

;******************************************************************************
; FindFSE - Find a filesystem entry by DOS type
;
; Searches the FileSystem.resource for a filesystem handler matching
; the specified DOS type. DOS types are 4-character identifiers like
; 'DOS\1' (OFS), 'DOS\3' (FFS), 'PFS\3' (PFS3), etc.
;
; The function walks the linked list of FileSysEntry structures,
; comparing the fse_DosType field of each entry against the target.
; If found, returns both the entry pointer and its version number.
;
; Used by FindFS to check if a filesystem is already loaded before
; attempting to load it from the RDB.
;
; INPUTS:
;   D0.L - DOS type to search for (e.g., 'DOS\3', 'PFS\3')
;   A2.L - FileSystem.resource pointer
;
; OUTPUTS:
;   D0.L - Filesystem version number (0 if not found)
;   A0.L - FileSysEntry pointer (if found)
;******************************************************************************
FindFSE:        ;; Find matching FSE in FileSysResource
.gotfsr:        move.l  fsr_FileSysEntries+LH_HEAD(a2),a0
.loop:          tst.l   LN_SUCC(a0)
                beq.s   .notfound                           ; End of list
                cmp.l   fse_DosType(a0),d0                  ; Check DosType
                beq.s   .found
                move.l  LN_SUCC(a0),a0
                bra.s   .loop
.found:         move.l  fse_Version(a0),d0                  ; FSE FS version in d0
.done:          rts
.notfound:      moveq   #0,d0
                rts

;******************************************************************************
; LoadFS - Load filesystem from RDB and add to FileSystem.resource
;
; Loads a filesystem handler from the disk's RDB and integrates it into
; the system. This process involves:
;
; 1. Loading the filesystem code from LSEG (LoadSeg) blocks on disk
; 2. Allocating a new FileSysEntry structure
; 3. Copying filesystem metadata from the FSHD header to the entry
; 4. Linking the loaded code as the filesystem's seglist
; 5. Adding the new entry to the head of FileSystem.resource list
;
; Once loaded, the filesystem becomes available for mounting partitions
; of its DOS type. The seglist contains the executable filesystem code.
;
; INPUTS:
;   A2.L - FileSystem.resource pointer
;   A3.L - FSHD buffer containing filesystem header block
;   A5.L - Frame pointer for accessing local variables
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - FileSysEntry pointer (NULL on failure)
;
; REGISTER USAGE:
;   D2.L - Seglist pointer (result from LoadLseg)
;   A1.L - Allocated FileSysEntry structure
;   A4.L - Destination pointer for copying filesystem metadata
;******************************************************************************
LoadFS:         movem.l d2/a2/a4,-(sp)
                 ;; Load from RDB and add to FileSystem.resource
                move.l  fhb_SegListBlocks(a3),d0            ; First LSeg block
                bsr     LoadLseg
                tst.l   d0                                  ; Do we have a SegList?
                beq     .error                              ; Nope
                move.l  d0,d2                               ; Stash SegList

                move.l  #MEMF_PUBLIC+MEMF_CLEAR,d1          ; Allocate a FileSysEntry struct
                move.l  #fse_GlobalVec+4,d0
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq     .error

                move.l  d0,a1
                move.l  d0,-(sp)                            ; Stash FileSysEntry address
                lea.l   fhb_DosType(a3),a0
                lea.l   fse_DosType(a1),a4
                moveq   #11,d1                              ; Copy relevant fields
.copy:          move.l  (a0)+,(a4)+
                dbf     d1,.copy
                move.l  MB_DEVICE(a5),LN_NAME(a1)
                move.l  d2,fse_SegList(a1)                  ; Patch in the new seglist
                lea.l   fsr_FileSysEntries(a2),a0
                jsr     _LVOAddHead(a6)                     ; Add it to the head of FileSysEntries

                move.l  (sp)+,d0
.done           movem.l (sp)+,d2/a2/a4
                rts
.error:         move.l  #0,d0
                bra.s  .done

;******************************************************************************
; FindFS - Locate filesystem handler by DOS type with variable block support
;
; This function searches for a filesystem handler in two locations:
; 1. FileSystem.resource - handlers loaded by other software
; 2. RDB filesystem headers - handlers stored on the disk itself
;
; If found in both locations, it compares versions and uses the newer one.
; If the RDB version is newer, it loads the filesystem from disk and adds
; it to FileSystem.resource for future use. Now supports variable block
; sizes for FSHD buffer allocation.
;
; INPUTS:
;   D0.L - DOS type to search for (e.g., 'DOS\1', 'PFS\3')
;   A5.L - Frame pointer for accessing local variables
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - FileSysEntry pointer (NULL if not found)
;
; REGISTER USAGE:
;   D2.L - DOS type being searched for
;   D3.L - FileSysEntry pointer from FileSystem.resource (or 0)
;   D4.L - Version number from FileSystem.resource (or -1)
;   D5.L - Version number from RDB FSHD block (or -1)
;   A2.L - FileSystem.resource pointer
;   A3.L - FSHD buffer for reading filesystem headers
;
; ALGORITHM:
;   1. Open/create FileSystem.resource
;   2. Search for DOS type in existing entries
;   3. Search for DOS type in RDB FSHD blocks using device block size
;   4. Compare versions and use newer one
;   5. Load from RDB if RDB version is newer or same
;   6. Return best available FileSysEntry
;******************************************************************************
FindFS:         movem.l d2-d5/a2-a4,-(sp)
                move.l  d0,d2
                bsr     OpenFSR
                tst.l   d0
                beq     .done
                moveq   #-1,d4          ; FSE Version
                moveq   #0,d3           ; FSE
                move.l  d0,a2           ; Pointer to FileSystem.resource
                move.l  d2,d0           ; DosType
                bsr     FindFSE         ; Look for a matching dos type
                tst.l   d0
                beq     .findfshb       ; Not found, try RDB
                move.l  d0,d4           ; FSE Version
                move.l  a0,d3           ; FSE Node
.findfshb:
                ;; Now check in the RDB
                moveq   #-1,d5
                cmp.l   #$FFFF,MB_FSHB(a5)                  ; No FSHB Blocks?
                beq     .err                                ; Then skip

                move.l  MB_BLOCKSIZE(a5),d0                             ; Allocate FileSysHeaderBlock buffer
                move.l  #MEMF_ANY+MEMF_CLEAR,d1
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq     .err
                move.l  d0,a3                               ; Buffer in a3
                move.l  MB_FSHB(a5),d0                      ; Start at the first FSHB

.fshbloop:      cmp.l   #$FFFF,d0                           ; End of list?
                beq.s   .notfoundfshb                       ; Not found
                lea.l   MB_IOREQ(a5),a1
                move.l  #'FSHD',d1                          ; Block type
                move.l  a3,a0                               ; Buffer
                bsr.w   GetBlock
                tst.l   d0
                bne     .notfoundfshb                       ; GetBlock failed, give up
                cmp.l   fhb_DosType(a3),d2
                beq     .foundfshb                          ; Found it!
                move.l  fhb_Next(a3),d0                     ; Get the next block
                bra.s   .fshbloop
.foundfshb:     move.l  fhb_Version(a3),d5                  ; FSHB ver in d5
.notfoundfshb:
                ; ;; Compare the versions
                ; ;; If FSR has the same or newer version we use that
                cmp.l   d5,d4                               ; FSR version is same or higher, use it
                bge.s   .freefsbuf
                bsr     LoadFS                              ; Load FS from RDB
                tst.l   d0
                beq     .freefsbuf                          ; Failed, give up
                move.l  d0,d3                               ; Loaded FS into a2

.freefsbuf:     move.l  a3,d0
                tst.l   d0
                beq.s   .skipfree
                move.l  MB_BLOCKSIZE(a5),d0
                move.l  a3,a1
                jsr     _LVOFreeMem(a6)
.skipfree:
                move.l  d3,d0
.done:          movem.l (sp)+,d2-d5/a2-a4
                rts
.err            moveq   #0,d0
                bra.s   .done

;******************************************************************************
; LoadLseg - Load seglist from LSEG blocks with enhanced read handle
;
; Loads executable code from a chain of LSEG (LoadSeg) blocks stored on
; disk. This is used to load filesystem handlers that are embedded in
; the RDB. The process:
;
; 1. Allocates a buffer sized to match the device's block size
; 2. Sets up an enhanced read handle structure for streaming data
; 3. Stores mount base pointer in read handle for block size access
; 4. Calls the relocate function to parse and load the executable
; 5. The relocate function handles hunks, relocations, and linking
; 6. Returns a seglist pointer suitable for DOS LoadSeg format
;
; The resulting seglist can be executed by DOS or stored in FileSysEntry.
; Now supports variable block sizes and enhanced parameter passing.
;
; INPUTS:
;   D0.L - First LSEG block number on disk
;   A5.L - Frame pointer for accessing local variables
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - Seglist pointer (NULL on failure)
;
; READ HANDLE STRUCTURE:
;   The function creates a temporary read handle on the stack:
;   MOUNTBASE: Pointer to mount base (A5) for block size access
;   RH_BUF: Pointer to block-sized buffer for LSEG data
;   RH_IOREQ: IORequest for disk I/O operations
;   RH_SECTOR: Current LSEG block number
;   RH_INDEX: Byte offset within current block
;   RH_BYTES: Bytes remaining in current block
;******************************************************************************
LoadLseg        movem.l d2/a2,-(sp)
                move.l  d0,d2                   ; Stash blocknum
 
                move.l  MB_BLOCKSIZE(a5),d0                 ; Allocate LSEG Buffer
                move.l  #MEMF_ANY+MEMF_CLEAR,d1
                jsr     _LVOAllocMem(a6)
                tst.l   d0
                beq     .error

                sub.l   #RH_SIZE,sp
                move.l  sp,a2                   ; Read handle on the stack
                move.l  a5,MOUNTBASE(a2)
                lea.l   MB_IOREQ(a5),a0
                move.l  a0,RH_IOREQ(a2)         ; Use existing IOREQ
                move.l  d0,RH_BUF(a2)           ; LSEG Buffer
                clr.l   RH_BYTES(a2)            ; 
                clr.l   RH_INDEX(a2)
                move.l  d2,RH_SECTOR(a2)        ; First LSEG sector

                ;; Read handle ready, call into loadseg here
                lea.l   LSGetLong(pc),a0
                bsr     _loadseg                ; ReadFunc in A0, FH in A2
                tst.l   d0
                beq     .freebuf
                move.l  d0,d2                   ; Stash seglist

.freebuf        move.l  RH_BUF(a2),d0
                move.l  d0,a1                   ; Free lseg buffer
                move.l  MB_BLOCKSIZE(a5),d0
                jsr     _LVOFreeMem(a6)
                add.l   #RH_SIZE,sp             ; Restore SP
                move.l  d2,d0                   ; Return Seglist
.done           movem.l (sp)+,d2/a2
                rts
.error:         moveq   #0,d0
                bra     .done

;******************************************************************************
; LSGetLong - Read a long value from LSEG blocks
;
; Streaming reader function for loading executable data from chained
; LSEG blocks. This function abstracts the block structure, providing
; a simple "read next long" interface to the relocate function.
;
; The function maintains a read handle with:
; - Current buffer with cached block data
; - Read index into the current block
; - Bytes remaining in current block
; - Next block number in the chain
; - Mount base reference for accessing block size information
;
; When the buffer is exhausted, automatically loads the next LSEG block
; and continues reading. Each LSEG block contains a header with the
; next block pointer, followed by executable data.
;
; INPUTS:
;   A0.L - Buffer to store the result (not used - returns in D0)
;   A4.L - Read handle structure with mount base reference
;   A6.L - Pointer to exec.library base
;
; OUTPUTS:
;   D0.L - The long value read from the LSEG stream
;
; READ HANDLE USAGE:
;   MOUNTBASE: Used to restore A5 for accessing MB_BLOCKSIZE
;   RH_BYTES: Tracks remaining bytes in current block
;   RH_INDEX: Current read position within block
;   RH_BUF: Pointer to block-sized buffer for LSEG data
;   RH_SECTOR: Current/next LSEG block number
;
; LSEG BLOCK FORMAT:
;   [0 to lsb_LoadData): LSEG header with next block pointer
;   [lsb_LoadData to block_size): Executable data
;   Usable data size = MB_BLOCKSIZE - lsb_LoadData
;******************************************************************************
LSGetLong:      movem.l a2-a3/a5/d2-d3,-(sp)
                move.l  MOUNTBASE(a4),a5
                move.l  a0,a2
                tst.l   RH_BYTES(a4)
                beq     .fill
.filled:        move.l  RH_INDEX(a4),d1         ; Read index
                move.l  RH_BUF(a4),a0
                add.l   d1,a0                   ; Index into the buffer
.copy:          move.l  (a0),d0
                sub.l   #4,RH_BYTES(a4)
                add.l   #4,RH_INDEX(a4)
.done           movem.l (sp)+,a2-a3/a5/d2-d3
                rts
      
.fill:          move.l  a2,-(sp)
                move.l  RH_IOREQ(a4),a1
                move.l  RH_BUF(a4),a2
                move.l  #'LSEG',d1
                move.l  RH_SECTOR(a4),d0
                move.l  a2,a0
                bsr     GetBlock                    ; Get LSEG block
                tst.l   d0
                bne     .filldone
                move.l  lsb_Next(a2),RH_SECTOR(a4)  ; Link to next LSEG
                move.l  MB_BLOCKSIZE(a5),d0
                sub.l   #lsb_LoadData,d0
                move.l  d0,RH_BYTES(a4)           ; Reset byte count (skip the LSEG header)
                move.l  #lsb_LoadData,RH_INDEX(a4)
.filldone:      move.l  (sp)+,a2
                bra     .filled

Expansion:
                DC.B "expansion.library",0
FileSysRes:
                DC.B "FileSystem.resource",0
FsrLen: *-FileSysRes
