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

    // Only update the devHead register if absolutely necessary to save time
    if (*unit->shadowDevHead != drvSel) {
        *unit->drive->devHead = *unit->shadowDevHead = drvSel;
        if (!ata_wait_ready(unit))
            return HFERR_SelTimeout;
    }

    *unit->drive->sectorCount    = 0;
    *unit->drive->lbaLow         = 0;
    *unit->drive->lbaMid         = 0;
    *unit->drive->lbaHigh        = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATAPI_CMD_IDENTIFY;

    if (!ata_wait_drq(unit)) {
        if (*unit->drive->status_command & (ata_flag_error | ata_flag_df)) {
            Warn("ATAPI: IDENTIFY Status: Error\n");
            Warn("ATAPI: last_error: %08lx\n",&unit->last_error[0]);
            // Save the error details
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

    if (cmd->scsi_Length > 0 && cmd->scsi_Data == NULL) return IOERR_BADADDRESS;

    volatile UBYTE *status = unit->drive->status_command;

    if (cmd->scsi_CmdLength > 12 || cmd->scsi_CmdLength < 6) return HFERR_BadStatus;

    cmd->scsi_Actual = 0;

    BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0);

    // Only update the devHead register if absolutely necessary to save time
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
            Trace("ATAPI: CMD Word: %ld\n",0);
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

/**
 * atapi_test_unit_ready
 * 
 * Send a TEST UNIT READY to the unit and update the media change count & presence
 * 
 * @param unit Pointer to an IDEUnit struct
 * @returns nonzero if there was an error
*/
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

    // If the sense key returned is not 0 (Unit ready) or 2 (Medium not present) try again
    // Mainly to run twice if sense code 6 is returned, so we can get the actual status of the medium
    for (int i = 0; i < 2; i++) {
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
        if (ret == 0 && (senseKey == 0) && unit->mediumPresent != true) {
            Trace("ATAPI: marking media as present\n");
            unit->mediumPresent = true;
            unit->change_count++;
            ret = atapi_get_capacity(unit);
            break;
        } else if (cmd->scsi_Status == 2 && senseKey == 2) {
            if (unit->mediumPresent == true) {
                Trace("ATAPI: marking media as absent\n");
                unit->mediumPresent = false;
                unit->change_count++;
                break;
            }
        } else if (senseKey == 6) {
            Trace("ATAPI: Unit attention, clearing with request_sense");
            atapi_request_sense(unit,NULL,0);
        }
    }

    if (cdb) FreeMem(cdb,sizeof(struct SCSI_CDB_10));
    if (cmd) FreeMem(cmd,sizeof(struct SCSICmd));

    return ret;
}

/**
 * atapi_request_sense
 * 
 * Request extended sense data from the ATAPI device
 * 
 * Currently just gets the senseKey to clear a UNIT ATTENTION status
 * @param unit Pointer to an IDEUnit struct
 * @return non-zero on error
*/
UBYTE atapi_request_sense(struct IDEUnit *unit, UBYTE *buffer, int length) {
    struct SCSI_CDB_10 *cdb = NULL;
    struct SCSICmd *cmd = NULL;
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
    cmd->scsi_SenseData   = buffer;
    cmd->scsi_SenseLength = length;
    cmd->scsi_Flags       = SCSIF_AUTOSENSE | SCSIF_READ;

    ret = atapi_packet(cmd,unit);
    Trace("ATAPI RS: Status %lx\n",ret);
    if (cdb) FreeMem(cdb,sizeof(struct SCSI_CDB_10));
    if (cmd) FreeMem(cmd,sizeof(struct SCSICmd));

    return ret;
}

/**
 * atapi_get_capacity
 * 
 * Send a READ CAPACITY (10) to the ATAPI device then update the unit geometry with the returned values
 * 
 * @param unit Pointer to an IDEUnit struct
 * @return non-zero on error
*/
UBYTE atapi_get_capacity(struct IDEUnit *unit) {
    struct SCSI_CDB_10 *cdb = NULL;
    struct SCSICmd *cmd = NULL;
    UBYTE ret;
    struct {
        ULONG logicalSectors;
        ULONG blockSize;
    } response;


    if ((cdb = AllocMem(sizeof(struct SCSI_CDB_10),MEMF_ANY|MEMF_CLEAR)) == NULL) {
        Trace("ATAPI: GC AllocMem failed.\n");
        return TDERR_NoMem;
    }

    if ((cmd = AllocMem(sizeof(struct SCSICmd),MEMF_ANY|MEMF_CLEAR)) == NULL) {
        Trace("ATAPI: GC AllocMem failed.\n");
        FreeMem(cdb,sizeof(struct SCSI_CDB_10));
        return TDERR_NoMem;
    }

    cdb->operation = SCSI_CMD_READ_CAPACITY_10;

    cmd->scsi_CmdLength = sizeof(struct SCSI_CDB_10);
    cmd->scsi_CmdActual = 0;
    cmd->scsi_Command   = (UBYTE *)cdb;
    cmd->scsi_Flags     = SCSIF_READ;
    cmd->scsi_Data      = (UWORD *)&response;
    cmd->scsi_Length    = 8;
    cmd->scsi_SenseData = NULL;

    unit->cylinders       = 0;
    unit->heads           = 0;
    unit->sectorsPerTrack = 0;
    unit->logicalSectors  = 0;
    unit->blockShift      = 0;

    if ((ret = atapi_packet(cmd,unit)) == 0) {
        unit->logicalSectors  = response.logicalSectors + 1;
        unit->blockSize       = response.blockSize;
        
        while ((unit->blockSize >> unit->blockShift) > 1) {
            unit->blockShift++;
        }
    }
    Trace("New geometry: %ld %ld\n",unit->logicalSectors, unit->blockSize);
    
    if (cdb) FreeMem(cdb,sizeof(struct SCSI_CDB_10));
    if (cmd) FreeMem(cmd,sizeof(struct SCSICmd));
    return ret;
}