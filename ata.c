// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <inline/timer.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <exec/errors.h>
#include <stdbool.h>

#include "debug.h"
#include "device.h"
#include "ata.h"
#include "atapi.h"
#include "scsi.h"
#include "string.h"
#include "blockcopy.h"
#include "wait.h"
#include "lide_alib.h"

static BYTE write_taskfile_lba(struct IDEUnit *unit, UBYTE command, ULONG lba, UBYTE sectorCount, UBYTE features);
static BYTE write_taskfile_lba48(struct IDEUnit *unit, UBYTE command, ULONG lba, UBYTE sectorCount, UBYTE features);
static BYTE write_taskfile_chs(struct IDEUnit *unit, UBYTE command, ULONG lba, UBYTE sectorCount, UBYTE features);

/**
 * ata_status_reg_delay
 *
 * We need a short delay before actually checking the status register to let the drive update the status
 * To get the right delay we read the status register 4 times but just throw away the result
 * More info: https://wiki.osdev.org/ATA_PIO_Mode#400ns_delays
 *
 * @param unit Pointer to an IDEUnit struct
*/
static void __attribute__((always_inline)) ata_status_reg_delay(struct IDEUnit *unit) {
    asm volatile (
        ".rep 4     \n\t"
        "tst.l (%0) \n\t" // Use tst.l so we don't need to save/restore some other register
        ".endr      \n\t"
        :
        : "a" (unit->drive.status_command)
        :
    );
}

/**
 * ata_save_error
 *
 * Save the contents of the drive registers so that errors can be reported in sense data
 *
*/
static void ata_save_error(struct IDEUnit *unit) {
    unit->last_error[0] = *unit->drive.error_features;
    unit->last_error[1] = *unit->drive.lbaHigh;
    unit->last_error[2] = *unit->drive.lbaMid;
    unit->last_error[3] = *unit->drive.lbaLow;
    unit->last_error[4] = *unit->drive.status_command;
    unit->last_error[5] = *unit->drive.devHead;
}

/**
 * ata_check_error
 *
 * Check for ATA error / fault
 *
 * @param unit Pointer to an IDEUnit struct
 * @returns True if error is indicated
*/
static bool __attribute__((always_inline)) ata_check_error(struct IDEUnit *unit) {
    return (*unit->drive.status_command & (ata_flag_error | ata_flag_df));
}

/**
 * ata_wait_drq
 *
 * Poll DRQ in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
 * @param fast More aggressive polling, 1000 tries before timer wait vs 100
*/
static bool ata_wait_drq(struct IDEUnit *unit, ULONG tries, bool fast) {
    struct timerequest *tr = unit->itask->tr;
    Trace("wait_drq enter\n");
    UBYTE status;

    int loops = (fast) ? 1000 : 100;

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<loops; j++) {
            status = *unit->drive.status_command;
            if ((status & ata_flag_drq) != 0) return true;
            if (status & (ata_flag_error | ata_flag_df)) return false;
        }
        wait_us(tr,ATA_DRQ_WAIT_LOOP_US);
    }
    Trace("wait_drq timeout\n");
    return false;
}

/**
 * ata_wait_not_busy
 *
 * Poll BSY in the status register until clear or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool ata_wait_not_busy(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->itask->tr;

    ata_status_reg_delay(unit);

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<100; j++) {
            if ((*unit->drive.status_command & ata_flag_busy) == 0) return true;
        }
        wait_us(tr,ATA_BSY_WAIT_LOOP_US);
    }
    return false;
}

/**
 * ata_wait_ready
 *
 * Poll RDY in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool ata_wait_ready(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->itask->tr;

    ata_status_reg_delay(unit);

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<1000; j++) {
            if ((*unit->drive.status_command & (ata_flag_ready | ata_flag_busy)) == ata_flag_ready) return true;
        }
        wait_us(tr,ATA_RDY_WAIT_LOOP_US);
    }
    return false;
}

/**
 * ata_select
 *
 * Selects the drive by writing to the dev head register
 * If the request would not change the drive select then this will be a no-op to save time
 *
 * @param unit Pointer to an IDEUnit struct
 * @param select the dev head value to set
 * @param wait whether to wait for the drive to clear BSY after selection
 * @returns true if the drive selection was changed
*/
bool ata_select(struct IDEUnit *unit, UBYTE select, bool wait)
{
    bool changed = false;
    volatile UBYTE *shadowDevHead = unit->shadowDevHead;

    if (!unit->lba) select &= ~(0x40);

    if (*shadowDevHead == select) {
        return false;
    }

    if ((*shadowDevHead & 0xF0) != (select & 0xF0)) changed = true;

    // Wait for BSY to clear before changing drive unless there's no drive selected
    if (*shadowDevHead != 0 && changed) ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT);

    *unit->drive.devHead = select;

    if (changed && wait) {
        wait_us(unit->itask->tr,5); // Could possibly be replaced with call to ata_status_reg_delay
        ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT);
    }

    *shadowDevHead = select;

    return changed;
}

/**
 * ata_identify
 *
 * Send an IDENTIFY command to the device and place the results in the buffer
 * @param unit Pointer to an IDEUnit struct
 * @param buffer Pointer to the destination buffer
 * @return false on error
*/
bool ata_identify(struct IDEUnit *unit, UWORD *buffer)
{
    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0; // Select drive

    ata_select(unit,drvSel,false);

    if (!ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT)) return false;

    *unit->drive.sectorCount    = 0;
    *unit->drive.lbaLow         = 0;
    *unit->drive.lbaMid         = 0;
    *unit->drive.lbaHigh        = 0;
    *unit->drive.error_features = 0;
    *unit->drive.devHead        = drvSel;
    *unit->drive.status_command = ATA_CMD_IDENTIFY;

    if (ata_check_error(unit) || !ata_wait_drq(unit,500,false)) {
        Warn("ATA: IDENTIFY Status: Error\n");
        Warn("ATA: last_error: %08lx\n",&unit->last_error[0]);
        // Save error information
        ata_save_error(unit);
        return false;
    }

    if (buffer) {
        UWORD read_data;
        for (int i=0; i<256; i++) {
            read_data = *unit->drive.data; //autoincrement on the ide-side
            // Interface is byte-swapped, so swap the identify data back.
            buffer[i] = ((read_data >> 8) | (read_data << 8));
        }
    }

    return true;
}

/**
 * ata_bench
 * 
 * Measure the amount of E Clock ticks taken to transfer 512K from the unit
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param xfer_routine Pointer to one of the transfer routines
 * @param buffer pointer to a 512 byte buffer
 * @return tick count
 * 
 */
static ULONG ata_bench(struct IDEUnit *unit, void *xfer_routine, void *buffer) {
    struct ExecBase *SysBase = unit->SysBase;
    struct Device *TimerBase = unit->itask->tr->tr_node.io_Device;
    struct EClockVal *startTime;
    struct EClockVal *endTime;
    ULONG ticks = 0;

    if (TimerBase->dd_Library.lib_Version < 36) return 0;

    if (buffer) {
        void (*do_xfer)(void *source asm("a0"), void *destination asm("a1")) = xfer_routine;
        if ((startTime = (struct EClockVal *)AllocMem(sizeof(struct EClockVal),MEMF_ANY|MEMF_CLEAR))) {
            if ((endTime = (struct EClockVal *)AllocMem(sizeof(struct EClockVal),MEMF_ANY|MEMF_CLEAR))) {
                ReadEClock(startTime);

                for (int i=0; i<1024; i++) {
                    do_xfer((void *)unit->drive.status_command,buffer);
                }

                ReadEClock(endTime);
                ticks =  (*(uint64_t *)endTime) - (*(uint64_t *)startTime);
                FreeMem(endTime,sizeof(struct EClockVal));
            }
            FreeMem(startTime,sizeof(struct EClockVal));
        }
    }
    return ticks;
}

/**
 * ata_autoselect_xfer
 * 
 * Set the transfer method for the unit based on the CPU, Board type and benchmark result
 * 
 * @param unit Pointer to an IDEUnit struct
 * @return transfer method
 */
static enum xfer ata_autoselect_xfer(struct IDEUnit *unit) {
    struct ExecBase *SysBase = unit->SysBase;
    ULONG ticks;
    void *buf;

    // longword_movem requires 512 Byte register spacing
    if ((unit->drive.lbaMid - unit->drive.lbaLow) != 512)
        return longword_move;

    // longword_movem will always be faster on a standard 68000
    if ((SysBase->AttnFlags & (AFF_68020 | AFF_68030 | AFF_68040 | AFF_68060)) == 0)
        return longword_movem;
    
    // ReadEClock needed by ata_bench not supported before Kick 2.0
    if (SysBase->LibNode.lib_Version < 36)
        return longword_movem;
    
    if ((buf = AllocMem(512,MEMF_ANY))) {
        enum xfer method;
        ticks = ata_bench(unit,&ata_read_long_movem,buf);
        if (ticks > 0 && ata_bench(unit,&ata_read_long_move,buf) < ticks) {
            method = longword_move;
        } else {
            method = longword_movem;
        }
        FreeMem(buf,512);
        return method;
    } else {
        return longword_movem;
    }
}

/**
 * ata_set_xfer
 * 
 * Sets the transfer routine for the unit
 * 
 * @param unit Pointer to an IDEUnit strict
 * @param method Transfer routine
 */
void ata_set_xfer(struct IDEUnit *unit, enum xfer method) {
    switch (method) {
        default:
        case longword_movem:
            unit->read_fast       = &ata_read_long_movem;
            unit->read_unaligned  = &ata_read_unaligned_long;
            unit->write_fast      = &ata_write_long_movem;
            unit->write_unaligned = &ata_write_unaligned_long;

            unit->xferMethod = longword_movem;
            break;
        case longword_move:
            unit->read_fast       = &ata_read_long_move;
            unit->read_unaligned  = &ata_read_unaligned_long;
            unit->write_fast      = &ata_write_long_move;
            unit->write_unaligned = &ata_write_unaligned_long;

            unit->xferMethod = longword_move;
            break;
    }
}

/**
 * ata_init_unit
 *
 * Initialize a unit, check if it is there and responding
 * @param unit Pointer to an IDEUnit struct
 * @returns false on error
*/
bool ata_init_unit(struct IDEUnit *unit) {
    struct ExecBase *SysBase = unit->SysBase;

    unit->cylinders       = 0;
    unit->heads           = 0;
    unit->sectorsPerTrack = 0;
    unit->blockSize       = 0;
    unit->present         = false;
    unit->mediumPresent   = false;

    ULONG offset;
    UWORD *buf;
    bool dev_found = false;

    offset = (unit->channel == 0) ? CHANNEL_0 : CHANNEL_1;

    unit->drive.data           = (UWORD*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_data);
    unit->drive.error_features = (UBYTE*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_error);
    unit->drive.sectorCount    = (UBYTE*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_sectorCount);
    unit->drive.lbaLow         = (UBYTE*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_lbaLow);
    unit->drive.lbaMid         = (UBYTE*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_lbaMid);
    unit->drive.lbaHigh        = (UBYTE*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_lbaHigh);
    unit->drive.devHead        = (UBYTE*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_devHead);
    unit->drive.status_command = (UBYTE*) ((void *)unit->cd->cd_BoardAddr + offset + ata_reg_status);

    *unit->shadowDevHead = *unit->drive.devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive

    enum xfer method = ata_autoselect_xfer(unit);
    ata_set_xfer(unit,method);

    for (int i=0; i<(8*NEXT_REG); i+=NEXT_REG) {
        // Check if the bus is floating (D7/6 pulled-up with resistors)
        if ((i != ata_reg_devHead) && (*((volatile UBYTE *)unit->cd->cd_BoardAddr + offset  + i) & 0xC0) != 0xC0) {
            dev_found = true;
            Trace("INIT: Unit base: %08lx; Drive base %08lx\n",unit, unit->drive);
            break;
        }
    }

    if (dev_found == false || !ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT)) {
        return false;
    }

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) { // Allocate buffer for IDENTIFY result
        return false;
    }

    Trace("INIT: IDENTIFY\n");
    if (ata_identify(unit,buf) == true) {
        Info("INIT: ATA Drive found!\n");

        unit->lba             = (buf[ata_identify_capabilities] & ata_capability_lba) != 0;
        unit->cylinders       = buf[ata_identify_cylinders];
        unit->heads           = buf[ata_identify_heads];
        unit->sectorsPerTrack = buf[ata_identify_sectors];
        unit->blockSize       = 512;
        unit->logicalSectors  = buf[ata_identify_logical_sectors+1] << 16 | buf[ata_identify_logical_sectors];
        unit->blockShift      = 0;
        unit->mediumPresent   = true;
        unit->multipleCount   = buf[ata_identify_multiple] & 0xFF;

        if (unit->multipleCount > 0 && (ata_set_multiple(unit,unit->multipleCount) == 0)) {
            unit->xferMultiple = true;
        } else {
            unit->xferMultiple = false;
            unit->multipleCount = 1;
        }

        // Support LBA-48 but only up to 2TB
        if ((buf[ata_identify_features] & ata_feature_lba48) && unit->logicalSectors >= 0xFFFFFFF) {
            if (buf[ata_identify_lba48_sectors + 2] > 0 ||
                buf[ata_identify_lba48_sectors + 3] > 0) {
                Info("INIT: Rejecting drive larger than 2TB\n");
                return false;
            }

            unit->lba48 = true;
            Info("INIT: Drive supports LBA48 mode \n");
            unit->logicalSectors = (buf[ata_identify_lba48_sectors + 1] << 16 |
                                    buf[ata_identify_lba48_sectors]);
            unit->write_taskfile = &write_taskfile_lba48;

        } else if (unit->lba == true) {
            // LBA-28 up to 127GB
            unit->write_taskfile = &write_taskfile_lba;

        } else {
            // CHS Mode
            Warn("INIT: Drive doesn't support LBA mode\n");
            unit->write_taskfile = &write_taskfile_chs;
            unit->logicalSectors = (unit->cylinders * unit->heads * unit->sectorsPerTrack);
        }

        Info("INIT: Logical sectors: %ld\n",unit->logicalSectors);

        if (unit->logicalSectors == 0 || unit->heads == 0 || unit->cylinders == 0) goto ident_failed;

        if (unit->logicalSectors >= 267382800) {
            // For drives larger than 127GB fudge the geometry
            unit->heads           = 63;
            unit->sectorsPerTrack = 255;
            unit->cylinders       = (unit->logicalSectors / (63*255));
        } else if (unit->logicalSectors >= 16514064) {
            // If a drive is larger than 8GB then the drive will report a geometry of 16383/16/63 (CHS)
            // In this case generate a new Cylinders value
            unit->heads = 16;
            unit->sectorsPerTrack = 255;
            unit->cylinders = (unit->logicalSectors / (16*255));
            Info("INIT: Adjusting geometry, new geometry; 16/255/%ld\n",unit->cylinders);
        }


        while ((unit->blockSize >> unit->blockShift) > 1) {
            unit->blockShift++;
        }
    } else if (atapi_check_signature(unit)) { // Check for ATAPI Signature
        if (atapi_identify(unit,buf) && (buf[0] & 0xC000) == 0x8000) {
            Info("INIT: ATAPI Drive found!\n");

                unit->deviceType      = (buf[0] >> 8) & 0x1F;
                unit->atapi           = true;
        } else {
ident_failed:
            Warn("INIT: IDENTIFY failed\n");
            // Command failed with a timeout or error
            FreeMem(buf,512);
            return false;
        }
    }

    if (unit->atapi == false && unit->blockSize == 0) {
        Warn("INIT: Error! blockSize is 0\n");
        if (buf) FreeMem(buf,512);
        return false;
    }

    Info("INIT: Blockshift: %ld\n",unit->blockShift);
    unit->present = true;

    Info("INIT: LBAs %ld Blocksize: %ld\n",unit->logicalSectors,unit->blockSize);

    if (buf) FreeMem(buf,512);
    return true;
}

/**
 * ata_set_multiple
 *
 * Configure the DRQ block size for READ MULTIPLE and WRITE MULTIPLE
 * @param unit Pointer to an IDEUnit struct
 * @param multiple DRQ Block size
 * @return non-zero on error
*/
bool ata_set_multiple(struct IDEUnit *unit, BYTE multiple) {
    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0; // Select drive

    ata_select(unit,drvSel,true);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
            return HFERR_SelTimeout;

    *unit->drive.sectorCount    = multiple;
    *unit->drive.lbaLow         = 0;
    *unit->drive.lbaMid         = 0;
    *unit->drive.lbaHigh        = 0;
    *unit->drive.error_features = 0;
    *unit->drive.status_command = ATA_CMD_SET_MULTIPLE;

    if (!ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT))
        return IOERR_UNITBUSY;

    if (ata_check_error(unit)) {
        if (*unit->drive.error_features & ata_err_flag_aborted) {
            return IOERR_ABORTED;
        } else {
            return TDERR_NotSpecified;
        }
    }

    return 0;
}

#pragma GCC push_options
#pragma GCC optimize ("-O3")

/**
 * ata_read
 *
 * Read blocks from the unit
 * @param buffer destination buffer
 * @param lba LBA Address
 * @param count Number of blocks to transfer
 * @param unit Pointer to the unit structure
 * @returns error
*/
BYTE ata_read(void *buffer, ULONG lba, ULONG count, struct IDEUnit *unit) {
    Trace("ata_read enter\n");
    Trace("ATA: Request sector count: %ld\n",count);

    UBYTE error = 0;
    ULONG txn_count; // Amount of sectors to transfer in the current READ/WRITE command

    UBYTE command;
    UBYTE multipleCount = unit->multipleCount;
    volatile void *dataRegister = unit->drive.data;

    if (unit->lba48) {
        command = ATA_CMD_READ_MULTIPLE_EXT;
    } else {
        command = (unit->xferMultiple) ? ATA_CMD_READ_MULTIPLE : ATA_CMD_READ;
    }

    void (*ata_xfer)(void *source asm("a0"), void *destination asm("a1"));

    /* If the buffer is not word-aligned we need to use a slower routine */
    if (((ULONG)buffer) & 0x01) {
        ata_xfer = unit->read_unaligned;
    } else {
        ata_xfer = unit->read_fast;
    }

    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)) {
        ata_save_error(unit);
        return HFERR_SelTimeout;
    }

    /**
     * Transfer up-to MAX_TRANSFER_SECTORS per ATA command invocation
     *
     * count:          Number of sectors to transfer for this io request
     * txn_count:      Number of sectors to transfer to/from the drive in one ATA command transaction
     * multiple_count: Max number of sectors that can be transferred before polling DRQ
     */
    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            txn_count = MAX_TRANSFER_SECTORS;
        } else {
            txn_count = count;               // Get any remainders
        }
        count -= txn_count;

        Trace("ATA: XFER Count: %ld, txn_count: %ld\n",count,txn_count);

        if ((error = unit->write_taskfile(unit,command,lba,txn_count,0)) != 0) {
            ata_save_error(unit);
            return error;
        }

        lba += txn_count;

        while (txn_count) {
            if (!ata_wait_drq(unit,ATA_DRQ_WAIT_COUNT,true)) {
                ata_save_error(unit);
                return IOERR_UNITBUSY;
            }

            /* Transfer up to (multiple_count) sectors before polling DRQ again */
            for (int i = 0; i < multipleCount && txn_count; i++) {
                ata_xfer((void *)dataRegister,buffer);
                txn_count--;
                buffer += 512;
            }
        }

    }

    return 0;
}

/**
 * ata_write
 *
 * Write blocks to the unit
 * @param buffer source buffer
 * @param lba LBA Address
 * @param count Number of blocks to transfer
 * @param unit Pointer to the unit structure
 * @returns error
*/
BYTE ata_write(void *buffer, ULONG lba, ULONG count, struct IDEUnit *unit) {
    Trace("ata_write enter\n");
    Trace("ATA: Request sector count: %ld\n",count);

    UBYTE error = 0;

    ULONG txn_count; // Amount of sectors to transfer in the current READ/WRITE command

    UBYTE command;
    UBYTE multipleCount = unit->multipleCount;
    volatile void *dataRegister = unit->drive.data;

    if (unit->lba48) {
        command = ATA_CMD_WRITE_MULTIPLE_EXT;
    } else {
        command = (unit->xferMultiple) ? ATA_CMD_WRITE_MULTIPLE : ATA_CMD_WRITE;
    }

    void (*ata_xfer)(void *source asm("a0"), void *destination asm("a1"));

    /* If the buffer is not word-aligned we need to use a slower routine */
    if ((ULONG)buffer & 0x01) {
        ata_xfer = unit->write_unaligned;
    } else {
        ata_xfer = unit->write_fast;
    }

    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)) {
        ata_save_error(unit);
        return HFERR_SelTimeout;
    }

    /**
     * Transfer up-to MAX_TRANSFER_SECTORS per ATA command invocation
     *
     * count:          Number of sectors to transfer for this io request
     * txn_count:      Number of sectors to transfer to/from the drive in one ATA command transaction
     * multiple_count: Max number of sectors that can be transferred before polling DRQ
     */
    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            txn_count = MAX_TRANSFER_SECTORS;
        } else {
            txn_count = count;               // Get any remainders
        }
        count -= txn_count;

        Trace("ATA: XFER Count: %ld, txn_count: %ld\n",count,txn_count);

        if ((error = unit->write_taskfile(unit,command,lba,txn_count,0)) != 0) {
            ata_save_error(unit);
            return error;
        }

        lba += txn_count;

        while (txn_count) {
            if (!ata_wait_drq(unit,ATA_DRQ_WAIT_COUNT,true)) {
                ata_save_error(unit);
                return IOERR_UNITBUSY;
            }

            /* Transfer up to (multiple_count) sectors before polling DRQ again */
            for (int i = 0; i < multipleCount && txn_count; i++) {
                ata_xfer(buffer,(void *)dataRegister);
                txn_count--;
                buffer += 512;
            }
        }

    }

    return 0;
}

/**
 * ata_read_unaligned_long
 *
 * Read data to an unaligned buffer
 * @param source Pointer to the drive data port
 * @param destination Pointer to the data buffer
*/
void ata_read_unaligned_long(void *source asm("a0"), void *destination asm("a1")) {
    ULONG readLong;
    UBYTE *dest = (UBYTE *)destination;

    for (int i=0; i<(512/4); i++) {          // Read (512 / 4) Long words from drive
        readLong = *(ULONG *)source;
        dest[0] = ((readLong >> 24) & 0xFF); // Write it out in 4 bytes
        dest[1] = ((readLong >> 16) & 0xFF);
        dest[2] = ((readLong >> 8) & 0xFF);
        dest[3] = (readLong & 0xFF);
        dest += 4;                           // Increment dest pointer by 4 bytes
    }
}

/**
 * ata_write_unaligned_long
 *
 * Write data from an unaligned buffer
 * @param source Pointer to the data buffer
 * @param destination Pointer to the drive data port
*/
void ata_write_unaligned_long(void *source asm("a0"), void *destination asm("a1")) {
    UBYTE *src = (UBYTE *)source;
    for (int i=0; i<(512/4); i++) {  // Write (512 / 4) Long words to drive
        *(ULONG *)destination = (src[0] << 24 | src[1] << 16 | src[2] << 8 | src[3]);
        src += 4;
    }
}

/**
 * write_taskfile_chs
 *
 * @param unit Pointer to an IDEUnit struct
 * @param lba  Pointer to the LBA variable
*/
static BYTE write_taskfile_chs(struct IDEUnit *unit, UBYTE command, ULONG lba, UBYTE sectorCount, UBYTE features) {
    UWORD cylinder = (lba / (unit->heads * unit->sectorsPerTrack));
    UBYTE head     = ((lba / unit->sectorsPerTrack) % unit->heads) & 0xF;
    UBYTE sector   = (lba % unit->sectorsPerTrack) + 1;

    BYTE devHead;

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
        return HFERR_SelTimeout;

    devHead = ((unit->primary) ? 0xA0 : 0xB0) | (head & 0x0F);

    *unit->shadowDevHead         = devHead;
    *unit->drive.devHead        = devHead;
    *unit->drive.sectorCount    = sectorCount; // Count value of 0 indicates to transfer 256 sectors
    *unit->drive.lbaLow         = (UBYTE)(sector);
    *unit->drive.lbaMid         = (UBYTE)(cylinder);
    *unit->drive.lbaHigh        = (UBYTE)(cylinder >> 8);
    *unit->drive.error_features = features;
    *unit->drive.status_command = command;

    return 0;
}

/**
 * write_taskfile_lba
 *
 * @param unit Pointer to an IDEUnit struct
 * @param lba  Pointer to the LBA variable
*/
static BYTE write_taskfile_lba(struct IDEUnit *unit, UBYTE command, ULONG lba, UBYTE sectorCount, UBYTE features) {
    BYTE devHead;

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
        return HFERR_SelTimeout;

    devHead = ((unit->primary) ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F);

    *unit->shadowDevHead         = devHead;
    *unit->drive.devHead        = devHead;
    *unit->drive.sectorCount    = sectorCount; // Count value of 0 indicates to transfer 256 sectors
    *unit->drive.lbaLow         = (UBYTE)(lba);
    *unit->drive.lbaMid         = (UBYTE)(lba >> 8);
    *unit->drive.lbaHigh        = (UBYTE)(lba >> 16);
    *unit->drive.error_features = features;
    *unit->drive.status_command = command;

    return 0;
}

/**
 * write_taskfile_lba48
 *
 * @param unit Pointer to an IDEUnit struct
 * @param lba  Pointer to the LBA variable
*/
static BYTE write_taskfile_lba48(struct IDEUnit *unit, UBYTE command, ULONG lba, UBYTE sectorCount, UBYTE features) {

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
        return HFERR_SelTimeout;

    *unit->drive.sectorCount    = (sectorCount == 0) ? 1 : 0;
    *unit->drive.lbaHigh        = 0;
    *unit->drive.lbaMid         = 0;
    *unit->drive.lbaLow         = (UBYTE)(lba >> 24);
    *unit->drive.sectorCount    = sectorCount; // Count value of 0 indicates to transfer 256 sectors
    *unit->drive.lbaHigh        = (UBYTE)(lba >> 16);
    *unit->drive.lbaMid         = (UBYTE)(lba >> 8);
    *unit->drive.lbaLow         = (UBYTE)(lba);
    *unit->drive.error_features = features;
    *unit->drive.status_command = command;

    return 0;
}

#pragma GCC pop_options

/**
 * ata_set_pio
 *
 * @param unit Pointer t oan IDEUnit struct
 * @param pio pio mode
*/
BYTE ata_set_pio(struct IDEUnit *unit, UBYTE pio) {
    BYTE error = 0;

    if (pio > 4) return IOERR_BADADDRESS;

    if (pio > 0) pio |= 0x08;

    if ((error = write_taskfile_lba(unit,ATA_CMD_SET_FEATURES,0,pio,0x03)) != 0)
        return error;

    if ((error = ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)))
        return error;

    if (ata_check_error(unit)) return IOERR_BADLENGTH;

    return 0;
}

/**
 * scsi_ata_passthrough
 *
 * Handle SCSI ATA PASSTHROUGH (12) command to send ATA commands to the drive
 *
 * @param unit Pointer to an IDEUnit struct
 * @param cmd Pointer to a SCSICmd struct
 * @return non-zero on error
*/
BYTE scsi_ata_passthrough(struct IDEUnit *unit, struct SCSICmd *cmd) {
    struct SCSI_CDB_ATA *cdb = (struct SCSI_CDB_ATA *)cmd->scsi_Command;

    bool byt_blok  = (cdb->length & ATA_BYT_BLOK) ? true : false;
    UBYTE protocol = (cdb->protocol >> 1) & 0x0F;
    UBYTE t_length = cdb->length & ATA_TLEN_MASK;

    ULONG lba   = (cdb->lbaHigh << 16 | cdb->lbaMid << 8 | cdb->lbaLow);
    ULONG count = 0;
    BYTE  error = 0;

    UWORD *src  = NULL;
    UWORD *dest = NULL;

    cmd->scsi_CmdActual = cmd->scsi_CmdLength;

    switch (t_length) {
        case 0x00: // No Data transferred
            break;
        case 0x01: // Transfer length in feature field
            count = cdb->features;
            cdb->features = 0;
            break;
        case 0x02: // Transfer length in sector_count field
            count = cdb->sectorCount;
            cdb->sectorCount = 0;
            break;
        default:
            return IOERR_BADLENGTH;
    }

    if (byt_blok) count *= unit->blockSize;

    if (count > (512*256)) return IOERR_BADLENGTH; // Can't be bothered supporting larger transfers

    if (count > cmd->scsi_Length) return IOERR_BADLENGTH;

    switch (protocol) {
        case ATA_NODATA:
            break;

        case ATA_PIO_IN: // Data to Host
            if (count < 2) return IOERR_BADLENGTH;
            src = (UWORD *)unit->drive.data;
            dest = cmd->scsi_Data;
            break;

        case ATA_PIO_OUT: // Data to Drive
            if (count < 2) return IOERR_BADLENGTH;
            src = cmd->scsi_Data;
            dest = (UWORD *)unit->drive.data;
            break;

        default:
            return IOERR_NOCMD;

    }

    count += (count & 1); // Ensure byte count is even

    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)) {
        ata_save_error(unit);
        return HFERR_SelTimeout;
    }

    if ((error = write_taskfile_lba(unit,cdb->command,lba,cdb->sectorCount,cdb->features)) != 0) {
        ata_save_error(unit);
        return error;
    }

    if (protocol == ATA_PIO_IN || protocol == ATA_PIO_OUT) {
        for (int i = 0; i < count/2; i++) {
            if (i % 512 == 0) {
                if (!ata_wait_drq(unit,ATA_DRQ_WAIT_COUNT,true)) {
                    ata_save_error(unit);
                    return IOERR_UNITBUSY;
                }
            }

                dest[i] = src[i];
        }
    }

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)) {
        ata_save_error(unit);
        return HFERR_BadStatus;
    }

    if (ata_check_error(unit)){
        ata_save_error(unit);
        return IOERR_ABORTED;
    }

    cmd->scsi_Actual = cmd->scsi_Length;

    return 0;
}
