// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <exec/errors.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <string.h>

#include "ata.h"
#include "atapi.h"
#include "debug.h"
#include "device.h"
#include "idetask.h"
#include "newstyle.h"
#include "scsi.h"
#include "td64.h"
#include "wait.h"

/**
 * scsi_inquiry_ata
 * 
 * Handle SCSI-Direct INQUIRY commands for ATA devices
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
static BYTE scsi_inquiry_ata(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    struct SCSI_Inquiry *data = (struct SCSI_Inquiry *)scsi_command->scsi_Data;
    BYTE error;

    data->peripheral_type   = unit->device_type;
    data->removable_media   = 0;
    data->version           = 2;
    data->response_format   = 2;
    data->additional_length = (sizeof(struct SCSI_Inquiry) - 4);

    UWORD *identity = AllocMem(512,MEMF_CLEAR|MEMF_ANY);
    if (!identity) {
        error = TDERR_NoMem;
        scsi_sense(scsi_command,0,0,error);
        return error;
    }

   if (!(ata_identify(unit,identity))) {
        error = HFERR_SelTimeout;
        scsi_sense(scsi_command,0,0,error);
        return error;
    }

    CopyMem(&identity[ata_identify_model],&data->vendor,24);
    CopyMem(&identity[ata_identify_fw_rev],&data->revision,4);
    CopyMem(&identity[ata_identify_serial],&data->serial,8);
    FreeMem(identity,512);
    scsi_command->scsi_Actual = scsi_command->scsi_Length;
    return 0;
}

/**
 * scsi_read_capaity_ata
 * 
 * Handle SCSI-Direct READ CAPACITY commands for ATA devices
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
static BYTE scsi_read_capaity_ata(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    struct SCSI_CAPACITY_10 *data = (struct SCSI_CAPACITY_10 *)scsi_command->scsi_Data;
    BYTE error;

    if (data == NULL) {
        error = IOERR_BADADDRESS;
        scsi_sense(scsi_command,0,0,error);
        return error;
    }

    struct SCSI_READ_CAPACITY_10 *cdb = (struct SCSI_READ_CAPACITY_10 *)scsi_command->scsi_Command;

    data->block_size = unit->blockSize;

    if (cdb->flags & 0x01) {
        // Partial Medium Indicator - Return end of cylinder
        // Implement this so HDToolbox stops moaning about track size
        ULONG spc = unit->cylinders * unit->heads;
        data->lba = (((cdb->lba / spc) + 1) * spc) - 1;
    } else {
        data->lba = (unit->logicalSectors) - 1;
    }


    scsi_command->scsi_Actual = 8;

    return 0;
}

/**
 * scsi_mode_sense_ata
 * 
 * Handle SCSI-Direct MODE SENSE (6) commands for ATA devices
 * 
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
static BYTE scsi_mode_sense_ata(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    BYTE error;
    UBYTE *data    = (APTR)scsi_command->scsi_Data;
    UBYTE *command = (APTR)scsi_command->scsi_Command;

    if (data == NULL) {
        return IOERR_BADADDRESS;
    }

    UBYTE page    = command[2] & 0x3F;
    UBYTE subpage = command[3];

    if (subpage != 0) {
        error = HFERR_BadStatus;
        scsi_sense(scsi_command,0,0,error);
        return error;
    }

    UBYTE *data_length = data;   // Mode data length
    data[1] = unit->device_type; // Mode parameter: Media type
    data[2] = 0;                 // DPOFUA
    data[3] = 0;                 // Block descriptor length

    *data_length = 3;

    UBYTE idx = 4;
    if (page == 0x3F || page == 0x03) {
        data[idx++] = 0x03; // Page Code: Format Parameters
        data[idx++] = 0x16; // Page length
        for (int i=0; i <8; i++) {
            data[idx++] = 0;
        }
        data[idx++] = (unit->sectorsPerTrack >> 8);
        data[idx++] = unit->sectorsPerTrack;
        data[idx++] = (unit->blockSize >> 8);
        data[idx++] = unit->blockSize;
        for (int i=0; i<12; i++) {
            data[idx++] = 0;
        }
    }

    if (page == 0x3F || page == 0x04) {
        data[idx++] = 0x04; // Page code: Rigid Drive Geometry Parameters
        data[idx++] = 0x16; // Page length
        data[idx++] = 0;
        data[idx++] = (unit->cylinders >> 8);
        data[idx++] = unit->cylinders;
        data[idx++] = unit->heads;
        for (int i=0; i<19; i++) {
            data[idx++] = 0;
        }
    }

    *data_length += (idx + 1);
    scsi_command->scsi_Actual = *data_length;
    return 0;

}

/**
 * handle_scsi_command
 *
 * Handle SCSI Direct commands
 * @param ioreq IO Request
*/
static void handle_scsi_command(struct IOStdReq *ioreq) {
    struct SCSICmd *scsi_command = ioreq->io_Data;
    struct IDEUnit *unit = (struct IDEUnit *)ioreq->io_Unit;

    UBYTE *data    = (APTR)scsi_command->scsi_Data;
    UBYTE *command = (APTR)scsi_command->scsi_Command;

    ULONG lba;
    ULONG count;
    UBYTE error = 0;
    scsi_command->scsi_SenseActual = 0;


    enum xfer_dir direction = WRITE;

    Trace("SCSI: Command %ld\n",*scsi_command->scsi_Command);

    if (unit->atapi == false)
    {
        // Non-ATAPI drives - Translate SCSI CMD to ATA
        switch (scsi_command->scsi_Command[0]) {
            case SCSI_CMD_TEST_UNIT_READY:
                scsi_command->scsi_Actual = 0;
                error = 0;
                break;

            case SCSI_CMD_INQUIRY:
                error = scsi_inquiry_ata(unit,scsi_command);
                break;

            case SCSI_CMD_MODE_SENSE_6:
                error = scsi_mode_sense_ata(unit,scsi_command);
                break;

            case SCSI_CMD_READ_CAPACITY_10:
                error = scsi_read_capaity_ata(unit,scsi_command);
                break;

            case SCSI_CMD_READ_6:
            case SCSI_CMD_WRITE_6:
                lba   = (((((struct SCSI_CDB_6 *)command)->lba_high & 0x1F) << 16) |
                        ((struct SCSI_CDB_6 *)command)->lba_mid << 8 |
                        ((struct SCSI_CDB_6 *)command)->lba_low);

                count = ((struct SCSI_CDB_6 *)command)->length;
                goto do_scsi_transfer;

            case SCSI_CMD_READ_10:
            case SCSI_CMD_WRITE_10:
                lba    = ((struct SCSI_CDB_10 *)command)->lba;
                count  = ((struct SCSI_CDB_10 *)command)->length;

    do_scsi_transfer:
                if (data == NULL || (lba + count) >= unit->logicalSectors) {
                    error = IOERR_BADADDRESS;
                    scsi_sense(scsi_command,lba,count,error);
                    break;
                }

                direction = (scsi_command->scsi_Flags & SCSIF_READ) ? READ : WRITE;

                if (direction == READ) {
                    error = ata_read(data,lba,count,&scsi_command->scsi_Actual,unit);
                } else {
                    error = ata_write(data,lba,count,&scsi_command->scsi_Actual,unit);
                }
                if (error != 0 ) {
                    if (error == TDERR_NotSpecified) {
                        scsi_sense(scsi_command,lba,
                        (unit->last_error[0] << 8 | unit->last_error[4])
                        ,error);
                    } else {
                        scsi_sense(scsi_command,lba,count,error);
                    }
                }
                break;

            default:
                error = IOERR_NOCMD;
                scsi_sense(scsi_command,0,0,error);
                break;
        }
    } else {
        // SCSI command handling for ATAPI Drives
        
        switch (scsi_command->scsi_Command[0]) {

            case SCSI_CMD_READ_6:
            case SCSI_CMD_WRITE_6:
                // ATAPI devices don't support READ/WRITE(6) so translate it
                error = atapi_scsi_read_write_6(scsi_command,unit);
                break;

            case SCSI_CMD_MODE_SENSE_6:
                error = atapi_scsi_mode_sense_6(scsi_command,unit);
                break;

            case SCSI_CMD_READ_CAPACITY_10:
                // CDROMs don't support parameters for READ_CAPACITY_10 so clear them all
                for (int i=1; i < scsi_command->scsi_CmdLength; i++) {
                    scsi_command->scsi_Command[i] = 0;
                }

            default:
                if (!((ULONG)scsi_command->scsi_Data & 0x01)) { // Buffer is word-aligned?
                    error = atapi_packet(scsi_command,unit);
                } else {
                    error = atapi_packet_unaligned(scsi_command,unit);
                }

                if (error != 0) {
                    if (scsi_command->scsi_Flags & (SCSIF_AUTOSENSE)) {

                        Trace("Auto sense requested\n");

                        struct SCSICmd *cmd = MakeSCSICmd();

                        cmd->scsi_Command[0] = SCSI_CMD_REQUEST_SENSE;
                        cmd->scsi_Command[4] = 18;
                        cmd->scsi_Data       = (UWORD *)scsi_command->scsi_SenseData;
                        cmd->scsi_Length     = scsi_command->scsi_SenseLength;
                        cmd->scsi_Flags      = SCSIF_READ;
                        cmd->scsi_CmdLength  = 1;

                        atapi_packet(cmd,unit);
                        scsi_command->scsi_SenseActual = cmd->scsi_Actual;
                        DeleteSCSICmd(cmd);
                    }
                }
                break;
        }
    }

    // SCSI Command complete, handle any errors

    Trace("SCSI: return: %02lx\n",error);
    
    ioreq->io_Error = error;
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;

    if (error != 0) {
        Warn("SCSI: Error: %ld\n",error);
        Warn("SCSI: Command: %ld\n",scsi_command->scsi_Command);
        scsi_command->scsi_Status = SCSI_CHECK_CONDITION;
    } else {
        scsi_command->scsi_Status = 0;
    }
}

/**
 * diskchange_task
 *
 * This task periodically polls all removable devices for media changes and updates 
*/
void __attribute__((noreturn)) diskchange_task () {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct Task volatile *task = FindTask(NULL);
    struct MsgPort *TimerMP, *iomp = NULL;
    struct timerequest *TimerReq = NULL;
    struct IOStdReq *ioreq = NULL, *intreq = NULL;
    struct IDEUnit *unit = NULL;
    bool previous;
    bool present;

    while (task->tc_UserData == NULL); // Wait for Task Data to be populated
    struct DeviceBase *dev = (struct DeviceBase *)task->tc_UserData;

    if ((TimerMP = CreatePort(NULL,0)) == NULL || (TimerReq = (struct timerequest *)CreateExtIO(TimerMP, sizeof(struct timerequest))) == NULL) goto die;
    if ((iomp = CreatePort(NULL,0)) == NULL || (ioreq = CreateStdIO(iomp)) == NULL) goto die;
    if (OpenDevice("timer.device",UNIT_VBLANK,(struct IORequest *)TimerReq,0) != 0) goto die;

    ioreq->io_Command = TD_CHANGESTATE; // Run TD_CHANGESTATE to update medium presence, this should be replaced with a TUR call
    ioreq->io_Data    = NULL;
    ioreq->io_Length  = 1;
    ioreq->io_Actual  = 0;

    while (1) {
        ioreq->io_Data   = NULL;
        ioreq->io_Length = 0;

        for (int i=-0; i < MAX_UNITS; i++) {
            unit = &dev->units[i];
            if (unit->present && unit->atapi) {
                Trace("Testing unit %ld\n",i);
                previous = unit->mediumPresent;  // Get old state
                ioreq->io_Unit = (struct Unit *)unit;

                PutMsg(dev->IDETaskMP,(struct Message *)ioreq); // Send request directly to the ide task
                WaitPort(iomp);
                GetMsg(iomp);
                
                present = (ioreq->io_Actual == 0); // Get current state

                if (present != previous) {
                    // Forbid while accessing the list;
                    Forbid();
                    for (intreq = (struct IOStdReq *)unit->changeints.mlh_Head; intreq->io_Message.mn_Node.ln_Succ != NULL; intreq = (struct IOStdReq *)intreq->io_Message.mn_Node.ln_Succ) {
                        if (intreq->io_Data) {
                            Cause(intreq->io_Data);
                        }
                    }
                    Permit();
                }
            }
        }
        Trace("Wait...\n");
        wait(TimerReq,CHANGEINT_INTERVAL);
    }

die:
    Info("Change task dying...\n");
    if (ioreq) DeleteStdIO(ioreq);
    if (iomp) DeletePort(iomp);
    if (TimerReq && TimerReq->tr_node.io_Device) CloseDevice(TimerReq->tr_node.io_Device);
    if (TimerReq) DeleteExtIO((struct IORequest *)TimerReq);
    if (TimerMP) DeletePort(TimerMP);
    
    RemTask(NULL);
    Wait(0);
    while (1);
}


/**
 * ide_task
 *
 * This is a task to complete IO Requests for all units
 * Requests are sent here from begin_io via the dev->IDETaskMP Message port
*/
void __attribute__((noreturn)) ide_task () {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct Task volatile *task = FindTask(NULL);
    struct MsgPort *mp;
    struct IOStdReq *ioreq;
    struct IDEUnit *unit;
    UWORD blockShift;
    ULONG lba;
    ULONG count;
    enum xfer_dir direction = WRITE;

    Info("IDE Task: waiting for init\n");
    while (task->tc_UserData == NULL); // Wait for Task Data to be populated
    struct DeviceBase *dev = (struct DeviceBase *)task->tc_UserData;

    Trace("IDE Task: CreatePort()\n");
    // Create the MessagePort used to send us requests
    if ((mp = CreatePort(NULL,0)) == NULL) {
        dev->IDETask = NULL; // Failed to create MP, let the device know
        RemTask(NULL);
        Wait(0);
    }

    dev->IDETimerMP->mp_SigTask = FindTask(NULL);
    dev->IDETimerMP->mp_SigBit  = AllocSignal(-1);
    dev->IDETimerMP->mp_Flags   = PA_SIGNAL;

    dev->IDETaskMP = mp;
    dev->IDETaskActive = true;

    while (1) {
        // Main loop, handle IO Requests as they come in.
        Trace("IDE Task: WaitPort()\n");
        Wait(1 << mp->mp_SigBit); // Wait for an IORequest to show up

        while ((ioreq = (struct IOStdReq *)GetMsg(mp))) {
            unit = (struct IDEUnit *)ioreq->io_Unit;
            direction = WRITE;

            switch (ioreq->io_Command) {
                case TD_EJECT:
                    if (!unit->atapi) {
                        ioreq->io_Error = IOERR_NOCMD;
                        break;
                    }
                    bool insert = (ioreq->io_Length == 0) ? true : false;
                    ioreq->io_Error = atapi_start_stop_unit(unit,insert,1);
                    break;

                case TD_CHANGESTATE:
                    ioreq->io_Error  = 0;
                    ioreq->io_Actual = 0;
                    if (unit->atapi) {
                        ioreq->io_Actual = (atapi_test_unit_ready(unit) != 0);
                        break;
                    }
                    ioreq->io_Actual = (((struct IDEUnit *)ioreq->io_Unit)->mediumPresent) ? 0 : 1;
                    break;

                case TD_PROTSTATUS:
                    ioreq->io_Error = 0;
                    if (unit->atapi) {
                        if (unit->device_type == 0x05 || (ioreq->io_Error = atapi_check_wp(unit)) == TDERR_WriteProt) {
                            ioreq->io_Error = 0;
                            ioreq->io_Actual = 1;
                            break;
                        }
                    }
                    ioreq->io_Actual = 0; // Not protected
                    break;

                case TD_ADDCHANGEINT:
                    Info("Addchangeint\n");
                    ioreq->io_Error = 0;
                    Forbid();
                    AddHead((struct List *)&unit->changeints,(struct Node *)&ioreq->io_Message.mn_Node);
                    Permit();
                    // Don't reply to this request
                    continue;

                case TD_REMCHANGEINT:
                    ioreq->io_Error = 0;
                    struct MinNode *changeint;
                    Forbid();
                    for (changeint = unit->changeints.mlh_Head; changeint->mln_Succ != NULL; changeint = changeint->mln_Succ) {
                        if (ioreq == (struct IOStdReq *)changeint) {
                            Remove(&ioreq->io_Message.mn_Node);
                        }
                    }
                    Permit();
                    break;

                case CMD_READ:
                case TD_READ64:
                case NSCMD_TD_READ64:
                    direction = READ;

                case CMD_WRITE:
                case TD_WRITE64:
                case TD_FORMAT:
                case TD_FORMAT64:
                case NSCMD_TD_WRITE64:
                case NSCMD_TD_FORMAT64:
                    if (unit->atapi == true && unit->mediumPresent == false) {
                        Trace("Access attempt without media\n");
                        ioreq->io_Error = TDERR_DiskChanged;
                        break;
                    }
                    
                    blockShift = ((struct IDEUnit *)ioreq->io_Unit)->blockShift;
                    lba = (((long long)ioreq->io_Actual << 32 | ioreq->io_Offset) >> blockShift);
                    count = (ioreq->io_Length >> blockShift);

                    if ((lba + count) > (unit->logicalSectors)) {
                        Trace("Read past end of device\n");
                        ioreq->io_Error = TDERR_SeekError;
                        break;
                    }

                    if (unit->atapi == true) {
                        ioreq->io_Error = atapi_translate(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit, direction);
                    } else {
                        if (direction == READ) {
                            ioreq->io_Error = ata_read(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit);
                        } else {
                            ioreq->io_Error = ata_write(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit);
                        }
                    }
                    break;

                /* SCSI Direct */
                case HD_SCSICMD:
                    handle_scsi_command(ioreq);
                    break;

                /* CMD_DIE: Shut down this task and clean up */
                case CMD_DIE:
                    Info("Task: CMD_DIE: Shutting down IDE Task\n");
                    DeletePort(mp);
                    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
                    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
                    if (dev->IDETimerMP) DeletePort(dev->IDETimerMP);
                    dev->IDETaskMP     = NULL;
                    dev->IDETask       = NULL;
                    dev->IDETaskActive = false;
                    ReplyMsg(&ioreq->io_Message);
                    RemTask(NULL);
                    Wait(0);
                    break;
                default:
                    // Unknown commands.
                    ioreq->io_Error  = IOERR_NOCMD;
                    ioreq->io_Actual = 0;
                    break;
            }

            ReplyMsg(&ioreq->io_Message);
        }
    }

}

