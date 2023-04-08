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
#include "scsi.h"
#include "string.h"

#define WAIT_TIMEOUT_MS 0
#define WAIT_TIMEOUT_S  5
#define ATAPI_POLL_TRIES 100

void MyGetSysTime(struct timerequest *tr) {
   tr->tr_node.io_Command = TR_GETSYSTIME;
   DoIO((struct IORequest *)tr);
}

static void wait(int count) {
   for (int i=0; i<count; i++) {
        asm volatile ("nop");
   }
}

/**
 * ata_wait_not_busy
 * 
 * Wait with a timeout for the unit to report that it is not busy
 * @param unit Pointer to an IDEUnit struct
 * @return false on timeout
*/
static bool ata_wait_not_busy(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval then;
    struct timerequest *tr = unit->TimeReq;

    then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
    then.tv_secs  = WAIT_TIMEOUT_S;
    MyGetSysTime(tr);
    AddTime(&then,&tr->tr_time);
#endif
    Trace("wait_not_busy\n");

    while (1) {
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_busy) == 0) return true;
        wait(1000);
#ifndef NOTIMER
        MyGetSysTime(tr);
        if (CmpTime(&then,&tr->tr_time) == 1) {
            Warn("wait_not_busy timeout\n");
            return false;
        }
#endif
    }
}


/**
 * ata_wait_ready
 * 
 * Wait with a timeout for the unit to report that it is ready
 * @param unit Pointer to an IDEUnit struct
 * @return false on timeout
*/
static bool ata_wait_ready(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval then;
    struct timerequest *tr = unit->TimeReq;

    then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
    then.tv_secs  = WAIT_TIMEOUT_S;
    MyGetSysTime(tr);
    AddTime(&then,&tr->tr_time);
#endif
    Trace("wait_ready_enter\n");
    while (1) {
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_ready | ata_flag_busy)) == ata_flag_ready) return true;
        wait(1000);
#ifndef NOTIMER
        MyGetSysTime(tr);
        if (CmpTime(&then,&tr->tr_time) == 1) {
            Warn("wait_ready timeout\n");
            return false;
        }
#endif
    }
}

/**
 * ata_wait_drq
 * 
 * Wait with a timeout for the unit to set drq
 * @param unit Pointer to an IDEUnit struct
 * @return false on timeout
*/
static bool ata_wait_drq(struct IDEUnit *unit) {
    int tries = ATAPI_POLL_TRIES;
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval then;
    struct timerequest *tr;

    // Performing these time functions halves throughput
    // So only do time-out when we're not sure there's a drive there until I figure out a proper fix...
    if (unit->present == false) {
        tr = unit->TimeReq;
        then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
        then.tv_secs  = WAIT_TIMEOUT_S;
        MyGetSysTime(tr);
        AddTime(&then,&tr->tr_time);
    }
#endif

    Trace("wait_drq\n");
    
    while (1) {
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_drq | ata_flag_busy | ata_flag_error)) == ata_flag_drq) return true;
        if (unit->present == false) {
#ifndef NOTIMER
            MyGetSysTime(tr);
            if (CmpTime(&then,&tr->tr_time) == 1) {
                Warn("wait_drq timeout\n");
                return false;
        }
#endif
        wait(1000);
        } else if (unit->atapi == true) {
            if (tries > 0) {
                tries--;
            } else {
                return false;
            }
        }
    }
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
    
    if (*unit->shadowDevHead != drvSel) {
        *unit->drive->devHead = *unit->shadowDevHead = drvSel;
        if (!ata_wait_ready(unit))
            return HFERR_SelTimeout;
    }

    *unit->drive->sectorCount = 0;
    *unit->drive->lbaLow  = 0;
    *unit->drive->lbaMid  = 0;
    *unit->drive->lbaHigh = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATA_CMD_IDENTIFY;

    if (!ata_wait_drq(unit)) {
        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
        Warn("ATA: IDENTIFY Status: Error\n");
        Warn("ATA: last_error: %08lx\n",&unit->last_error[0]);
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
        if ((*((volatile UBYTE *)unit->drive->data + i) & 0xC0) != 0xC0) {
            dev_found = true;
            Trace("INIT: Unit base: %08lx; Drive base %08lx\n",unit, unit->drive);
            break;
        }
    }

    if (dev_found == false || !ata_wait_not_busy(unit))
        return false;

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) // Allocate buffer for IDENTIFY result
        return false;

    Trace("INIT: IDENTIFY\n");
    if (ata_identify(unit,buf) == true) {
        Info("INIT: ATA Drive found!\n");

        unit->cylinders       = *((UWORD *)buf + ata_identify_cylinders);
        unit->heads           = *((UWORD *)buf + ata_identify_heads);
        unit->sectorsPerTrack = *((UWORD *)buf + ata_identify_sectors);
        unit->blockSize       = 512;//*((UWORD *)buf + ata_identify_sectorsize);
        unit->logicalSectors  = *((UWORD *)buf + ata_identify_logical_sectors+1) << 16 | *((UWORD *)buf + ata_identify_logical_sectors);
        unit->blockShift      = 0;
        unit->mediumPresent   = true;

        Info("INIT: Logical sectors: %ld\n",unit->logicalSectors);
        if (unit->logicalSectors >= 16514064) {
            // If a drive is larger than 8GB then the drive will report a geometry of 16383/16/63 (CHS)
            // In this case generate a new Cylinders value
            unit->heads = 16;
            unit->sectorsPerTrack = 255;
            unit->cylinders = (unit->logicalSectors / (16*255));
            Info("INIT: Adjusting geometry, new geometry; 16/255/%ld\n",unit->cylinders);
        }
    } else if (atapi_identify(unit,buf) == true) {
        struct SCSICmd scsi_cmd;
        struct SCSI_CDB_10 cdb;

        if ((buf[0] & 0xC000) != 0x8000) {
            Info("INIT: Not an ATAPI device.\n");
            FreeMem(buf,512);
            return false;
        }

        Info("INIT: ATAPI Drive found!\n");

        unit->device_type     = (buf[0] >> 8) & 0x1F;
        unit->blockSize       = 2048;
        atapi_test_unit_ready(unit);
        if ((atapi_test_unit_ready(unit)) != 0) {
            Trace("ATAPI: Identify - TUR failed");
            goto skip_capacity;
        };

        memset(&cdb,0,sizeof(struct SCSI_CDB_10));
        cdb.operation = SCSI_CMD_READ_CAPACITY_10;

        memset(&scsi_cmd,0,sizeof(struct SCSICmd));
        scsi_cmd.scsi_CmdLength = sizeof(struct SCSI_CDB_10);
        scsi_cmd.scsi_CmdActual = 0;
        scsi_cmd.scsi_Command   = (UBYTE *)&cdb;
        scsi_cmd.scsi_Flags     = SCSIF_READ;
        scsi_cmd.scsi_Data      = buf;
        scsi_cmd.scsi_Length    = 8;
        scsi_cmd.scsi_SenseData = NULL;

        unit->cylinders       = 0;
        unit->heads           = 0;
        unit->sectorsPerTrack = 0;
        unit->logicalSectors  = 0;
        unit->blockShift      = 0;
        unit->atapi           = true;

        if ((atapi_packet(&scsi_cmd,unit) == 0)) {
            unit->logicalSectors  = (*(ULONG *)buf) + 1;
        }

skip_capacity:

        Info("INIT: LBAs %ld Blocksize: %ld\n",unit->logicalSectors,unit->blockSize);

    } else {
        Warn("INIT: IDENTIFY failed\n");
        // Command failed with a timeout or error
        FreeMem(buf,512);
        return false;
    }

    // It's faster to shift than divide
    // Figure out how many shifts are needed for the equivalent divide
    if (unit->blockSize == 0) {
        Warn("INIT: Error! blockSize is 0\n");
        if (buf) FreeMem(buf,512);
        return false;
    }

    while ((unit->blockSize >> unit->blockShift) > 1) {
        unit->blockShift++;
    }
    Info("INIT: Blockshift: %d",unit->blockShift);
    unit->present = true;

    if (buf) FreeMem(buf,512);
    return true;
}

/**
 * ata_transfer
 * 
 * Read/write a block from/to the unit
 * @param buffer Source buffer
 * @param lba LBA Address
 * @param count Number of blocks to transfer
 * @param actual Pointer to the io requests io_Actual 
 * @param unit Pointer to the unit structure
 * @param direction READ/WRITE
 * @returns error
*/
BYTE ata_transfer(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit, enum xfer_dir direction) {
if (direction == READ) {
    Trace("ata_read\n");
} else {
    Trace("ata_write\n");
}
    ULONG subcount = 0;
    ULONG offset = 0;
    *actual = 0;

    if (count == 0) return TDERR_TooFewSecs;

    Trace("ATA: Request sector count: %ld\n",count);

    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            subcount = MAX_TRANSFER_SECTORS;
        } else {
            subcount = count;                // Get any remainders
        }
        count -= subcount;

        BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F);

        if (*unit->shadowDevHead != drvSelHead) {
            *unit->drive->devHead = *unit->shadowDevHead = drvSelHead;
            if (!ata_wait_ready(unit))
                return HFERR_SelTimeout;
        }


        Trace("ATA: XFER Count: %ld, Subcount: %ld\n",count,subcount);

        *unit->drive->sectorCount    = subcount; // Count value of 0 indicates to transfer 256 sectors
        *unit->drive->lbaLow         = (UBYTE)(lba);
        *unit->drive->lbaMid         = (UBYTE)(lba >> 8);
        *unit->drive->lbaHigh        = (UBYTE)(lba >> 16);
        *unit->drive->error_features = 0;
        *unit->drive->status_command = (direction == READ) ? ATA_CMD_READ : ATA_CMD_WRITE;

        for (int block = 0; block < subcount; block++) {
            if (!ata_wait_drq(unit))
                return IOERR_UNITBUSY;

            if (direction == READ) {
#if SLOWXFER
                for (int i=0; i<(unit->blockSize / 2); i++) {
                    ((UWORD *)buffer)[offset] = *(UWORD *)unit->drive->data;
                    offset++;
                }
#else
                ata_read_fast((void *)(unit->drive->error_features - 48),(buffer + offset));
                offset += 512;
#endif

            } else {
#if SLOWXFER
                for (int i=0; i<(unit->blockSize / 2); i++) {
                    *(UWORD *)unit->drive->data = ((UWORD *)buffer)[offset];
                    offset++;
                }
#else
                ata_write_fast((buffer + offset),(void *)(unit->drive->error_features - 48));
                offset += 512;
#endif
            }

            *actual += unit->blockSize;
        }

        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
            unit->last_error[0] = unit->drive->error_features[0];
            unit->last_error[1] = unit->drive->lbaHigh[0];
            unit->last_error[2] = unit->drive->lbaMid[0];
            unit->last_error[3] = unit->drive->lbaLow[0];
            unit->last_error[4] = unit->drive->status_command[0];
            unit->last_error[5] = unit->drive->devHead[0];

            Info("ATA ERROR!!!");
            Info("last_error: %08lx\n",unit->last_error);
            Info("LBA: %ld, LastLBA: %ld\n",lba,unit->logicalSectors-1);
            return TDERR_NotSpecified;
        }

        lba += subcount;

    }

    return 0;
}


#pragma GCC optimize ("-fomit-frame-pointer")
/**
 * ata_read_fast
 * 
 * Fast copy of a 512-byte sector using movem
 * Adapted from the open source at_apollo_device by Frédéric REQUIN
 * https://github.com/fredrequin/at_apollo_device
 * 
 * NOTE! source needs to be 48 bytes before the end of the data port!
 * The 68000 does an extra memory access at the end of a movem instruction!
 * Source: https://github.com/prb28/m68k-instructions-documentation/blob/master/instructions/movem.md
 * 
 * With the src of end-48 the error reg will be harmlessly read instead.
 * 
 * @param source Pointer to drive data port
 * @param destination Pointer to source buffer
*/
void ata_read_fast (void *source, void *destinaton) {
    asm volatile ("moveq  #48,d7\n\t"

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 1
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 2
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 3
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 4

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 5
    
    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 6

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                  
    "add.l   d7,%1\n\t" // 7

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   
    "add.l   d7,%1\n\t" // 8

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   
    "add.l   d7,%1\n\t" // 9

    "movem.l (%0),d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    "add.l   d7,%1\n\t" // 10

    "movem.l 16(%0),d0-d6/a1\n\t"
    "movem.l d0-d6/a1,(%1)\n\t"
    :
    :"a" (source),"a" (destinaton)
    :"a1","a2","a3","a4","a6","d0","d1","d2","d3","d4","d5","d6","d7"
    );
}

/**
 * ata_write_fast
 * 
 * Fast copy of a 512-byte sector using movem
 * Adapted from the open source at_apollo_device by Frédéric REQUIN
 * https://github.com/fredrequin/at_apollo_device
 * 
 * @param source Pointer to source buffer
 * @param destination Pointer to drive data port
*/
void ata_write_fast (void *source, void *destinaton) {
    asm volatile (
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"
    
    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                  

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"                   

    "movem.l (%0)+,d0-d6/a1-a4/a6\n\t"
    "movem.l d0-d6/a1-a4/a6,(%1)\n\t"

    "movem.l (%0)+,d0-d6/a1\n\t"
    "movem.l d0-d6/a1,(%1)\n\t"
    :
    :"a" (source),"a" (destinaton)
    :"a1","a2","a3","a4","a6","d0","d1","d2","d3","d4","d5","d6","d7"
    );
}

#pragma GCC reset_options


/**
 * atapi_identify
 * 
 * Send an "IDENTIFY PACKET DEVICE" command and read the results into a buffer
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param buffer Pointer to the destination buffer
 * @returns True on success, false on failure
*/
bool atapi_identify(struct IDEUnit *unit, UWORD *buffer) {

    UBYTE drvSel = (unit->primary) ? 0xE0 : 0xF0; // Select drive

    if (*unit->shadowDevHead != drvSel) {
        *unit->drive->devHead = *unit->shadowDevHead = drvSel;
        if (!ata_wait_ready(unit))
            return HFERR_SelTimeout;
    }

    *unit->drive->sectorCount = 0;
    *unit->drive->lbaLow  = 0;
    *unit->drive->lbaMid  = 0;
    *unit->drive->lbaHigh = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATAPI_CMD_IDENTIFY;

    if (!ata_wait_drq(unit)) {
        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
        Warn("ATAPI: IDENTIFY Status: Error\n");
        Warn("ATAPI: last_error: %08lx\n",&unit->last_error[0]);
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
            buffer[i] = ((read_data >> 8) | (read_data << 8));
        }
    }

    return true;
}

/**
 * atapi_translate
 * 
 * Translate TD commands to ATAPI and issue them to the device
 * 
 * @param io_Data Pointer to the data buffer
 * @param lba LBA to transfer
 * @param count Number of LBAs to transfer
 * @param io_Actual Pointer to the io_Actual field of the ioreq
 * @param unit Pointer to the IDE Unit
 * @param direction Transfer direction
 * @returns error
*/
BYTE atapi_translate(APTR io_Data,ULONG lba, ULONG count, ULONG *io_Actual, struct IDEUnit *unit, enum xfer_dir direction)
{
    struct SCSI_CDB_10 cdb;
    struct SCSICmd atapi_cmd;

    if (count == 0) {
        return IOERR_BADLENGTH;
    }

    BYTE ret = 0;
    atapi_cmd.scsi_Command     = (UBYTE *)&cdb;
    atapi_cmd.scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
    atapi_cmd.scsi_CmdActual   = 0;
    atapi_cmd.scsi_Flags       = (direction == READ) ? SCSIF_READ : SCSIF_WRITE;
    atapi_cmd.scsi_Data        = io_Data;
    atapi_cmd.scsi_Length      = count * unit->blockSize;
    atapi_cmd.scsi_SenseData   = NULL;
    atapi_cmd.scsi_SenseLength = 0;

    cdb.operation = (direction == READ) ? SCSI_CMD_READ_10 : SCSI_CMD_WRITE_10;
    cdb.control   = 0;
    cdb.flags     = 0;
    cdb.group     = 0;
    cdb.lba       = lba;
    cdb.length    = (UWORD)count;

    ret = atapi_packet(&atapi_cmd,unit);
    *io_Actual = atapi_cmd.scsi_Actual;
    return ret;
}

/**
 * atapi_packet
 * 
 * Send a SCSICmd to an ATAPI device
 * 
 * @param cmd Pointer to a SCSICmd struct
 * @param unit Pointer to the IDEUnit
 * @returns error
*/
BYTE atapi_packet(struct SCSICmd *cmd, struct IDEUnit *unit) {
    Trace("atapi_packet\n");
    ULONG byte_count;
    UWORD data;
    UBYTE senseKey;
    UBYTE operation = ((struct SCSI_CDB_10 *)cmd->scsi_Command)->operation;

    volatile UBYTE *status = unit->drive->status_command;

    if (cmd->scsi_CmdLength > 12 || cmd->scsi_CmdLength < 6) return HFERR_BadStatus;

    cmd->scsi_Actual = 0;

    BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0);

    if (*unit->shadowDevHead != drvSelHead) {
        *unit->drive->devHead = *unit->shadowDevHead = drvSelHead;
        if (!ata_wait_ready(unit))
            return HFERR_SelTimeout;
    }

    while ((*status & ata_flag_ready) == 0);

    if (cmd->scsi_Length > 65534) {
        byte_count = 65534;
    } else {
        byte_count = cmd->scsi_Length;
    }
    *unit->drive->sectorCount    = 0;
    *unit->drive->lbaLow         = 0;
    *unit->drive->lbaMid         = byte_count;
    *unit->drive->lbaHigh        = byte_count >> 8;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATAPI_CMD_PACKET;

    // HP0
    ata_wait_not_busy(unit);

    if ((*status & ata_flag_drq) == 0) {
        Info("ATAPI: Bad state, Packet command refused?!\n");
        return HFERR_Phase;
    }
        
    if ((*unit->drive->sectorCount & 0x03) == atapi_flag_cd) {
        Trace("ATAPI: Start command phase\n");
    } else {
        Trace("ATAPI: Failed command phase\n");
        return HFERR_Phase;
    }
    // HP1
    for (int i=0; i < (cmd->scsi_CmdLength/2); i++)
    {
        data = *((UWORD *)cmd->scsi_Command + i);
        *unit->drive->data = data;
        Trace("ATAPI: CMD Word: %ld\n",data);
    }

    // ATAPI requires 12 command bytes transferred, pad out our 6 and 10 byte CDBs
    if (cmd->scsi_CmdLength < 12)
    {
        for (int i = cmd->scsi_CmdLength; i < 12; i+=2) {
            *unit->drive->data = 0x00;
            Info("ATAPI: CMD Word: %ld\n",0);
        }
    }
    if (*status & ata_flag_error) {
        senseKey = *unit->drive->error_features >> 4;
        Info("ATAPI ERROR!\n");
        Info("Sense Key: %02lx\n",senseKey);
        Info("Error: %02lx\n",*status);
        Info("Interrupt reason: %02lx\n",*unit->drive->sectorCount);
        cmd->scsi_Status = 2;
        if (cmd->scsi_Flags & (SCSIF_AUTOSENSE | SCSIF_OLDAUTOSENSE) && cmd->scsi_SenseData != NULL) {
            cmd->scsi_SenseData[0] = senseKey;
            cmd->scsi_SenseActual = 1;
        }
        return HFERR_BadStatus;
    }

    cmd->scsi_CmdActual = cmd->scsi_CmdLength;
    
    ULONG index = 0;

    if (cmd->scsi_Length == 0) goto xferdone;

    while (1) {
        ata_wait_not_busy(unit);

        if (!ata_wait_drq(unit)) {
            // Some commands (like mode-sense) may return less data than the length specified
            // Read/Write actual should always match, if not throw an error.
            if (operation == SCSI_CMD_READ_10 || operation == SCSI_CMD_WRITE_10) {
                if (cmd->scsi_Actual != cmd->scsi_Length) {
                    return IOERR_ABORTED;
                }
            }
            goto xferdone;
        }

        byte_count = *unit->drive->lbaHigh << 8 | *unit->drive->lbaMid;
        byte_count += (byte_count & 1); // Make sure it's an even count
        for (int i=0; i < (byte_count / 2); i++) {
            if (cmd->scsi_Flags & SCSIF_READ) {
                cmd->scsi_Data[index] = *unit->drive->data;
            } else {
                *unit->drive->data = cmd->scsi_Data[index];
            }
            index++;
            cmd->scsi_Actual+=2;
            if (cmd->scsi_Actual >= cmd->scsi_Length) {
                Trace("Ending command %ld with %ld bytes remaining.\n", (ULONG)(*(UBYTE *)cmd->scsi_Command),(byte_count - (i*2)) -2);
                Trace("SCSI Len: %ld Buf Len %ld\n",((struct SCSI_CDB_10 *)cmd->scsi_Command)->length * 2048, cmd->scsi_Length);
                goto xferdone;
            }
        }
    }
xferdone:
    if (*status & ata_flag_error) {
        senseKey = *unit->drive->error_features >> 4;
        Warn("ATAPI ERROR!\n");
        Warn("Sense Key: %02lx\n",senseKey);
        Warn("Error: %02lx\n",*status);
        Warn("Interrupt reason: %02lx\n",*unit->drive->sectorCount);
        cmd->scsi_Status = 2;
        if (cmd->scsi_Flags & (SCSIF_AUTOSENSE | SCSIF_OLDAUTOSENSE) && cmd->scsi_SenseData != NULL) {
            // TODO: get extended sense data from drive
            cmd->scsi_SenseData[0] = senseKey;
            cmd->scsi_SenseActual = 1;
        }
        return HFERR_BadStatus;
    }
    if (cmd->scsi_Actual != cmd->scsi_Length) {
        Warn("Command %lx Transferred %ld of %ld requested for %ld blocks\n",(ULONG)(*(UBYTE *)cmd->scsi_Command),cmd->scsi_Actual, cmd->scsi_Length, ((struct SCSI_CDB_10 *)cmd->scsi_Command)->length);
    }
    Trace("exit atapi_packet\n");
    return 0;
}

UBYTE atapi_test_unit_ready(struct IDEUnit *unit) {
    struct SCSI_CDB_10 *cdb = NULL;
    struct SCSICmd *cmd = NULL;
    UBYTE senseKey = 0;
    UBYTE ret;

    if ((cdb = AllocMem(sizeof(struct SCSI_CDB_10),MEMF_ANY|MEMF_CLEAR)) == NULL) {
        Trace("ATAPI: TUR AllocMem failed.\n");
        return TDERR_NoMem;
    }

    if ((cmd = AllocMem(sizeof(struct SCSICmd),MEMF_ANY|MEMF_CLEAR)) == NULL) {
        Trace("ATAPI: TUR AllocMem failed.\n");
        FreeMem(cdb,sizeof(struct SCSI_CDB_10));
        return TDERR_NoMem;
    }

    cdb->operation        = SCSI_CMD_TEST_UNIT_READY;
    cmd->scsi_Command     = (UBYTE *)cdb;
    cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
    cmd->scsi_Length      = 0;
    cmd->scsi_Data        = NULL;
    cmd->scsi_SenseData   = &senseKey;
    cmd->scsi_SenseLength = 1;
    cmd->scsi_Flags       = SCSIF_AUTOSENSE | SCSIF_READ;

    ret = atapi_packet(cmd,unit);

    Trace("ATAPI: TUR Return: %ld SenseKey %lx\n",ret,senseKey);
    if (cmd->scsi_Status == 0 && unit->mediumPresent != true) {
        unit->mediumPresent = true;
        unit->change_count++;
    } else if (cmd->scsi_Status == 2) {
        ret = 0;
        if ((senseKey == 2) & (unit->mediumPresent == true)) {
            Trace("ATAPI: marking media as absent");
            unit->mediumPresent = false;
            unit->change_count++;
        } else if ((senseKey == 6) & (unit->mediumPresent = false)) {
            Trace("ATAPI: marking media as present");
            unit->mediumPresent = true;
            unit->change_count++;
        }
    }

    if (senseKey == 6) {
        atapi_request_sense(unit);
    }
    if (cdb) FreeMem(cdb,sizeof(struct SCSI_CDB_10));
    if (cmd) FreeMem(cmd,sizeof(struct SCSICmd));

    return ret;
}


UBYTE atapi_request_sense(struct IDEUnit *unit) {
    struct SCSI_CDB_10 *cdb = NULL;
    struct SCSICmd *cmd = NULL;
    UBYTE senseKey = 0;
    UBYTE ret;

    if ((cdb = AllocMem(sizeof(struct SCSI_CDB_10),MEMF_ANY|MEMF_CLEAR)) == NULL) {
        Trace("ATAPI: RS AllocMem failed.\n");
        return TDERR_NoMem;
    }

    if ((cmd = AllocMem(sizeof(struct SCSICmd),MEMF_ANY|MEMF_CLEAR)) == NULL) {
        Trace("ATAPI: RS AllocMem failed.\n");
        FreeMem(cdb,sizeof(struct SCSI_CDB_10));
        return TDERR_NoMem;
    }

    cdb->operation        = SCSI_CMD_REQUEST_SENSE;
    cmd->scsi_Command     = (UBYTE *)cdb;
    cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
    cmd->scsi_Length      = 0;
    cmd->scsi_Data        = NULL;
    cmd->scsi_SenseData   = &senseKey;
    cmd->scsi_SenseLength = 1;
    cmd->scsi_Flags       = SCSIF_AUTOSENSE | SCSIF_READ;

    ret = atapi_packet(cmd,unit);
    Trace("ATAPI RS: Status %lx",ret);
    if (cdb) FreeMem(cdb,sizeof(struct SCSI_CDB_10));
    if (cmd) FreeMem(cmd,sizeof(struct SCSICmd));

    return ret;
}