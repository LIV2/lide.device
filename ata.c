#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <inline/timer.h>
#include <proto/exec.h>
#include <proto/alib.h>
#include <proto/expansion.h>
#include <exec/errors.h>

#include <stdbool.h>
#include <stdio.h>

#include "debug.h"
#include "device.h"
#include "ata.h"
#include "scsi.h"
#include <string.h>

#define WAIT_TIMEOUT_MS 500

void MyGetSysTime(struct timerequest *tr) {
   tr->tr_node.io_Command = TR_GETSYSTIME;
   DoIO((struct IORequest *)tr);
}

BYTE atapi_translate(APTR io_Data,ULONG lba, ULONG count, ULONG *io_Actual, struct IDEUnit *unit, enum xfer_dir direction)
{
    struct SCSI_CDB_10 cdb;
    struct SCSICmd atapi_cmd;

    if (count == 0) {
        return IOERR_BADLENGTH;
    }

    BYTE ret = 0;
    atapi_cmd.scsi_Command = (UBYTE *)&cdb;
    atapi_cmd.scsi_CmdLength = sizeof(struct SCSI_CDB_10);
    atapi_cmd.scsi_CmdActual = 0;
    atapi_cmd.scsi_Flags = (direction == READ) ? SCSIF_READ : SCSIF_WRITE;
    atapi_cmd.scsi_Data = io_Data;
    atapi_cmd.scsi_Length = count * unit->blockSize;

    cdb.operation = (direction == READ) ? SCSI_CMD_READ_10 : SCSI_CMD_WRITE_10;
    cdb.control = 0;
    cdb.flags   = 0;
    cdb.group   = 0;
    cdb.lba     = lba;
    cdb.length  = (UWORD)count;

    ret = atapi_packet(&atapi_cmd,unit);
    *io_Actual = atapi_cmd.scsi_Actual;
    return ret;
}

/**
 * ata_wait_not_busy
 * 
 * Wait with a timeout for the unit to report that it is not busy
 *
 * returns false on timeout
*/
static bool ata_wait_not_busy(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval then;
    struct timerequest *tr = unit->TimeReq;

    then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
    then.tv_secs  = 0;
    MyGetSysTime(tr);
    AddTime(&then,&tr->tr_time);
#endif
    Trace("wait_not_busy\n");

    while (1) {
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_busy) == 0) return true;
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
 * 
 * returns false on timeout
*/
static bool ata_wait_ready(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval then;
    struct timerequest *tr = unit->TimeReq;

    then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
    then.tv_secs  = 0;
    MyGetSysTime(tr);
    AddTime(&then,&tr->tr_time);
#endif
    Trace("wait_ready_enter\n");
    while (1) {
        if (*(volatile BYTE *)unit->drive->status_command & ata_flag_ready) return true;

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
 * 
 * returns false on timeout
*/
static bool ata_wait_drq(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval then;
    struct timerequest *tr;

    // Performing these time functions halves throughput
    // So only do time-out when we're not sure there's a drive there
    if (unit->present == false) {
        tr = unit->TimeReq;
        then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
        then.tv_secs  = 0;
        MyGetSysTime(tr);
        AddTime(&then,&tr->tr_time);
    }
#endif

    Trace("wait_drq\n");
    
    while (1) {
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_drq | ata_flag_busy | ata_flag_error)) == ata_flag_drq) return true;
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_error) != 0) {
            Info("ATAPI Error: %02lx\n",(*(volatile BYTE *)unit->drive->status_command));
            Info("ATAPI Error reg: %02lx\n",(*(volatile BYTE *)unit->drive->error_features));
            Info("ATAPI Interrupt reg: %02lx\n",(*(volatile BYTE *)unit->drive->sectorCount));
            return false;
        }
        if (unit->present == false) {
#ifndef NOTIMER
            MyGetSysTime(tr);
            if (CmpTime(&then,&tr->tr_time) == 1) {
                Warn("wait_drq timeout\n");
                return false;
        }
#endif

        }
    }
}


/**
 * ata_identify
 * 
 * Send an IDENTIFY command to the device and place the results in the buffer
 * 
 * returns fals on error
*/
bool ata_identify(struct IDEUnit *unit, UWORD *buffer)
{
    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive
    *unit->drive->sectorCount = 0;
    *unit->drive->lbaLow  = 0;
    *unit->drive->lbaMid  = 0;
    *unit->drive->lbaHigh = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATA_CMD_IDENTIFY;

    if (!ata_wait_drq(unit)) {
        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
        Warn("IDENTIFY Status: Error\n");
        Warn("last_error: %08lx\n",&unit->last_error[0]);
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


bool atapi_identify(struct IDEUnit *unit, UWORD *buffer) {
    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive
    *unit->drive->sectorCount = 0;
    *unit->drive->lbaLow  = 0;
    *unit->drive->lbaMid  = 0;
    *unit->drive->lbaHigh = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATAPI_CMD_IDENTIFY;

    if (!ata_wait_drq(unit)) {
        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
        Warn("IDENTIFY Status: Error\n");
        Warn("last_error: %08lx\n",&unit->last_error[0]);
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
 * 
 * returns false on error
*/
bool ata_init_unit(struct IDEUnit *unit) {
    unit->cylinders       = 0;
    unit->heads           = 0;
    unit->sectorsPerTrack = 0;
    unit->blockSize       = 0;
    unit->present         = false;

    ULONG offset;
    UWORD *buf;
    bool dev_found = false;

    offset = (unit->channel == 0) ? CHANNEL_0 : CHANNEL_1;
    unit->drive = (void *)unit->cd->cd_BoardAddr + offset; // Pointer to drive base

    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive

    for (int i=0; i<(8*NEXT_REG); i+=NEXT_REG) {
        // Check if the bus is floating (D7/6 pulled-up with resistors)
        if ((*((volatile UBYTE *)unit->drive->data + i) & 0xC0) != 0xC0) {
            dev_found = true;
            Trace("Unit base: %08lx; Drive base %08lx\n",unit, unit->drive);
            break;
        }
    }

    if (dev_found == false || !ata_wait_not_busy(unit))
        return false;

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) // Allocate buffer for IDENTIFY result
        return false;

    Trace("IDENTIFY\n");
    if (ata_identify(unit,buf) == true) {
        Info("ATA Drive found!\n");

        unit->cylinders       = *((UWORD *)buf + ata_identify_cylinders);
        unit->heads           = *((UWORD *)buf + ata_identify_heads);
        unit->sectorsPerTrack = *((UWORD *)buf + ata_identify_sectors);
        unit->blockSize       = *((UWORD *)buf + ata_identify_sectorsize);
        unit->logicalSectors  = *((UWORD *)buf + ata_identify_logical_sectors+1) << 16 | *((UWORD *)buf + ata_identify_logical_sectors);
        unit->blockShift      = 0;
        Info("Logical sectors: %ld\n",unit->logicalSectors);
        if (unit->logicalSectors >= 16514064) {
            // If a drive is larger than 8GB then the drive will report a geometry of 16383/16/63 (CHS)
            // In this case generate a new Cylinders value
            unit->heads = 16;
            unit->sectorsPerTrack = 255;
            unit->cylinders = (unit->logicalSectors / (16*255));
            Info("Adjusting geometry, new geometry; 16/255/%ld\n",unit->cylinders);
        }
    } else if (atapi_identify(unit,buf) == true) {
        struct SCSICmd scsi_cmd;
        struct SCSI_CDB_10 cdb;

        if ((buf[0] & 0xC000) != 0x8000) {
            Info("Not an ATAPI device.\n");
            FreeMem(buf,512);
            return false;
        }

        Info("ATAPI Drive found!\n");

        unit->device_type     = (buf[0] >> 8) & 0x1F;
        unit->atapi_byteCount = (buf[126]);

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

        if ((atapi_packet(&scsi_cmd,unit) != 0)) {
            Info("Atapi packet command failed!\n");
            FreeMem(buf,512);
            return false;
        }

        unit->cylinders       = 0;
        unit->heads           = 0;
        unit->sectorsPerTrack = 0;
        unit->logicalSectors  = (*(ULONG *)buf) + 1;
        unit->blockSize       = *((ULONG *)buf + 1);
        unit->atapi           = true;

        Info("LBAs %ld Blocksize: %ld\n",unit->logicalSectors,*((ULONG *)buf + 1));

    } else {
        Warn("IDENTIFY failed\n");
        // Command failed with a timeout or error
        FreeMem(buf,512);
        return false;
    }

    // It's faster to shift than divide
    // Figure out how many shifts are needed for the equivalent divide
    if (unit->blockSize == 0) {
        Warn("Error! blockSize is 0\n");
        if (buf) FreeMem(buf,512);
        return false;
    }

    while ((unit->blockSize >> unit->blockShift) > 1) {
        unit->blockShift++;
    }
    Info("Blockshift: %ld",unit->blockShift);
    unit->present = true;

    if (buf) FreeMem(buf,512);
    return true;
}

BYTE atapi_packet(struct SCSICmd *cmd, struct IDEUnit *unit) {
    Trace("atapi_packet\n");
    ULONG byte_count;
    UWORD data;
    ULONG count;

    if (cmd->scsi_CmdLength > 12) return HFERR_BadStatus;
    BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0);

    if (*unit->drive->devHead != drvSelHead) {
        *unit->drive->devHead = drvSelHead;
        if (!ata_wait_not_busy(unit))
            return HFERR_SelTimeout;
    }

    *unit->drive->sectorCount    = 0;
    *unit->drive->lbaLow         = 0;
    *unit->drive->lbaMid         = cmd->scsi_Length;
    *unit->drive->lbaHigh        = cmd->scsi_Length >> 8;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATAPI_CMD_PACKET;

    if (!ata_wait_drq(unit))
        return IOERR_UNITBUSY;

    for (int i=0; i < (cmd->scsi_CmdLength/2); i++)
    {
        data = *((UWORD *)cmd->scsi_Command + i);
        *unit->drive->data = data;
        Info("CMD Word: %ld\n",data);
    }
    if (cmd->scsi_CmdLength < 12)
    {
        for (int i = cmd->scsi_CmdLength; i < 12; i+=2) {
            *unit->drive->data = 0x00;
        }
    }
    cmd->scsi_CmdActual = cmd->scsi_CmdLength;

    byte_count = *unit->drive->lbaHigh << 8 | *unit->drive->lbaMid;

    count = cmd->scsi_Length;

    while ((*unit->drive->sectorCount & 1) == 1) {
        Info("*");
    }

    Info("Do XFER count %ld len %ld\n",count,cmd->scsi_Length);
    while (count > 0) {
        for (int i=0; i < cmd->scsi_Length / 2; i++) {
            if ((i % byte_count) == 0) {
                if (!ata_wait_drq(unit))
                    return IOERR_UNITBUSY;
            }
            if (cmd->scsi_Flags & SCSIF_READ) {
                *(cmd->scsi_Data + i) = *unit->drive->data;
                Trace("%04lx\n",cmd->scsi_Data[i]);
            } else {
                *unit->drive->data = cmd->scsi_Data[i];
            }
            cmd->scsi_Actual +=2;
            count -= 2;
        }
    }

    return 0;
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

    Trace("Request sector count: %ld\n",count);

    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            subcount = MAX_TRANSFER_SECTORS;
        } else {
            subcount = count;                // Get any remainders
        }
        count -= subcount;

        BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F);

        if (*unit->drive->devHead != drvSelHead) {
            *unit->drive->devHead = drvSelHead;
            if (!ata_wait_not_busy(unit))
                return HFERR_SelTimeout;
        }

        Trace("XFER Count: %ld, Subcount: %ld\n",count,subcount);

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
                read_fast((void *)(unit->drive->error_features - 48),(buffer + offset));
                offset += 512;
#endif

            } else {
#if SLOWXFER
                for (int i=0; i<(unit->blockSize / 2); i++) {
                    *(UWORD *)unit->drive->data = ((UWORD *)buffer)[offset];
                    offset++;
                }
#else
                write_fast((buffer + offset),(void *)(unit->drive->error_features - 48));
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
 * read_fast
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
void read_fast (void *source, void *destinaton) {
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
 * write_fast
 * 
 * Fast copy of a 512-byte sector using movem
 * Adapted from the open source at_apollo_device by Frédéric REQUIN
 * https://github.com/fredrequin/at_apollo_device
 * 
 * @param source Pointer to source buffer
 * @param destination Pointer to drive data port
*/
void write_fast (void *source, void *destinaton) {
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