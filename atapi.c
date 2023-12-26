// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
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
#include <exec/execbase.h>
#include <stdbool.h>

#include "debug.h"
#include "device.h"
#include "ata.h"
#include "atapi.h"
#include "scsi.h"
#include "string.h"
#include "wait.h"
#include "blockcopy.h"

/**
 * atapi_status_reg_delay
 * 
 * We need a short delay before actually checking the status register to let the drive update the status
 * To get the right delay we read the status register 4 times but just throw away the result
 * More info: https://wiki.osdev.org/ATA_PIO_Mode#400ns_delays
 * 
 * @param unit Pointer to an IDEUnit struct
*/
static void  __attribute__((always_inline)) atapi_status_reg_delay(struct IDEUnit *unit) {
    BYTE status;
    asm volatile (
        ".rep 4         \n\t"
        "move.b (%1),%0 \n\t"
        ".endr          \n\t"
        : "=&d" (status)
        : "a" (unit->drive->status_command)
        : "d0"
    );
}
/**
 * atapi_wait_drq
 * 
 * Poll DRQ in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool atapi_wait_drq(struct IDEUnit *unit, ULONG tries) {
    Trace("atapi_wait_drq enter\n");
    struct timerequest *tr = unit->itask->tr;
    atapi_status_reg_delay(unit);
    for (int i=0; i < tries; i++) {
        if ((*unit->drive->status_command & ata_flag_drq) != 0) return true;
        wait_us(tr,ATAPI_DRQ_WAIT_LOOP_US);
    }
    Trace("atapi_wait_drq timeout\n");
    return false;
}

/**
 * atapi_wait_drq_not_bsy
 * 
 * Poll the status register until DRQ set and BSY clear or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool atapi_wait_drq_not_bsy(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->itask->tr;
    atapi_status_reg_delay(unit);

    for (int i=0; i < tries; i++) {
        if ((*unit->drive->status_command & (ata_flag_busy | ata_flag_drq)) == ata_flag_drq) return true;
        wait_us(tr,ATAPI_DRQ_WAIT_LOOP_US);
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
    Trace("atapi_wait_not_bsy enter\n");
    struct timerequest *tr = unit->itask->tr;

    for (int i=0; i < tries; i++) {
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_busy) == 0) return true;
        wait_us(tr,ATAPI_BSY_WAIT_LOOP_US);
    }
    Trace("atapi_wait_not_bsy timeout\n");
    return false;
}

/**
 * atapi_wait_not_drqbsy
 * 
 * Poll DRQ & BSY in the status register until clear or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool atapi_wait_not_drqbsy(struct IDEUnit *unit, ULONG tries) {
    Trace("atapi_wait_not_drqbsy enter\n");
    struct timerequest *tr = unit->itask->tr;
    atapi_status_reg_delay(unit);

    for (int i=0; i < tries; i++) {
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_busy | ata_flag_drq)) == 0) return true;
        wait_us(tr,ATAPI_BSY_WAIT_LOOP_US);
    }
    Trace("atapi_wait_not_drqbsy timeout\n");
    return false;
}

/**
 * atapi_dev_reset
 * 
 * Resets the device by sending a DEVICE RESET command to it
 * @param unit Pointer to an IDEUnit struct
*/
void atapi_dev_reset(struct IDEUnit *unit) {
    Info("ATAPI: Resetting device\n");
    atapi_wait_not_bsy(unit,10);
    *unit->drive->status_command = ATA_CMD_DEVICE_RESET;
    atapi_wait_not_bsy(unit,ATAPI_BSY_WAIT_COUNT);

}

/**
 * atapi_check_signature
 * 
 * Resets the device then checks the signature in the LBA High and Mid registers to see if an ATAPI device is present
 * @param unit Pointer to an IDEUnit struct
*/
bool atapi_check_signature(struct IDEUnit *unit) {
    
    atapi_dev_reset(unit);
    wait_us(unit->itask->tr,10000);
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
            Info("ATAPI: IDENTIFY Status: Error\n");
            Info("ATAPI: last_error: %08lx\n",&unit->last_error[0]);
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
BYTE atapi_translate(APTR io_Data, ULONG lba, ULONG count, ULONG *io_Actual, struct IDEUnit *unit, enum xfer_dir direction) 
{
    Trace("atapi_translate enter\n");
    struct SCSICmd *cmd = MakeSCSICmd();
    if (cmd == NULL) return TDERR_NoMem;
    struct SCSI_CDB_10 *cdb = (struct SCSI_CDB_10 *)cmd->scsi_Command;
    UBYTE errorCode = 0;
    UBYTE senseKey  = 0;
    UBYTE asc       = 0;
    UBYTE asq       = 0;

    if (count == 0) {
        return IOERR_BADLENGTH;
    }
    Trace("%ld lba %ld count\n %ld bs\n",lba,count,unit->blockShift);
    BYTE err = 0;
    BYTE ret = 0;

    for (int tries = 4; tries > 0; tries--) {
        cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
        cmd->scsi_CmdActual   = 0;
        cmd->scsi_Flags       = (direction == READ) ? SCSIF_READ : SCSIF_WRITE;
        cmd->scsi_Data        = io_Data;
        cmd->scsi_Length      = count * unit->blockSize;
        cmd->scsi_Actual      = 0;
        cmd->scsi_SenseData   = NULL;
        cmd->scsi_SenseLength = 0;

        cdb->operation = (direction == READ) ? SCSI_CMD_READ_10 : SCSI_CMD_WRITE_10;
        cdb->control   = 0;
        cdb->flags     = 0;
        cdb->group     = 0;
        cdb->lba       = lba;
        cdb->length    = (UWORD)count;

        if ((err = atapi_packet(cmd,unit)) == 0) {
            goto done;
        } else {
            if (cmd->scsi_Status == 2) {
                // Unit reported CHECK STATUS
                // Request the sense data
                if ((ret = atapi_request_sense(unit,&errorCode,&senseKey,&asc,&asq)) != 0) {
                    // Got an error even trying to get the sense data :/
                    goto done;
                }
                switch (senseKey) {
                    case 0x01:                       // Recovered error
                        ret = 0;
                        goto done;

                    case 0x02:                       // Unit not ready
                        if (asc == 0x4) {            // Becoming ready
                            ret = TDERR_DiskChanged;
                            wait(unit->itask->tr,1);   // Wait
                            continue;                // and try again
                        } else {
                            ret = TDERR_DiskChanged; // No media
                            atapi_update_presence(unit,false);
                            goto done;
                        }

                    case 0x06:                       // Media changed or unit completed reset
                        ret = err;
                        continue;                    // Try the command again

                    case 0x07:                       // Disk is write protected
                        ret = TDERR_WriteProt;
                        goto done;
                    
                    default:                         // Anything else
                        ret = TDERR_NotSpecified;
                        continue;                    // Try again
                }

            } else {
                // Command time outs / bad phase etc end up here
                atapi_dev_reset(unit); // Reset the unit before trying again
                continue;
            }
        }
    }

done:
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
    LONG byte_count = 0;
    LONG remaining;
    UWORD data;
    BYTE ret = 0;
    UBYTE senseKey;
    UBYTE operation = ((struct SCSI_CDB_10 *)cmd->scsi_Command)->operation;
    volatile UBYTE *status = unit->drive->status_command;

    
    if (cmd->scsi_Length > 0 && cmd->scsi_Data == NULL) {
        ret = IOERR_BADADDRESS;
        goto end;
    }

    if (cmd->scsi_CmdLength > 12 || cmd->scsi_CmdLength < 6) {
        ret = IOERR_BADADDRESS;
        goto end;
    }

    cmd->scsi_Actual = 0;

    BYTE drvSelHead = ((unit->primary) ? 0xE0 : 0xF0);

    // Only update the devHead register if absolutely necessary to save time
    ata_select(unit,drvSelHead,true);

    if (!atapi_wait_not_drqbsy(unit,ATAPI_BSY_WAIT_COUNT)) {
        ret = IOERR_UNITBUSY;
        goto end;
    }

    if (cmd->scsi_Length > 65534) {
        byte_count = 65534;
    } else {
        byte_count = cmd->scsi_Length;
    }

    *unit->drive->lbaMid         = byte_count & 0xFF; 
    *unit->drive->lbaHigh        = byte_count >> 8 & 0xFF;
    *unit->drive->error_features = 0;
    *unit->drive->devHead        = drvSelHead;
    *unit->drive->status_command = ATAPI_CMD_PACKET;

    if (!atapi_wait_drq_not_bsy(unit,ATAPI_DRQ_WAIT_COUNT)) {
        Trace("ATAPI: Packet bsy timeout\n");
        ret = IOERR_UNITBUSY;
        goto end;
    }
        
    if ((*unit->drive->sectorCount & 0x03) != IR_COMMAND) {
        Trace("ATAPI: Failed command phase\n");
        ret = HFERR_Phase;
        goto end;
    }

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

    if (*status & ata_flag_error) goto ata_error;

    cmd->scsi_CmdActual = cmd->scsi_CmdLength;
    
    ULONG index = 0;

    while (1) {

        if (!atapi_wait_not_bsy(unit,ATAPI_BSY_WAIT_COUNT)) {
          ret = IOERR_UNITBUSY;
          goto end;
        }

        if (cmd->scsi_Length == 0) break;

        if (!(atapi_wait_drq(unit,10))) break;

        if ((*unit->drive->sectorCount & 0x01) != 0x00) break; // CoD doesn't indicate further data transfer

        byte_count = *unit->drive->lbaHigh << 8 | *unit->drive->lbaMid;
        byte_count += (byte_count & 0x01); // Ensure that the byte count is always an even number

        while (byte_count > 0) {
            remaining = cmd->scsi_Length - cmd->scsi_Actual;

            if ((byte_count >= 512 && remaining >= 512)) {
              // 512 or more bytes to transfer, use the fast ATA transfer routines
              if (cmd->scsi_Flags & SCSIF_READ) {
                  unit->read_fast((void *)unit->drive->data, cmd->scsi_Data + index);
              } else {
                  unit->write_fast(cmd->scsi_Data + index, (void *)unit->drive->data);
              }
              index += 256;
              cmd->scsi_Actual += 512;
              byte_count -= 512;
            } else if (remaining > 0) {
                // Less than 512 bytes means we can't use the fast ATA transfer routines, copy word-by-word
                if (cmd->scsi_Flags & SCSIF_READ) {
                    cmd->scsi_Data[index] = *unit->drive->data;
                } else {
                    *unit->drive->data = cmd->scsi_Data[index];
                }
                index++;
                cmd->scsi_Actual+=2;
                byte_count -= 2;
            } else {
                // If we got here the drive wanted to transfer more data than the buffer could take
                ret = HFERR_BadStatus;
                goto end;
            }
        }
    }

    if (unit->SysBase->SoftVer > 36) {
        CacheClearE(cmd->scsi_Data,cmd->scsi_Length,CACRF_ClearI);
    }

    atapi_wait_not_bsy(unit,ATAPI_BSY_WAIT_COUNT);

    if ((*unit->drive->sectorCount & 0x03) != IR_STATUS) { // Drive reports command completion?
      // No
      ret = HFERR_Phase;
    }

end:
    if (*status & ata_flag_error) {
 ata_error:
        unit->last_error[0] = *unit->drive->error_features;
        unit->last_error[1] = *unit->drive->status_command;
        unit->last_error[2] = *unit->drive->sectorCount;
        senseKey = *unit->drive->error_features >> 4;
        Warn("ATAPI ERROR!\n");
        Warn("Sense Key: %02lx\n",senseKey);
        Warn("Error: %02lx\n",*status);
        Warn("Interrupt reason: %02lx\n",*unit->drive->sectorCount);
        cmd->scsi_Status = 2;
        if (ret == 0) ret = HFERR_BadStatus;
    }
    if (cmd->scsi_Actual < cmd->scsi_Length) {
        if (ret == 0 && (operation == SCSI_CMD_READ_10 || operation == SCSI_CMD_WRITE_10)) {
            ret = IOERR_BADLENGTH;
        }
        Warn("Command %lx Transferred %ld of %ld requested for %ld blocks\n",(ULONG)(*(UBYTE *)cmd->scsi_Command),cmd->scsi_Actual, cmd->scsi_Length, ((struct SCSI_CDB_10 *)cmd->scsi_Command)->length);
    }

    Trace("exit atapi_packet\n");
    return ret;
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

    UBYTE senseError, senseKey, asc, asq = 0;
    UBYTE ret = 0;

    for (int tries = 4; tries > 0; tries--) {
        cdb->operation        = SCSI_CMD_TEST_UNIT_READY;
        cmd->scsi_Command     = (UBYTE *)cdb;
        cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
        cmd->scsi_Length      = 0;
        cmd->scsi_Data        = NULL;
        cmd->scsi_Flags       = SCSIF_READ;

        ret = atapi_packet(cmd,unit);

        if (ret == 0) {
          break;
        } else {
            if ((ret = atapi_request_sense(unit,&senseError,&senseKey,&asc,&asq)) == 0) {
                Trace("SenseKey: %lx ASC: %lx ASQ: %lx\n",senseKey,asc,asq);
                switch (senseKey) {
                    case 0x02: // Not ready
                        if (asc == 4) { // Becoming ready
                            // The medium is becoming ready, wait a few seconds before checking again
                            ret = TDERR_DiskChanged;
                            if (tries > 0) wait(unit->itask->tr,3);
                        } else { // Anything else - No medium/bad medium etc
                            ret = TDERR_DiskChanged;
                            goto done;
                        }
                        break;
                    case 0x06: // Unit attention
                        if (asc == 0x28) { // Medium became ready
                            ret = 0;
                        }
                        break;
                    case 0x03: // Medium error
                        ret = TDERR_DiskChanged;
                        break;
                    default:
                        // Anything else, could be a timeout/bad phase etc
                        // Reset the unit and try again
                        //atapi_dev_reset(unit);
                        ret = TDERR_NotSpecified;
                        break;
                }
            } else {
                atapi_dev_reset(unit);
                break;
            }
        }
    }

done:
    atapi_update_presence(unit,(ret == 0)); // Update the media presence
    DeleteSCSICmd(cmd);

    return ret;
}

/**
 * atapi_request_sense
 * 
 * Request extended sense data from the ATAPI device
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param senseKey Pointer for the senseKey result
 * @param asc Pointer for the asc result
 * @param asq Pointer for the asq result
 * @return non-zero on error
*/
BYTE atapi_request_sense(struct IDEUnit *unit, UBYTE *errorCode, UBYTE *senseKey, UBYTE *asc, UBYTE *asq) {
    struct SCSICmd *cmd = MakeSCSICmd();
    if (cmd == NULL) return TDERR_NoMem;
    UBYTE *cdb = (UBYTE *)cmd->scsi_Command;

    UBYTE *buf;
    
    if ((buf = AllocMem(18,MEMF_CLEAR|MEMF_ANY)) == NULL) {
        DeleteSCSICmd(cmd);
        return TDERR_NoMem;
    }

    UBYTE ret;

    cdb[0]                = SCSI_CMD_REQUEST_SENSE;
    cdb[4]                = 18;
    cmd->scsi_Command     = (UBYTE *)cdb;
    cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
    cmd->scsi_Length      = 18;
    cmd->scsi_Data        = (UWORD *)buf;
    cmd->scsi_Flags       = SCSIF_READ;

    ret = atapi_packet(cmd,unit);

    Trace("ATAPI RS: Status %lx\n",ret);

    *errorCode = buf[0];
    *senseKey  = buf[2] & 0x0F;
    *asc       = buf[12];
    *asq       = buf[13];

    DeleteSCSICmd(cmd);
    if (buf) FreeMem(buf,18);
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
    } capacity;

    cdb->operation = SCSI_CMD_READ_CAPACITY_10;

    cmd->scsi_CmdLength = sizeof(struct SCSI_CDB_10);
    cmd->scsi_CmdActual = 0;
    cmd->scsi_Command   = (UBYTE *)cdb;
    cmd->scsi_Flags     = SCSIF_READ;
    cmd->scsi_Data      = (UWORD *)&capacity;
    cmd->scsi_Length    = 8;
    cmd->scsi_SenseData = NULL;

    unit->cylinders       = 0;
    unit->heads           = 0;
    unit->sectorsPerTrack = 0;
    unit->logicalSectors  = 0;
    unit->blockShift      = 0;

    if ((ret = atapi_packet(cmd,unit)) == 0) {
        unit->logicalSectors  = capacity.logicalSectors + 1;
        unit->blockSize       = capacity.blockSize;
        
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

    cmd->scsi_CmdLength = sizeof(struct SCSI_CDB_10);
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

    if (cmd->scsi_Data == NULL || cmd->scsi_Length == 0) return IOERR_BADADDRESS;

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
 * atapi_scsi_read_write_6
 * 
 * ATAPI devices do not support READ (6) or WRITE (6)
 * Translate these calls to READ (10) / WRITE (10);
 * 
 * @param cmd Pointer to a SCSICmd struct
 * @param unit Pointer to an IDEUnit struct
 * @returns non-zero on error
*/
BYTE atapi_scsi_read_write_6 (struct SCSICmd *cmd, struct IDEUnit *unit) {
    BYTE ret;
    struct SCSI_CDB_10 *cdb = AllocMem(sizeof(struct SCSI_CDB_10),MEMF_ANY|MEMF_CLEAR);
    if (!cdb) return TDERR_NoMem;

    struct SCSI_CDB_6 *oldcdb  = (struct SCSI_CDB_6 *)cmd->scsi_Command;

    cdb->operation = oldcdb->operation | 0x20;
    cdb->length    = oldcdb->length;
    cdb->lba       = oldcdb->lba_high << 16 |
                     oldcdb->lba_mid  << 8  | 
                     oldcdb->lba_low;

    if (cdb->length == 0) cdb->length = 256; // for SCSI READ/WRITE 6 a transfer length of 0 specifies that 256 blocks will be transferred

    cmd->scsi_Command = (BYTE *)cdb;

    if (!((ULONG)cmd->scsi_Data & 0x01)) { // Buffer is word-aligned?
        ret = atapi_packet(cmd,unit);
    } else {
        ret = atapi_packet_unaligned(cmd,unit);
    }

    FreeMem(cdb,sizeof(struct SCSI_CDB_10));

    cmd->scsi_Command = (BYTE *)oldcdb;

    return ret;
}

/**
 * atapi_packet_unaligned
 * 
 * In the unlikely event that someone has allocated an unaligned data buffer, align the data first by making a copy
 * 
 * @param cmd Pointer to a SCSICmd struct
 * @param unit Pointer to an IDEUnit struct
 * @returns non-zero on exit 
*/
BYTE atapi_packet_unaligned(struct SCSICmd *cmd, struct IDEUnit *unit) {
    BYTE error = 0;

    // Some bozo with an unaligned data buffer... (lookin' at you HDToolbox!)
    // Allocate an aligned buffer and CopyMem to / from this one
    UWORD *orig_buffer = cmd->scsi_Data;
    if ((cmd->scsi_Data = AllocMem(cmd->scsi_Length,MEMF_CLEAR|MEMF_ANY)) == NULL) {
        cmd->scsi_Data = orig_buffer;
        error = TDERR_NoMem;
        return error;
    }

    if (cmd->scsi_Flags & SCSIF_READ) {
        error = atapi_packet(cmd,unit);
        CopyMem(cmd->scsi_Data,orig_buffer,cmd->scsi_Length);
    } else {
        CopyMem(orig_buffer,cmd->scsi_Data,cmd->scsi_Length);
        error = atapi_packet(cmd,unit);
    }
    FreeMem(cmd->scsi_Data,cmd->scsi_Length);
    cmd->scsi_Data = orig_buffer;

    return error;
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
    UBYTE ret  = 0;
    UBYTE *buf = NULL;

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) return TDERR_NoMem;

    UWORD actual = 0;

    if ((ret = atapi_mode_sense(unit,0x3F,(UWORD *)buf,512,&actual)) == 0) {
        if (buf[3] & 1<<7)
            ret = TDERR_WriteProt;
    }

    if (buf) FreeMem(buf,512);

    return ret;

}

/**
 * atapi_update_presence
 * 
 * If the medium has changed state update the unit info, geometry etc
 * @param unit Pointer to an IDEUnit struct
 * @param present Medium present
 * @returns bool true if changed
*/
bool atapi_update_presence(struct IDEUnit *unit, bool present) {
    bool ret = false;
    if (present && unit->mediumPresent == false) {
        unit->changeCount++;
        unit->mediumPresent = true;
        atapi_get_capacity(unit);
        ret = true;
    } else if (!present && unit->mediumPresent == true) {
        unit->changeCount++;
        unit->mediumPresent  = false;
        unit->logicalSectors = 0;
        unit->blockShift     = 0;
        unit->blockSize      = 0;
        ret = true;
    }
    return ret;
}
