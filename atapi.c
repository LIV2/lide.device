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
 * atapi_wait_drq
 * 
 * Poll DRQ in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool atapi_wait_drq(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->TimeReq;

    for (int i=0; i < tries; i++) {
        if ((*unit->drive->status_command & ata_flag_drq) != 0) return true;
        //if ((*unit->drive->status_command & (ata_flag_df | ata_flag_error)) != 0) return false;
        tr->tr_time.tv_micro = ATAPI_DRQ_WAIT_LOOP_US;
        tr->tr_time.tv_secs  = 0;
        tr->tr_node.io_Command = TR_ADDREQUEST;
        DoIO((struct IORequest *)tr);

    }
    return false;
}

/**
 * atapi_wait_not_bsy
 * 
 * Poll BSY in the status register until clear or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool atapi_wait_not_bsy(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->TimeReq;

    for (int i=0; i < tries; i++) {
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_busy) == 0) return true;
        tr->tr_time.tv_micro = ATAPI_BSY_WAIT_LOOP_US;
        tr->tr_time.tv_secs  = 0;
        tr->tr_node.io_Command = TR_ADDREQUEST;
        DoIO((struct IORequest *)tr);
    }
    return false;
}

/**
 * atapi_wait_rdy
 * 
 * Poll RDY in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool atapi_wait_rdy(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->TimeReq;

    for (int i=0; i < tries; i++) {
        if ((*unit->drive->status_command & (ata_flag_ready | ata_flag_busy)) == ata_flag_ready) return true;
        tr->tr_time.tv_micro = ATAPI_RDY_WAIT_LOOP_US;
        tr->tr_time.tv_secs  = 0;
        tr->tr_node.io_Command = TR_ADDREQUEST;
        DoIO((struct IORequest *)tr);
    }
    return false;
}

/**
 * atapi_dev_reset
 * 
 * Resets the device by sending a DEVICE RESET command to it
 * @param unit Pointer to an IDEUnit struct
*/
void atapi_dev_reset(struct IDEUnit *unit) {
    struct timerequest *tr = unit->TimeReq;
    
    atapi_wait_not_bsy(unit,10);
    *unit->drive->status_command = ATA_CMD_DEVICE_RESET;

    tr->tr_time.tv_micro   = 100;
    tr->tr_time.tv_sec     = 0;
    tr->tr_node.io_Command = TR_ADDREQUEST;
    DoIO((struct IORequest *)tr);
}

/**
 * atapi_check_signature
 * 
 * Resets the device then checks the signature in the LBA High and Mid registers to see if an ATAPI device is present
 * @param unit Pointer to an IDEUnit struct
*/
bool atapi_check_signature(struct IDEUnit *unit) {
    
    atapi_dev_reset(unit);

    for (int i=0; i<20; i++) {
        if ((*unit->drive->lbaHigh == 0xEB) && (*unit->drive->lbaMid == 0x14)) return true;
    }

    return false;
}

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
    ata_select(unit,drvSel,false);

    //if (!atapi_wait_rdy(unit,ATAPI_RDY_WAIT_COUNT))
    //        return HFERR_SelTimeout;

    *unit->drive->sectorCount    = 0;
    *unit->drive->lbaLow         = 0;
    *unit->drive->lbaMid         = 0;
    *unit->drive->lbaHigh        = 0;
    *unit->drive->error_features = 0;
    *unit->drive->status_command = ATAPI_CMD_IDENTIFY;

    if (!atapi_wait_drq(unit,ATAPI_DRQ_WAIT_COUNT)) {
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
    Trace("atapi_translate enter\n");
    struct SCSICmd *cmd = MakeSCSICmd();
    if (cmd == NULL) return TDERR_NoMem;
    struct SCSI_CDB_10 *cdb = (struct SCSI_CDB_10 *)cmd->scsi_Command;


    if (count == 0) {
        return IOERR_BADLENGTH;
    }

    BYTE ret = 0;
    cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
    cmd->scsi_CmdActual   = 0;
    cmd->scsi_Flags       = (direction == READ) ? SCSIF_READ : SCSIF_WRITE;
    cmd->scsi_Data        = io_Data;
    cmd->scsi_Length      = count * unit->blockSize;
    cmd->scsi_SenseData   = NULL;
    cmd->scsi_SenseLength = 0;

    cdb->operation = (direction == READ) ? SCSI_CMD_READ_10 : SCSI_CMD_WRITE_10;
    cdb->control   = 0;
    cdb->flags     = 0;
    cdb->group     = 0;
    cdb->lba       = lba;
    cdb->length    = (UWORD)count;

    ret = atapi_packet(cmd,unit);
    Trace("atapi_packet returns %ld\n",ret);
    *io_Actual = cmd->scsi_Actual;
    
    DeleteSCSICmd(cmd);

    return ret;
}

/**
 * atapi_packet
 * 
 * Send a SCSICmd to an ATAPI device
 * 
 * @param cmd Pointer to a SCSICmd struct
 * @param unit Pointer to the IDEUnit
 * @returns error, sense key returned in SenseData
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
    ata_select(unit,drvSelHead,true);

    if (!atapi_wait_rdy(unit,ATAPI_RDY_WAIT_COUNT))
            return HFERR_SelTimeout;
 
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
    if (!atapi_wait_not_bsy(unit,10000)) {
        Trace("ATAPI: Packet bsy timeout\n");
        return HFERR_SelTimeout;
    }

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

    while (1) {
        if (!atapi_wait_not_bsy(unit,ATAPI_BSY_WAIT_COUNT)) goto xferdone;

        if (!atapi_wait_drq(unit,ATAPI_DRQ_WAIT_COUNT)) {
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
BYTE atapi_test_unit_ready(struct IDEUnit *unit) {
    struct SCSICmd *cmd = MakeSCSICmd();
    if (cmd == NULL) return TDERR_NoMem;
    struct SCSI_CDB_10 *cdb = (struct SCSI_CDB_10 *)cmd->scsi_Command;

    UBYTE senseKey = 0;
    UBYTE ret = 0;

    // If the sense key returned is not 0 (Unit ready) or 2 (Medium not present) try again
    // Mainly to run a couple of times if sense code 6 is returned, so we can get the actual status of the medium
    for (int i = 0; i < 3; i++) {
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

        if (ret == 0) {
            if (unit->mediumPresent == false) {
                    Trace("ATAPI TUR: setting medium as present\n");
                    unit->mediumPresent = true;
                    unit->change_count++;
                    ret = atapi_get_capacity(unit);
                }
                goto done;
        } else {
            switch (senseKey) {
                case 2: // Not ready, no medium
                    if (unit->mediumPresent != false) {
                        Trace("ATAPI TUR: Setting medium as not present\n");
                        // Only increment change_count if the status changed
                        unit->change_count++;
                    }

                    unit->mediumPresent = false;
                    unit->logicalSectors = 0;
                    unit->blockShift = 0;
                    unit->blockSize = 0;
                    ret = TDERR_DiskChanged;
                    break;
                case 6: // Unit attention
                    Trace("ATAPI: Unit attention, clearing with request_sense");
                case 3: // Medium error
                    if ((ret = atapi_request_sense(unit,NULL,0)) == 0) // Get the sense data
                        ret = TDERR_DiskChanged;
                    break;
            }
        }
    }

done:
    DeleteSCSICmd(cmd);

    return ret;
}

/**
 * atapi_request_sense
 * 
 * Request extended sense data from the ATAPI device
 * 
 * @param unit Pointer to an IDEUnit struct
 * @return non-zero on error
*/
BYTE atapi_request_sense(struct IDEUnit *unit, UWORD *buffer, int length) {
    struct SCSICmd *cmd = MakeSCSICmd();
    if (cmd == NULL) return TDERR_NoMem;
    struct SCSI_CDB_10 *cdb = (struct SCSI_CDB_10 *)cmd->scsi_Command;

    UBYTE ret;

    cdb->operation        = SCSI_CMD_REQUEST_SENSE;
    cmd->scsi_Command     = (UBYTE *)cdb;
    cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
    cmd->scsi_Length      = length;
    cmd->scsi_Data        = buffer;
    cmd->scsi_Flags       = SCSIF_READ;

    ret = atapi_packet(cmd,unit);

    Trace("ATAPI RS: Status %lx\n",ret);

    DeleteSCSICmd(cmd);

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
BYTE atapi_get_capacity(struct IDEUnit *unit) {
    struct SCSICmd *cmd = MakeSCSICmd();
    if (cmd == NULL) return TDERR_NoMem;
    struct SCSI_CDB_10 *cdb = (struct SCSI_CDB_10 *)cmd->scsi_Command;

    BYTE ret;

    struct {
        ULONG logicalSectors;
        ULONG blockSize;
    } response;

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
    
    DeleteSCSICmd(cmd);
    return ret;
}


/** 
 * atapi_mode_sense
 * 
 * Send a MODE SENSE (10) request to the ATAPI device
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param page_code Page code to request
 * @param buffer Pointer to a buffer for the mode sense data response
 * @param length Size of the buffer
 * @param actual Pointer to the actual byte count returned
 * @return Non-zero on error
*/
BYTE atapi_mode_sense(struct IDEUnit *unit, BYTE page_code, UWORD *buffer, UWORD length, UWORD *actual) {
    struct SCSICmd *cmd = MakeSCSICmd();
    if (cmd == NULL) return TDERR_NoMem;

    UBYTE *cdb = cmd->scsi_Command;
    BYTE ret;


    cdb[0] = SCSI_CMD_MODE_SENSE_10;
    cdb[2] = page_code & 0x3F;
    cdb[7] = length >> 8;
    cdb[8] = length & 0xFF;

    cmd->scsi_CmdLength = 12;
    cmd->scsi_CmdActual = 0;
    cmd->scsi_Command   = (UBYTE *)cdb;
    cmd->scsi_Flags     = SCSIF_READ;
    cmd->scsi_Data      = buffer;
    cmd->scsi_Length    = length;
    cmd->scsi_SenseData = NULL;

    ret = atapi_packet(cmd,unit);

    if (actual) *actual = cmd->scsi_Actual;

    DeleteSCSICmd(cmd);
    return ret;
}


/**
 * atapi_scsi_mode_sense_6
 * 
 * ATAPI devices do not support MODE SENSE (6) so translate to a MODE SENSE (10)
 * 
 * @param cmd Pointer to a SCSICmd struct containing a MODE SENSE (6) request
 * @param unit Pointer to an IDEUnit struct
 * @returns non-zero on error, mode-sense data in cmd->scsi_Data
*/
BYTE atapi_scsi_mode_sense_6(struct SCSICmd *cmd, struct IDEUnit *unit) {
    BYTE ret;

    UBYTE page_code;

    UBYTE *buf  = NULL;
    UBYTE *dest = (UBYTE *)cmd->scsi_Data; 

    ULONG length = cmd->scsi_Length + 4;
    UWORD actual = 0;

    if (cmd->scsi_Data == NULL || cmd->scsi_Length == 0);

    // ATAPI doesn't seem to support subpages at all;
    if (cmd->scsi_Command[3] != 0) return IOERR_ABORTED;

    if ((buf = AllocMem(length,MEMF_ANY|MEMF_CLEAR)) == NULL) {
        return TDERR_NoMem;
    }

    page_code = cmd->scsi_Command[2];
    
    ret = atapi_mode_sense(unit,page_code,(UWORD *)buf,length,&actual);

    if (buf[0] != 0) { // Mode sense length MSB
        Warn("ATAPI: MODESENSE 6 to 10 - Returned mode sense data too large\n");
        ret = IOERR_BADLENGTH;
    }

    if (ret == 0) {
        dest[0] = (buf[1] - 4); // Length
        dest[1] = buf[1];       // Medium type
        dest[2] = buf[3];       // WP/DPOFUA Flags
        dest[3] = 0;            // Block descriptor length

        for (int i = 4; i < actual; i++) {
            // Copy the Mode Sense data
            dest[i] = buf[i+4];
        }
        cmd->scsi_Actual    = actual - 4;
        cmd->scsi_CmdActual = cmd->scsi_CmdLength;
    }

    if (buf) FreeMem(buf,length);

    return ret;
}

/**
 * atapi_start_stop_unit
 * 
 * send START STOP command to ATAPI drive e.g to eject the disc
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param start Start bit of START STOP
 * @param loej loej bit of START STOP
 * @returns non-zero on error
*/
BYTE atapi_start_stop_unit(struct IDEUnit *unit, bool start, bool loej) {
    struct SCSICmd *cmd = NULL;
    UBYTE operation = 0;
    UBYTE ret;

    if (loej)  operation |= (1<<1);
    if (start) operation |= (1<<0);

    if ((cmd = MakeSCSICmd()) == NULL) return TDERR_NoMem;
    
    cmd->scsi_Command[0] = SCSI_CMD_START_STOP_UNIT;
    cmd->scsi_Command[1] = (1<<0); // Immediate bit set
    cmd->scsi_Command[4] = operation;

    ret = atapi_packet(cmd,unit);

    DeleteSCSICmd(cmd);

    return ret;
}

/**
 * atapi_check_wp
 * 
 * Check write-protect status of the disk
 * 
 * @param unit Pointer to an IDEUnit struct
 * @returns non-zero on error
*/
BYTE atapi_check_wp(struct IDEUnit *unit) {
    struct SCSICmd *cmd = MakeSCSICmd();
    UBYTE *cdb = cmd->scsi_Command;
    UBYTE *buf;
    BYTE ret;
    cdb[0] = SCSI_CMD_MODE_SENSE_10;
    cdb[2] = 0x3F; // All pages
    cdb[7] = 7;    // Allocation length (we only really want the header)

    if ((buf = AllocMem(7,MEMF_ANY|MEMF_CLEAR)) == NULL) return TDERR_NoMem;

    cmd->scsi_Length = 7;
    cmd->scsi_Data = (UWORD *)buf;

    if ((ret = atapi_packet(cmd,unit)) == 0) {
        if (buf[3] & 1<<7)
            ret = TDERR_WriteProt;
    }

    if (buf) FreeMem(buf,7);

    return ret;

}