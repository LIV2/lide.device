// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <exec/errors.h>
#include <proto/exec.h>
#include <string.h>
#include <proto/expansion.h>

#include "ata.h"
#include "atapi.h"
#include "debug.h"
#include "device.h"
#include "idetask.h"
#include "newstyle.h"
#include "scsi.h"
#include "td64.h"
#include "wait.h"
#include "lide_alib.h"

/**
 * scsi_inquiry_ata
 *
 * Handle SCSI-Direct INQUIRY commands for ATA devices
 *
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
static BYTE scsi_inquiry_ata(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    struct ExecBase *SysBase = unit->SysBase;

    struct SCSI_Inquiry *data = (struct SCSI_Inquiry *)scsi_command->scsi_Data;
    BYTE error;

    data->peripheral_type   = unit->deviceType;
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

    if (subpage != 0 || (page != 0x3F && page != 0x03 && page != 0x04)) {
        error = HFERR_BadStatus;
        scsi_sense(scsi_command,0,0,error);
        return error;
    }

    UBYTE *data_length = data;  // Mode data length
    data[1] = unit->deviceType; // Mode parameter: Media type
    data[2] = 0;                // DPOFUA
    data[3] = 0;                // Block descriptor length

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
        data[idx++] = (unit->cylinders >> 16);
        data[idx++] = (unit->cylinders >> 8);
        data[idx++] = unit->cylinders;
        data[idx++] = unit->heads;
        for (int i=0; i<19; i++) {
            data[idx++] = 0;
        }
    }

    *data_length = idx - 1;
    scsi_command->scsi_Actual = idx;
    return 0;

}

/**
 * handle_scsi_command
 *
 * Handle SCSI Direct commands
 * @param ioreq IO Request
*/
static BYTE handle_scsi_command(struct IOStdReq *ioreq) {
    struct SCSICmd *scsi_command = ioreq->io_Data;
    struct IDEUnit *unit = (struct IDEUnit *)ioreq->io_Unit;

    UBYTE *data    = (APTR)scsi_command->scsi_Data;
    UBYTE *command = (APTR)scsi_command->scsi_Command;

    ULONG lba;
    ULONG count;
    BYTE error = 0;
    scsi_command->scsi_SenseActual = 0;


    enum xfer_dir direction = WRITE;

    Trace("SCSI: Command %lx\n",*scsi_command->scsi_Command);

    if (unit->atapi == false)
    {
        // Non-ATAPI drives - Translate SCSI CMD to ATA
        switch (scsi_command->scsi_Command[0]) {
            case SCSI_CMD_ATA_PASSTHROUGH:
                error = scsi_ata_passthrough(unit,scsi_command);
                break;

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
                    error = ata_read(data,lba,count,unit);
                } else {
                    error = ata_write(data,lba,count,unit);
                }
                if (error == 0) {
                    scsi_command->scsi_Actual = scsi_command->scsi_Length;
                } else {
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

            case SCSI_CMD_INQUIRY:
                // Fudge the SCSI version number for CD/DVDs
                // Some software expects version 2 but ATAPI returns version 0
                error = atapi_packet(scsi_command,unit);

                if (error == 0 && unit->deviceType == DG_CDROM) {
                    if ((scsi_command->scsi_Command[1] & 1) == 0) {
                        ((struct SCSI_Inquiry *)scsi_command->scsi_Data)->version = 2;
                    }
                }
                break;

            case SCSI_CMD_READ_6:
            case SCSI_CMD_WRITE_6:
                // ATAPI devices don't support READ/WRITE(6) so translate it
                error = atapi_scsi_read_write_6(scsi_command,unit);
                break;

            case SCSI_CMD_MODE_SENSE_6:
                error = atapi_scsi_mode_sense_6(scsi_command,unit);
                break;

            case SCSI_CMD_MODE_SELECT_6:
                error = atapi_scsi_mode_select_6(scsi_command,unit);
                break;

            case SCSI_CMD_PLAY_TRACK_INDEX:
                error = atapi_translate_play_audio_index(scsi_command,unit);
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

                break;
        }

        if (error != 0) {
            if (scsi_command->scsi_Flags & (SCSIF_AUTOSENSE)) {

                Trace("Auto sense requested\n");
                // Request sense with retries
                for (int retry = 0; retry < 3; retry++) {
                    if ((atapi_autosense(scsi_command,unit)) == 0)
                        break;

                    wait_us(unit->itask->tr,250000); // Wait 250ms before retrying
                }
            }
        }
    }

    // SCSI Command complete, handle any errors

    Trace("SCSI: return: %02lx\n",error);

    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;

    if (error != 0) {
        Warn("SCSI: Error: %ld\n",error);
        Warn("SCSI: Command: %ld\n",scsi_command->scsi_Command);
        scsi_command->scsi_Status = SCSI_CHECK_CONDITION;
    } else {
        scsi_command->scsi_Status = 0;
    }
    return error;
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
    bool present;

    while (task->tc_UserData == NULL); // Wait for Task Data to be populated
    struct DeviceBase *dev = (struct DeviceBase *)task->tc_UserData;

    if ((TimerMP = L_CreatePort(NULL,0)) == NULL || (TimerReq = (struct timerequest *)L_CreateExtIO(TimerMP, sizeof(struct timerequest))) == NULL) goto die;
    if ((iomp = L_CreatePort(NULL,0)) == NULL || (ioreq = L_CreateStdIO(iomp)) == NULL) goto die;
    if (OpenDevice("timer.device",UNIT_VBLANK,(struct IORequest *)TimerReq,0) != 0) goto die;

    ioreq->io_Command = TD_CHANGESTATE; // Run TD_CHANGESTATE to update medium presence, this should be replaced with a TUR call
    ioreq->io_Data    = NULL;
    ioreq->io_Length  = 1;
    ioreq->io_Actual  = 0;

    while (1) {
        ioreq->io_Data   = NULL;
        ioreq->io_Length = 0;

        if (SysBase->LibNode.lib_Version >= 36) {
            ObtainSemaphoreShared(&dev->ulSem);
        } else {
            ObtainSemaphore(&dev->ulSem);
        }

        for (unit = (struct IDEUnit *)dev->units.mlh_Head;
             unit->mn_Node.mln_Succ != NULL;
             unit = (struct IDEUnit *)unit->mn_Node.mln_Succ)
        {
            if (unit->present && unit->atapi && !unit->deferTUR) {
                Trace("Testing unit %ld\n",unit->unitNum);
                ioreq->io_Unit = (struct Unit *)unit;

                PutMsg(unit->itask->iomp,(struct Message *)ioreq); // Send request directly to the ide task
                WaitPort(iomp);
                GetMsg(iomp);

                present = (ioreq->io_Actual == 0); // Get current state

                if (present != unit->mediumPresentPrev) {

                    Forbid();
                    if (unit->changeInt != NULL)
                        Cause((struct Interrupt *)unit->changeInt); // TD_REMOVE

                    for (intreq = (struct IOStdReq *)unit->changeInts.mlh_Head;
                         intreq->io_Message.mn_Node.ln_Succ != NULL;
                         intreq = (struct IOStdReq *)intreq->io_Message.mn_Node.ln_Succ) {

                        if (intreq->io_Data) {
                            Cause(intreq->io_Data);
                        }
                    }
                    Permit();
                }

                unit->mediumPresentPrev = present;
            }
            unit->deferTUR = false;
        }

        ReleaseSemaphore(&dev->ulSem);

        Trace("Wait...\n");
        wait(TimerReq,CHANGEINT_INTERVAL);
    }

die:
    Info("Change task dying...\n");
    if (ioreq) L_DeleteStdIO(ioreq);
    if (iomp) L_DeletePort(iomp);
    if (TimerReq && TimerReq->tr_node.io_Device) CloseDevice((struct IORequest *)TimerReq);
    if (TimerReq) L_DeleteExtIO((struct IORequest *)TimerReq);
    if (TimerMP) L_DeletePort(TimerMP);

    RemTask(NULL);
    Wait(0);
    while (1);
}

/**
 * init_units
 *
 * Initialize the IDE Drives and add them to the dev->units list
 *
 * @param itask Pointer to an IDETask struct
 * @returns number of drives found
*/
static BYTE init_units(struct IDETask *itask) {
    struct ExecBase *SysBase = itask->dev->SysBase;
    UBYTE num_units = 0;
    struct DeviceBase *dev = itask->dev;

    for (BYTE i=0; i < 2; i++) {
        struct IDEUnit *unit = AllocMem(sizeof(struct IDEUnit),MEMF_ANY|MEMF_CLEAR);
        if (unit != NULL) {
            // Setup each unit structure
            unit->itask             = itask;
            unit->unitNum           = ((itask->boardNum * 4) + (itask->channel << 1) + i);
            unit->SysBase           = SysBase;
            unit->cd                = itask->cd;
            unit->primary           = ((i%2) == 1) ? false : true;
            unit->channel           = itask->channel;
            unit->openCount         = 0;
            unit->changeCount       = 1;
            unit->deviceType        = DG_DIRECT_ACCESS;
            unit->mediumPresent     = false;
            unit->mediumPresentPrev = false;
            unit->present           = false;
            unit->atapi             = false;
            unit->xferMultiple      = false;
            unit->multipleCount     = 0;
            unit->shadowDevHead     = &itask->shadowDevHead;
            *unit->shadowDevHead    = 0;
            unit->deferTUR          = false;

            // Initialize the change int list
            unit->changeInts.mlh_Tail     = NULL;
            unit->changeInts.mlh_Head     = (struct MinNode *)&unit->changeInts.mlh_Tail;
            unit->changeInts.mlh_TailPred = (struct MinNode *)&unit->changeInts;

            Warn("testing unit %ld\n",unit->unitNum);

            if (ata_init_unit(unit)) {
                if (unit->atapi) dev->hasRemovables = true;
                num_units++;
                itask->dev->numUnits++;
                dev->highestUnit = unit->unitNum;
                ObtainSemaphore(&dev->ulSem);
                AddTail((struct List *)&dev->units,(struct Node *)unit);
                ReleaseSemaphore(&dev->ulSem);

            } else {
                // Clear this to skip the pre-select BSY wait later
                *unit->shadowDevHead = 0;
                FreeMem(unit,sizeof(struct IDEUnit));
            }
        }
    }

    return num_units;
}

/**
 * cleanup
 *
 * Clean up after the task, freeing resources etc back to the system
*/
static void cleanup(struct IDETask *itask) {
    struct ExecBase *SysBase = itask->dev->SysBase;
    if (itask->iomp)
        L_DeletePort(itask->iomp);

    if (itask->tr) {
        if (itask->tr->tr_node.io_Device)
            CloseDevice((struct IORequest *)itask->tr);

        L_DeleteExtIO((struct IORequest *)itask->tr);
    }
    if (itask->timermp) L_DeletePort(itask->timermp);

    struct IDEUnit *unit;


    for (unit = (struct IDEUnit *)itask->dev->units.mlh_Head;
         unit->mn_Node.mln_Succ != NULL;
         unit =  (struct IDEUnit *)unit->mn_Node.mln_Succ) {
            if (unit->itask == itask) {
                ObtainSemaphore(&itask->dev->ulSem);
                Remove((struct Node *)unit);
                ReleaseSemaphore(&itask->dev->ulSem);
                FreeMem(unit,sizeof(struct IDEUnit));
            }
         }


    itask->active = false;
    itask->task   = NULL;
    Signal(itask->parent, SIGF_SINGLE);
}

/**
 * ide_task
 *
 * This is a task to complete IO Requests for all units
 * Requests are sent here from begin_io via the dev->IDETaskMP Message port
*/
void __attribute__((noreturn)) ide_task () {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct Task *task = FindTask(NULL);
    struct IDETask *itask = (struct IDETask *)task->tc_UserData;
    struct IOStdReq *ioreq;
    struct IOExtTD *iotd;
    struct IDEUnit *unit;
    UWORD blockShift;
    ULONG lba;
    ULONG count;
    BYTE  error = 0;
    enum xfer_dir direction = WRITE;

    itask->task = task;

    Trace("IDE Task: CreatePort()\n");
    // Create the MessagePort used to send us requests
    if ((itask->iomp = L_CreatePort(NULL,0)) == NULL) {
        cleanup(itask);
        RemTask(NULL);
        Wait(0);
    }
    Trace("IDE Task: CreatePort() ok\n");

    if ((itask->timermp = L_CreatePort(NULL,0)) != NULL && (itask->tr = (struct timerequest *)L_CreateExtIO(itask->timermp, sizeof(struct timerequest))) != NULL) {
        if (OpenDevice("timer.device",UNIT_MICROHZ,(struct IORequest *)itask->tr,0)) {
            cleanup(itask);
            RemTask(NULL);
            Wait(0);
        }
    } else {
        Info("Failed to create Timer MP or Request.\n");
        cleanup(itask);
        RemTask(NULL);
        Wait(0);
    }

    if (init_units(itask) == 0) {
        cleanup(itask);
        RemTask(NULL);
        Wait(0);
    }

    itask->active = true;
    Signal(itask->parent,SIGF_SINGLE);

    while (1) {
        // Main loop, handle IO Requests as they come in.
        Trace("IDE Task: WaitPort()\n");
        Wait(1 << itask->iomp->mp_SigBit); // Wait for an IORequest to show up

        while ((ioreq = (struct IOStdReq *)GetMsg(itask->iomp)) != NULL) {
            unit = (struct IDEUnit *)ioreq->io_Unit;
            iotd = (struct IOExtTD *)ioreq;

            direction = WRITE;

            switch (ioreq->io_Command) {
                case TD_EJECT:
                    if (!unit->atapi) {
                        error  = IOERR_NOCMD;
                        break;
                    }
                    ioreq->io_Actual = (unit->mediumPresent) ? 0 : 1;   // io_Actual reflects the previous state

                    bool insert = (ioreq->io_Length == 0) ? true : false;

                    if (insert == false) atapi_update_presence(unit,false); // Immediately update medium presence on Eject

                    error = atapi_start_stop_unit(unit,insert,1);
                    break;

                case TD_CHANGESTATE:
                    error   = 0;
                    ioreq->io_Actual = 0;
                    if (unit->atapi) {
                        ioreq->io_Actual = (atapi_test_unit_ready(unit) != 0);
                        break;
                    }
                    ioreq->io_Actual = (((struct IDEUnit *)ioreq->io_Unit)->mediumPresent) ? 0 : 1;
                    break;

                case TD_PROTSTATUS:
                    error  = 0;
                    if (unit->atapi) {
                        if ((error  = atapi_check_wp(unit)) == TDERR_WriteProt) {
                            error  = 0;
                            ioreq->io_Actual = 1;
                            break;
                        }
                    }
                    ioreq->io_Actual = 0; // Not protected
                    break;

                case ETD_READ:
                case NSCMD_ETD_READ64:
                    direction = READ;
                    goto validate_etd;

                case ETD_WRITE:
                case ETD_FORMAT:
                case NSCMD_ETD_WRITE64:
                case NSCMD_ETD_FORMAT64:
                    direction = WRITE;
validate_etd:
                    if (iotd->iotd_Count < unit->changeCount) {
                        error  = TDERR_DiskChanged;
                        break;
                    } else {
                        goto transfer;
                    }
                case CMD_READ:
                case TD_READ64:
                case NSCMD_TD_READ64:
                    direction = READ;
                    goto transfer;

                case CMD_WRITE:
                case TD_WRITE64:
                case TD_FORMAT:
                case TD_FORMAT64:
                case NSCMD_TD_WRITE64:
                case NSCMD_TD_FORMAT64:
                    direction = WRITE;
transfer:
                    if (unit->atapi == true && unit->mediumPresent == false) {
                        Trace("Access attempt without media\n");
                        error  = TDERR_DiskChanged;
                        break;
                    }

                    blockShift = ((struct IDEUnit *)ioreq->io_Unit)->blockShift;
                    lba = (((long long)ioreq->io_Actual << 32 | ioreq->io_Offset) >> blockShift);
                    count = (ioreq->io_Length >> blockShift);

                    if (count == 0) {
                        error = IOERR_BADLENGTH;
                        break;
                    }

                    if ((lba + count) > (unit->logicalSectors)) {
                        Trace("Read past end of device\n");
                        error  = IOERR_BADADDRESS;
                        break;
                    }

                    if (unit->atapi == true) {
                        error  = atapi_translate(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit, direction);
                    } else {
                        if (direction == READ) {
                            error  = ata_read(ioreq->io_Data, lba, count, unit);
                        } else {
                            error  = ata_write(ioreq->io_Data, lba, count, unit);
                        }
                        ioreq->io_Actual = ioreq->io_Length;
                    }
                    break;

                /* SCSI Direct */
                case HD_SCSICMD:
                    error = handle_scsi_command(ioreq);
                    break;

                case CMD_XFER:
                    if (ioreq->io_Length < 3) {
                        ata_set_xfer(unit,ioreq->io_Length);
                        error = 0;
                    } else {
                        error = IOERR_ABORTED;
                    }
                    break;

                case CMD_PIO:
                    if (ioreq->io_Length <= 4) {
                        error = ata_set_pio(unit,ioreq->io_Length);
                    } else {
                        error = IOERR_BADADDRESS;
                    }
                    break;

                /* CMD_DIE: Shut down this task and clean up */
                case CMD_DIE:
                    Info("Task: CMD_DIE: Shutting down IDE Task\n");
                    cleanup(itask);
                    ReplyMsg(&ioreq->io_Message);
                    RemTask(NULL);
                    Wait(0);
                    break;
                default:
                    // Unknown commands.
                    error = IOERR_NOCMD;
                    ioreq->io_Actual = 0;
                    break;
            }

#if DEBUG & DBG_CMD
            traceCommand(ioreq);
#endif
            ioreq->io_Error = error;
            ReplyMsg(&ioreq->io_Message);
        }
    }

}

/**
 * direct_changestate
 *
 * Send a TD_CHANGESTATE request directly to the IDE Task
 * This will have the side-effect of updating the presence status and geometry of the unit if the status changed
 *
 * @param unit Pointer to an IDEUnit struct
 * @param dev Pointer to DeviceBase
 * @returns -1 on error, 0 if disk present, >0 if no disk
*/
BYTE direct_changestate (struct IDEUnit *unit, struct DeviceBase *dev) {
    struct ExecBase *SysBase = unit->SysBase;

    BYTE ret = -1;
    struct MsgPort *iomp = NULL;
    struct IOStdReq *ioreq = NULL;

    if ((iomp = L_CreatePort(NULL,0)) == NULL || (ioreq = L_CreateStdIO(iomp)) == NULL) goto die;

    ioreq->io_Command = TD_CHANGESTATE;
    ioreq->io_Data    = NULL;
    ioreq->io_Length  = 1;
    ioreq->io_Actual  = 0;
    ioreq->io_Unit    = (struct Unit *)unit;
    PutMsg(unit->itask->iomp,(struct Message *)ioreq); // Send request directly to the ide task
    WaitPort(iomp);
    GetMsg(iomp);

    if (ioreq->io_Error == 0) {
        ret = ioreq->io_Actual; // TD_CHANGESTATE returns 0 in io_Actual if disk preset, nonzero if no disk
    } else {
        ret = -1;
    }
die:
    if (ioreq) L_DeleteStdIO(ioreq);
    if (iomp)  L_DeletePort(iomp);

    return ret;
}
