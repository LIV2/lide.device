#include <proto/exec.h>
#include <proto/expansion.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/errors.h>
#include <libraries/dos.h>
#include <stdbool.h>
#include "device.h"
#include "ata.h"
#include <stdio.h>

bool __attribute((always_inline)) ata_wait_busy(struct IDEUnit *unit) {
    for (int i=0; i<1000; i++) {
        if ((*unit->drive->status_command & ata_busy) == 0) return true;
    }
    // Timed out
    return false;
}

bool ata_identify(struct IDEUnit *unit, UWORD *buffer)
{
    *unit->drive->sectorCount = 0;
    *unit->drive->lbaLow  = 0;
    *unit->drive->lbaMid  = 0;
    *unit->drive->lbaHigh = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATA_CMD_IDENTIFY;
    
    volatile UBYTE *status = unit->drive->status_command;

    if (*status == 0) return false;

    while ((*status & (ata_drq | ata_error)) == 0) {
        ;;
    }
    
    if (*status & ata_error) {
        return false;
    }

    if (buffer) {
        UWORD read_data;
        for (int i=0; i<256; i++) {
            read_data = *unit->drive->data;
            buffer[i] = (read_data >> 8) | (read_data << 8);
        }       
    }

    return true;
}

bool ata_init_unit(struct IDEUnit *unit) {
    unit->cylinders       = 0;
    unit->heads           = 0;
    unit->sectorsPerTrack = 0;

    ULONG offset;
    bool dev_found = false;

    offset = (unit->channel == 0) ? CHANNEL_0 : CHANNEL_1;
    unit->drive = (void *)unit->cd->cd_BoardAddr + offset;

    *unit->drive->devHead = (unit->primary) ? 0xF0 : 0xE0;
    for (int i=0; i<(8*NEXT_REG); i+=NEXT_REG) {
        // Check if the bus is floating (D7/6 pulled-up with resistors)
        if ((*(volatile UBYTE *)((void *)unit->drive->data + i) & 0xC0) != 0xC0) {
            dev_found = true;
            break;
        }
    }

    if (dev_found == false) return false;
    if (!ata_wait_busy(unit)) return false;

    return ata_identify(unit,NULL);
}
