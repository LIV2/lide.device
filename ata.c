// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of liv2ride.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <inline/timer.h>
#include <proto/exec.h>
#include <proto/alib.h>
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

static void ata_read_fast (void *, void *);
static void ata_write_fast (void *, void *);

/**
 * ata_wait_drq
 * 
 * Poll DRQ in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool ata_wait_drq(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->TimeReq;
    Info("wait_drq enter\n");
    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<1000; j++) {
            if ((*unit->drive->status_command & ata_flag_drq) != 0) return true;
            if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) return false;
        }
        tr->tr_time.tv_micro = ATA_DRQ_WAIT_LOOP_US;
        tr->tr_time.tv_secs  = 0;
        tr->tr_node.io_Command = TR_ADDREQUEST;
        DoIO((struct IORequest *)tr);

    }
    Info("wait_drq timeout\n");
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
    struct timerequest *tr = unit->TimeReq;

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<1000; j++) {
            if ((*unit->drive->status_command & ata_flag_busy) == 0) return true;
        }
        tr->tr_time.tv_micro = ATA_BSY_WAIT_LOOP_US;
        tr->tr_time.tv_secs  = 0;
        tr->tr_node.io_Command = TR_ADDREQUEST;
        DoIO((struct IORequest *)tr);
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
    struct timerequest *tr = unit->TimeReq;

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<1000; j++) {
            if ((*unit->drive->status_command & (ata_flag_ready | ata_flag_busy)) == ata_flag_ready) return true;
        }
        tr->tr_time.tv_micro = ATA_RDY_WAIT_LOOP_US;
        tr->tr_time.tv_secs  = 0;
        tr->tr_node.io_Command = TR_ADDREQUEST;
        DoIO((struct IORequest *)tr);
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
    bool ret = false;
    if (*unit->shadowDevHead == select) {
        return false;
    } else if ((*unit->shadowDevHead & 0xF0) != (select & 0xF0)) {
        if (wait) {
            ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT);
            ret = true;
        }
    }
    *unit->drive->devHead = select;
    *unit->shadowDevHead = select;
    return ret;
}

/**
 * ata_identify
 * 
 * Send an IDENTIFY command to the device and place the results in the buffer
 * @param unit Pointer to an IDEUnit struct
 * @param buffer Pointer to the destination buffer
 * @return fals on error
*/
bool ata_identify(struct IDEUnit *unit, UWORD *buffer)
{
    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0; // Select drive
    
    ata_select(unit,drvSel,true);
    //if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
    //        return HFERR_SelTimeout;
    ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT);

    *unit->drive->sectorCount    = 0;
    *unit->drive->lbaLow         = 0;
    *unit->drive->lbaMid         = 0;
    *unit->drive->lbaHigh        = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATA_CMD_IDENTIFY;

    if (!ata_wait_drq(unit,10)) {
        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
            Warn("ATA: IDENTIFY Status: Error\n");
            Warn("ATA: last_error: %08lx\n",&unit->last_error[0]);
            // Save error information
            unit->last_error[0] = *unit->drive->error_features;
            unit->last_error[1] = *unit->drive->lbaHigh;
            unit->last_error[2] = *unit->drive->lbaMid;
            unit->last_error[3] = *unit->drive->lbaLow;
            unit->last_error[4] = *unit->drive->status_command;
        }
        return false;
    }

    if (buffer) {
        UWORD read_data;
        for (int i=0; i<256; i++) {
            read_data = unit->drive->data[i];
            // Interface is byte-swapped, so swap the identify data back.
            buffer[i] = ((read_data >> 8) | (read_data << 8));
        }
    }

    return true;
}

/**
 * ata_init_unit
 * 
 * Initialize a unit, check if it is there and responding
 * @param unit Pointer to an IDEUnit struct
 * @returns false on error
*/
bool ata_init_unit(struct IDEUnit *unit) {
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
    unit->drive = (void *)unit->cd->cd_BoardAddr + offset; // Pointer to drive base

    *unit->shadowDevHead = *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive

    for (int i=0; i<(8*NEXT_REG); i+=NEXT_REG) {
        // Check if the bus is floating (D7/6 pulled-up with resistors)
        if ((i != ata_reg_devHead) && (*((volatile UBYTE *)unit->drive->data + i) & 0xC0) != 0xC0) {
            dev_found = true;
            Trace("INIT: Unit base: %08lx; Drive base %08lx\n",unit, unit->drive);
            break;
        }
    }
    
    if (dev_found == false || !ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT))
        return false;

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) // Allocate buffer for IDENTIFY result
        return false;

    Trace("INIT: IDENTIFY\n");
    if (ata_identify(unit,buf) == true) {
        Info("INIT: ATA Drive found!\n");

        if ((*((UWORD *)buf + ata_identify_capabilities) & ata_capability_lba) == 0) {
            Info("Rejecting drive due to lack of LBA support.\n");
            goto ident_failed;
        }

        unit->cylinders       = *((UWORD *)buf + ata_identify_cylinders);
        unit->heads           = *((UWORD *)buf + ata_identify_heads);
        unit->sectorsPerTrack = *((UWORD *)buf + ata_identify_sectors);
        unit->blockSize       = 512;//*((UWORD *)buf + ata_identify_sectorsize);
        unit->logicalSectors  = *((UWORD *)buf + ata_identify_logical_sectors+1) << 16 | *((UWORD *)buf + ata_identify_logical_sectors);
        unit->blockShift      = 0;
        unit->mediumPresent   = true;
        unit->multiple_count  = (*((UWORD *)buf + ata_identify_multiple) & 0xFF);

        if (unit->multiple_count > 0 && (ata_set_multiple(unit,unit->multiple_count) == 0)) {
            unit->xfer_multiple = true;
        } else {
            unit->xfer_multiple = false;
            unit->multiple_count = 1;
        }

        Info("INIT: Logical sectors: %ld\n",unit->logicalSectors);
        if (unit->logicalSectors >= 16514064) {
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

                unit->device_type     = (buf[0] >> 8) & 0x1F;
                unit->atapi           = true;
                if ((atapi_test_unit_ready(unit)) == 0) {
                    atapi_get_capacity(unit);
                }
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

    Info("INIT: Blockshift: %d\n",unit->blockShift);
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

    *unit->drive->sectorCount    = multiple;
    *unit->drive->lbaLow         = 0;
    *unit->drive->lbaMid         = 0;
    *unit->drive->lbaHigh        = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATA_CMD_SET_MULTIPLE;

    if (!ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT))
        return IOERR_UNITBUSY;
    
    if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
        if (*unit->drive->error_features & ata_err_flag_aborted) {
            return IOERR_ABORTED;
        } else {
            return TDERR_NotSpecified;
        }
    }

    return 0;
}

#pragma GCC optimize ("-O3")

/**
 * ata_read
 * 
 * Read blocks from the unit
 * @param buffer destination buffer
 * @param lba LBA Address
 * @param count Number of blocks to transfer
 * @param actual Pointer to the io requests io_Actual 
 * @param unit Pointer to the unit structure
 * @returns error
*/
BYTE ata_read(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit) {
    Trace("ata_read enter\n");
    ULONG txn_count; // Amount of sectors to transfer in the current READ/WRITE command

    UBYTE command = (unit->xfer_multiple) ? ATA_CMD_READ_MULTIPLE : ATA_CMD_READ;

    if (count == 0) return TDERR_TooFewSecs;

    Trace("ATA: Request sector count: %ld\n",count);

    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            txn_count = MAX_TRANSFER_SECTORS;
        } else {
            txn_count = count;                // Get any remainders
        }
        count -= txn_count;

        BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F);

        ata_select(unit,drvSelHead,false);
        if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
            return HFERR_SelTimeout;
        
        Trace("ATA: XFER Count: %ld, txn_count: %ld\n",count,txn_count);

        *unit->drive->sectorCount    = txn_count; // Count value of 0 indicates to transfer 256 sectors
        *unit->drive->lbaLow         = (UBYTE)(lba);
        *unit->drive->lbaMid         = (UBYTE)(lba >> 8);
        *unit->drive->lbaHigh        = (UBYTE)(lba >> 16);
        *unit->drive->status_command = command;

        while (txn_count) {
            if (!ata_wait_drq(unit,ATA_DRQ_WAIT_COUNT))
                return IOERR_UNITBUSY;

            for (int i = 0; i < unit->multiple_count && txn_count; i++) {
                ata_read_fast((void *)(unit->drive->error_features - 48),(buffer + *actual));

                lba++;
                txn_count--;
                *actual += unit->blockSize;
            }
        }

        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
            unit->last_error[0] = unit->drive->error_features[0];
            unit->last_error[1] = unit->drive->lbaHigh[0];
            unit->last_error[2] = unit->drive->lbaMid[0];
            unit->last_error[3] = unit->drive->lbaLow[0];
            unit->last_error[4] = unit->drive->status_command[0];
            unit->last_error[5] = unit->drive->devHead[0];

            Warn("ATA ERROR!!!\n");
            Warn("last_error: %08lx\n",unit->last_error);
            Warn("LBA: %ld, LastLBA: %ld\n",lba,unit->logicalSectors-1);
            return TDERR_NotSpecified;
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
 * @param actual Pointer to the io requests io_Actual 
 * @param unit Pointer to the unit structure
 * @returns error
*/
BYTE ata_write(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit) {
    Trace("ata_write enter\n");
    ULONG txn_count; // Amount of sectors to transfer in the current READ/WRITE command

    UBYTE command = (unit->xfer_multiple) ? ATA_CMD_WRITE_MULTIPLE : ATA_CMD_WRITE;

    if (count == 0) return TDERR_TooFewSecs;

    Trace("ATA: Request sector count: %ld\n",count);

    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            txn_count = MAX_TRANSFER_SECTORS;
        } else {
            txn_count = count;                // Get any remainders
        }
        count -= txn_count;

        BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F);

        ata_select(unit,drvSelHead,false);
        if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
            return HFERR_SelTimeout;
        

        Trace("ATA: XFER Count: %ld, txn_count: %ld\n",count,txn_count);

        *unit->drive->sectorCount    = txn_count; // Count value of 0 indicates to transfer 256 sectors
        *unit->drive->lbaLow         = (UBYTE)(lba);
        *unit->drive->lbaMid         = (UBYTE)(lba >> 8);
        *unit->drive->lbaHigh        = (UBYTE)(lba >> 16);
        *unit->drive->status_command = command;


        while (txn_count) {
            if (!ata_wait_drq(unit,ATA_DRQ_WAIT_COUNT))
                return IOERR_UNITBUSY;

            for (int i = 0; i < unit->multiple_count && txn_count; i++) {
                ata_write_fast((buffer + *actual),(void *)(unit->drive->error_features - 48));

                lba++;
                txn_count--;
                *actual += unit->blockSize;
            }
        }

        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
            unit->last_error[0] = unit->drive->error_features[0];
            unit->last_error[1] = unit->drive->lbaHigh[0];
            unit->last_error[2] = unit->drive->lbaMid[0];
            unit->last_error[3] = unit->drive->lbaLow[0];
            unit->last_error[4] = unit->drive->status_command[0];
            unit->last_error[5] = unit->drive->devHead[0];

            Warn("ATA ERROR!!!\n");
            Warn("last_error: %08lx\n",unit->last_error);
            Warn("LBA: %ld, LastLBA: %ld\n",lba,unit->logicalSectors-1);
            return TDERR_NotSpecified;
        }
    }

    return 0;
}