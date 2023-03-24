#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <inline/timer.h>
#include <proto/exec.h>
#include <proto/expansion.h>

#include <stdbool.h>
#include <stdio.h>

#include "device.h"
#include "ata.h"

#if DEBUG
#include <clib/debug_protos.h>
#endif

#define WAIT_TIMEOUT_MS 500

/**
 * ata_wait_not_busy
 * 
 * Wait with a timeout for the unit to report that it is not busy
 *
 * returns false on timeout
*/
bool ata_wait_not_busy(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval start, now;
    GetSysTime(&start);
#endif
#if DEBUG >= 2
    KPrintF("wait_not_busy\n");
#endif

    while (1) {
        if ((*(volatile BYTE *)unit->drive->status_command & ata_busy) == 0) return true;

#ifndef NOTIMER
        GetSysTime(&now);
        if (now.tv_micro >= (start.tv_micro + (WAIT_TIMEOUT_MS*1000))) {
#if DEBUG >= 2
            KPrintF("wait_not_busy timeout\n");
#endif
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
bool ata_wait_ready(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval start, now;
    GetSysTime(&start);
#endif
#if DEBUG >= 2
    KPrintF("wait_ready_enter\n");
#endif
    while (1) {
        if (*(volatile BYTE *)unit->drive->status_command & ata_ready) return true;

#ifndef NOTIMER
        GetSysTime(&now);
        if (now.tv_micro >= (start.tv_micro + (WAIT_TIMEOUT_MS*1000))) {
#if DEBUG >= 2
            KPrintF("wait_ready timeout\n");
#endif
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
bool ata_wait_drq(struct IDEUnit *unit) {
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval start, now;
    GetSysTime(&start);
#endif

#if DEBUG >= 2
    KPrintF("wait_drq\n");
#endif

    while (1) {
        if (*(volatile BYTE *)unit->drive->status_command& ata_drq) return true;

#ifndef NOTIMER
        GetSysTime(&now);
        if (now.tv_micro >= (start.tv_micro + (WAIT_TIMEOUT_MS*1000))) {
#if DEBUG >= 2
            KPrintF("wait_drq timeout\n");
#endif
            return false;
        }
#endif

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
#ifndef NOTIMER
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    struct timeval start, now;
    GetSysTime(&start);
#endif
    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive

    *unit->drive->sectorCount = 0;
    *unit->drive->lbaLow  = 0;
    *unit->drive->lbaMid  = 0;
    *unit->drive->lbaHigh = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATA_CMD_IDENTIFY;
    volatile UBYTE *status = unit->drive->status_command;

#if DEBUG >= 2
    KPrintF("Drive Status: %04x%04x %04x%04x\n",status, *status);
#endif
    if (*status == 0) return false; // No drive there?

    while ((*status & (ata_drq | ata_error)) == 0) { 
        // Wait until ready to transfer or error condition
#ifndef NOTIMER
        GetSysTime(&now);
        if (now.tv_micro >= (start.tv_micro + (WAIT_TIMEOUT_MS*1000))) return false;
#endif
   }

    if (*status & ata_error) {
#if DEBUG >= 1
    KPrintF("IDENTIFY Status: Error\n");
#endif

        return false;
    }

    if (buffer) {
        UWORD read_data;
        for (int i=0; i<256; i++) {
            read_data = *unit->drive->data;
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
        if ((*(volatile UBYTE *)((void *)unit->drive->data + i) & 0xC0) != 0xC0) {
            dev_found = true;
#if DEBUG >= 1
            KPrintF("Something there?\n");
            KPrintF("Unit base: %04x%04x; Drive base %04x%04x\n",unit, unit->drive);
#endif             
            break;
        }
    }

    if (dev_found == false || !ata_wait_not_busy(unit))
        return false;

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) // Allocate buffer for IDENTIFY result
        return false;
#if DEBUG >= 1
    KPrintF("Send IDENTIFY\n");
#endif   
    if (ata_identify(unit,buf) == false) {
#if DEBUG >= 1
    KPrintF("IDENTIFY Timeout\n");
#endif 
        // Command failed with a timeout or error
        FreeMem(buf,512);
        return false;
    }
#if DEBUG >= 1
    KPrintF("Drive found!\n");
#endif 
    unit->cylinders       = *((UWORD *)buf + ata_identify_cylinders);
    unit->heads           = *((UWORD *)buf + ata_identify_heads);
    unit->sectorsPerTrack = *((UWORD *)buf + ata_identify_sectors);
    unit->blockSize       = *((UWORD *)buf + ata_identify_sectorsize);
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
BYTE ata_transfer(APTR *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit, enum xfer_dir direction) {
#if DEBUG >= 2
if (direction == READ) {
    KPrintF("ata_read");
} else {
    KPrintF("ata_write");
}
#endif
    ULONG subcount;
    UWORD offset = 0;
    *actual = 0;

    if (count == 0) return TDERR_TooFewSecs;

    BYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;
    *unit->drive->devHead        = (UBYTE)(drvSel | ((lba >> 24) & 0x0F));

    // None of this max_transfer 1FE00 nonsense!
    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            subcount = MAX_TRANSFER_SECTORS;
        } else {
            subcount = count;                // Get any remainders
        }

        if (!ata_wait_ready(unit))
            return HFERR_BadStatus;

        *unit->drive->sectorCount    = (subcount & 0xFF); // Count value of 0 indicates to transfer 256 sectors
        *unit->drive->lbaLow         = (UBYTE)(lba);
        *unit->drive->lbaMid         = (UBYTE)(lba >> 8);
        *unit->drive->lbaHigh        = (UBYTE)(lba >> 16);
        *unit->drive->error_features = 0;
        *unit->drive->status_command = (direction == READ) ? ATA_CMD_READ : ATA_CMD_WRITE;

        for (int block = 0; block < subcount; block++) {
            if (!ata_wait_drq(unit))
                return HFERR_BadStatus;

            if (*unit->drive->error_features && ata_error) {
                #if DEBUG >= 1
                KPrintF("ATA ERROR!!!");
                #endif
                return TDERR_NotSpecified;
            }
            if (direction == READ) {
                for (int i=0; i<(unit->blockSize / 4); i++) {
                    ((UWORD *)buffer)[offset] = *(UWORD *)unit->drive->data;
                    offset++;
                }
            } else {
                for (int i=0; i<(unit->blockSize / 4); i++) {
                    *(UWORD *)unit->drive->data = ((UWORD *)buffer)[offset];
                    offset++;
                }
            }

            *actual += unit->blockSize;
        }

        count -= subcount;
    }

    return 0;
}