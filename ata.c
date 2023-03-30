#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <inline/timer.h>
#include <proto/exec.h>
#include <proto/alib.h>
#include <proto/expansion.h>

#include <stdbool.h>
#include <stdio.h>

#include "debug.h"
#include "device.h"
#include "ata.h"

#define WAIT_TIMEOUT_MS 500

void MyGetSysTime(struct timerequest *tr) {
   tr->tr_node.io_Command = TR_GETSYSTIME;
   DoIO((struct IORequest *)tr);
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
        if (*unit->drive->status_command & ata_flag_error) {
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
    struct ExecBase *SysBase = unit->SysBase;

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
            Trace("Something there?\n");
            Trace("Unit base: %08lx; Drive base %08lx\n",unit, unit->drive);
            break;
        }
    }

    if (dev_found == false || !ata_wait_not_busy(unit))
        return false;

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) // Allocate buffer for IDENTIFY result
        return false;

    Trace("Send IDENTIFY\n");
    if (ata_identify(unit,buf) == false) {
        Warn("IDENTIFY Timeout\n");
        // Command failed with a timeout or error
        FreeMem(buf,512);
        return false;
    }
    Info("Drive found!\n");

    unit->cylinders       = *((UWORD *)buf + ata_identify_cylinders);
    unit->heads           = *((UWORD *)buf + ata_identify_heads);
    unit->sectorsPerTrack = *((UWORD *)buf + ata_identify_sectors);
    unit->blockSize       = *((UWORD *)buf + ata_identify_sectorsize);
    unit->blockShift      = 0;

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
*/
BYTE ata_transfer(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit, enum xfer_dir direction) {
if (direction == READ) {
    Trace("ata_read");
} else {
    Trace("ata_write");
}
    ULONG subcount = 0;
    ULONG offset = 0;
    *actual = 0;

    if (count == 0) return TDERR_TooFewSecs;

    BYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;
    *unit->drive->devHead        = (UBYTE)(drvSel | ((lba >> 24) & 0x0F));

    Trace("Request sector count: %ld\n",count);

    if (!ata_wait_not_busy(unit))
        return HFERR_BadStatus;

    // None of this max_transfer 1FE00 nonsense!
    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            subcount = MAX_TRANSFER_SECTORS;
        } else {
            subcount = count;                // Get any remainders
        }
        count -= subcount;

        Trace("XFER Count: %ld, Subcount: %ld\n",count,subcount);

        *unit->drive->sectorCount    = subcount; // Count value of 0 indicates to transfer 256 sectors
        *unit->drive->lbaLow         = (UBYTE)(lba);
        *unit->drive->lbaMid         = (UBYTE)(lba >> 8);
        *unit->drive->lbaHigh        = (UBYTE)(lba >> 16);
        *unit->drive->error_features = 0;
        *unit->drive->status_command = (direction == READ) ? ATA_CMD_READ : ATA_CMD_WRITE;

        for (int block = 0; block < subcount; block++) {
            if (!ata_wait_drq(unit))
                return HFERR_BadStatus;


            if (direction == READ) {

                //for (int i=0; i<(unit->blockSize / 2); i++) {
                //    ((UWORD *)buffer)[offset] = *(UWORD *)unit->drive->data;
                //    offset++;
                //}
                read_fast((void *)(unit->drive->error_features - 48),(buffer + offset));
                offset += 512;

            } else {
                //for (int i=0; i<(unit->blockSize / 2); i++) {
                //    *(UWORD *)unit->drive->data = ((UWORD *)buffer)[offset];
                //    offset++;
                //}
                write_fast((buffer + offset),(void *)(unit->drive->error_features - 48));
                offset += 512;

            }


            *actual += unit->blockSize;
        }

        if (*unit->drive->status_command & ata_flag_error) {
            unit->last_error[0] = unit->drive->error_features[0];
            unit->last_error[1] = unit->drive->lbaHigh[0];
            unit->last_error[2] = unit->drive->lbaMid[0];
            unit->last_error[3] = unit->drive->lbaLow[0];
            unit->last_error[4] = unit->drive->status_command[0];

            Info("ATA ERROR!!!");
            Info("last_error: %08lx\n",unit->last_error);
            Info("LBA: %ld, LastLBA: %ld\n",lba,(unit->sectorsPerTrack * unit->cylinders * unit->heads));
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