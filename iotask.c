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
#include "iotask.h"
#include "newstyle.h"
#include "scsi.h"
#include "td64.h"
#include "sleep.h"
#include "lide_alib.h"

/**
 * create_timer
 *
 * Create and initialize the timer for the IO task
 *
 * @param SysBase Pointer to the ExecBase
 * @param mp Pointer to the MsgPort
 * @param tr Pointer to the timerequest
 * @param unit Timer unit to use
 * @returns True if the timer was created successfully, false otherwise
 */
static bool create_timer(struct ExecBase *SysBase, struct MsgPort **mp, struct timerequest **tr, ULONG unit) {

    Trace("Enter create_timer()\n");

    if ((*mp = L_CreatePort(NULL,0))) {
        if ((*tr = (struct timerequest *)L_CreateExtIO(*mp,sizeof(struct timerequest)))) {
            if (!OpenDevice("timer.device",unit,(struct IORequest *)*tr,0)) {
                return true;
            } else {
                Trace("Failed to open timer.device\n");
            }
            L_DeleteExtIO((struct IORequest *)*tr);
        } else {
            Trace("CreateExtIO failed\n");
        }
        L_DeletePort(*mp);
    } else {
        Trace("CreatePort failed\n");
    }

    Trace("create_timer failed\n");

    return false;
}

/**
 * run_timer
 *
 * Run the timer to signal the task periodically
 *
 * @param SysBase Pointer to the ExecBase
 * @param tr Pointer to the timerequest
 * @param secs Seconds to wait
 * @param micros Microseconds to wait
 */
static void run_timer(struct ExecBase *SysBase, struct timerequest *tr, ULONG secs, ULONG micros) {
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_sec   = secs;
    tr->tr_time.tv_micro = micros;
    SendIO((struct IORequest *)tr);
}

/**
 * delete_timer
 *
 * Delete a timer and messageport created by create_timer
 *
 * @param SysBase Pointer to the ExecBase
 * @param mp Pointer to the message port
 * @param tr Pointer to the timerequest
 */
static void delete_timer(struct ExecBase *SysBase, struct MsgPort *mp, struct timerequest *tr, bool armed) {
    if (tr) {
        if (tr->tr_node.io_Device) {
            if (armed) {
                AbortIO((struct IORequest *)tr);
                WaitIO((struct IORequest *)tr);
            }
            CloseDevice((struct IORequest *)tr);
        }
        L_DeleteExtIO((struct IORequest *)tr);
    }
    if (mp) {
        L_DeletePort(mp);
    }
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

    if (!scsi_command) return IOERR_BADADDRESS;

    UBYTE *data    = (APTR)scsi_command->scsi_Data;
    UBYTE *command = (APTR)scsi_command->scsi_Command;

    uint64_t lba;
    ULONG count;
    BYTE error = 0;
    scsi_command->scsi_SenseActual = 0;


    enum xfer_dir direction = WRITE;

    Trace("SCSI: Command %lx\n",*scsi_command->scsi_Command);

    if (unit->flags.atapi) {
        error = atapi_handle_scsi_command(unit,scsi_command);
    } else {
        // Translate SCSI CMD to ATA
        switch (scsi_command->scsi_Command[0]) {
            case SCSI_CMD_ATA_PASSTHROUGH:
                error = scsi_ata_passthrough(unit,scsi_command);
                break;

            case SCSI_CMD_TEST_UNIT_READY:
                scsi_command->scsi_Actual = 0;
                error = 0;
                break;

            case SCSI_CMD_INQUIRY:
                error = scsi_inquiry_emu(unit,scsi_command);
                break;

            case SCSI_CMD_MODE_SENSE_6:
                error = scsi_mode_sense_emu(unit,scsi_command);
                break;

            case SCSI_CMD_READ_CAPACITY_10:
                error = scsi_read_capacity_10_emu(unit,scsi_command);
                break;

            case SCSI_CMD_READ_CAPACITY_16:
                error = scsi_read_capacity_16_emu(unit,scsi_command);
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
                lba   = ((struct SCSI_CDB_10 *)command)->lba;
                count = ((struct SCSI_CDB_10 *)command)->length;
                goto do_scsi_transfer;

            case SCSI_CMD_READ_16:
            case SCSI_CMD_WRITE_16:
                lba   = ((struct SCSI_CDB_16 *)command)->lba;
                count = ((struct SCSI_CDB_16 *)command)->length;

    do_scsi_transfer:
                if (data == NULL || (lba + count) > unit->logicalSectors) {
                    error = IOERR_BADADDRESS;
                    fake_scsi_sense(scsi_command,lba,count,error);
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
                        fake_scsi_sense(scsi_command,lba,
                        (unit->last_error[0] << 8 | unit->last_error[4])
                        ,error);
                    } else {
                        fake_scsi_sense(scsi_command,lba,count,error);
                    }
                }
                break;

            default:
                error = HFERR_BadStatus;
                fake_scsi_sense(scsi_command,0,0,IOERR_NOCMD);
                break;
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
 * init_units
 *
 * Initialize the IDE Drives and add them to the dev->units list
 *
 * @param itask Pointer to an IDETask struct
 * @returns number of drives found
*/
static BYTE init_units(struct IOTask *itask) {
    struct ExecBase *SysBase = itask->SysBase;
    UBYTE num_units = 0;
    struct DeviceBase *dev = itask->dev;

    for (BYTE i=0; i < 2; i++) {
        struct IDEUnit *unit = AllocMem(sizeof(struct IDEUnit),MEMF_ANY|MEMF_CLEAR);

        if (unit != NULL) {
            // Setup each unit structure
            unit->itask                   = itask;
            unit->unitNum                 = ((itask->boardNum * 4) + (itask->channel << 1) + i);
            unit->SysBase                 = SysBase;
            unit->flags.primary           = ((i%2) == 1) ? false : true;
            unit->openCount               = 0;
            unit->changeCount             = 1;
            unit->deviceType              = DG_DIRECT_ACCESS;
            unit->flags.mediumPresent     = false;
            unit->flags.mediumPresentPrev = false;
            unit->flags.present           = false;
            unit->flags.atapi             = false;
            unit->flags.xferMultiple      = false;
            unit->multipleCount           = 0;
            unit->shadowDevHead           = &itask->shadowDevHead;
            *unit->shadowDevHead          = 0;

            // Initialize the change int list
            unit->changeInts.mlh_Tail     = NULL;
            unit->changeInts.mlh_Head     = (struct MinNode *)&unit->changeInts.mlh_Tail;
            unit->changeInts.mlh_TailPred = (struct MinNode *)&unit->changeInts;

            Warn("testing unit %ld\n",unit->unitNum);

            void *base = itask->cd->cd_BoardAddr;
            base += (itask->channel == 0) ? CHANNEL_0 : CHANNEL_1;

            if (ata_init_unit(unit,base)) {
                if (unit->flags.atapi) itask->hasRemovables = true;
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
static void cleanup(struct IOTask *itask) {
    struct ExecBase *SysBase = itask->SysBase;
    if (itask->iomp)
        L_DeletePort(itask->iomp);

    delete_timer(SysBase,itask->timermp,itask->tr,false);
    delete_timer(SysBase,itask->dcTimerMp,itask->dcTimerReq,itask->dcTimerArmed);

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
 * diskchange_check
 *
 * Check for disk changes and send interrupts if necessary
 *
 * @param itask Pointer to the IO task
 */
static void diskchange_check(struct IOTask *itask) {
    Info("diskchange check...\n");
    struct ExecBase *SysBase = itask->SysBase;
    struct IDEUnit *unit;
    struct IOStdReq *intreq = NULL;
    bool present;

    if (SysBase->LibNode.lib_Version >= 36) {
        ObtainSemaphoreShared(&itask->dev->ulSem);
    } else {
        ObtainSemaphore(&itask->dev->ulSem);
    }

    for (unit = (struct IDEUnit *)itask->dev->units.mlh_Head;
        unit->mn_Node.mln_Succ != NULL;
        unit = (struct IDEUnit *)unit->mn_Node.mln_Succ)
    {
        if (unit->flags.present && unit->flags.atapi) {

            present = (atapi_test_unit_ready(unit,true) == 0) ? true : false;

            if (present != unit->flags.mediumPresentPrev) {

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

            unit->flags.mediumPresentPrev = present;


        }
    }

    ReleaseSemaphore(&itask->dev->ulSem);
}

/**
 * process_ioreq
 *
 * Process an IO request for the IO task
 *
 * @param ioreq Pointer to the IO request
 * @param itask Pointer to the IO task
 */
static void process_ioreq(struct IOTask *itask, struct IOStdReq *ioreq) {
    struct ExecBase *SysBase = itask->SysBase;
    struct IOExtTD *iotd;
    struct IDEUnit *unit;
    UWORD blockShift;
    uint64_t lba;
    ULONG lba_high, lba_low;
    ULONG count;
    BYTE  error = 0;
    enum xfer_dir direction = WRITE;
    unit = (struct IDEUnit *)ioreq->io_Unit;
    iotd = (struct IOExtTD *)ioreq;

    direction = WRITE;

    switch (ioreq->io_Command) {
        case CMD_PAUSE:
            itask->paused = true;
            ioreq->io_Error = 0;
            ReplyMsg(&ioreq->io_Message);
            Wait(SIGBREAKF_CTRL_D);
            return;

        case CMD_START:
            if (unit->flags.atapi) {
                error = atapi_start_stop_unit(unit,true,false,false);
            }
            break;

        case CMD_STOP:
            if (unit->flags.atapi) {
                error = atapi_start_stop_unit(unit,false,false,false);
            }
            break;

        case TD_EJECT:
            if (!unit->flags.atapi) {
                error  = IOERR_NOCMD;
                break;
            }
            ioreq->io_Actual = (unit->flags.mediumPresent) ? 0 : 1;   // io_Actual reflects the previous state

            bool insert = (ioreq->io_Length == 0) ? true : false;

            error = atapi_start_stop_unit(unit,insert,1,false);
            break;

        case TD_CHANGESTATE:
            error   = 0;
            ioreq->io_Actual = 0;
            if (unit->flags.atapi) {
                ioreq->io_Actual = (atapi_test_unit_ready(unit,false) != 0);
                break;
            }
            break;

        case TD_PROTSTATUS:
            error  = 0;
            if (unit->flags.atapi) {
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
            if (unit->flags.atapi == true && unit->flags.mediumPresent == false) {
                Trace("Access attempt without media\n");
                error  = TDERR_DiskChanged;
                break;
            }

            blockShift = ((struct IDEUnit *)ioreq->io_Unit)->blockShift;

            // This looks like a lond-winded way to get the LBA doesn't it?
            // Splitting up the operation like this results in smaller code size (avoids 64-bit math from libgcc)
            lba_high = ioreq->io_Actual >> blockShift;
            lba_low = ioreq->io_Actual << (32 - blockShift);
            lba_low |= (ioreq->io_Offset >> blockShift);

            lba = ((uint64_t)lba_high << 32 | lba_low);

            count = (ioreq->io_Length >> blockShift);

            if (count == 0) {
                error = IOERR_BADLENGTH;
                break;
            }

            if ((lba + count) > (unit->logicalSectors)) {
                Trace("Access past end of device\n");
                error  = IOERR_BADADDRESS;
                break;
            }

            if (unit->flags.atapi == true) {
                error  = atapi_translate(ioreq->io_Data, (ULONG)lba, count, &ioreq->io_Actual, unit, direction);
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
            Info("Task: CMD_DIE: Shutting down IO Task\n");
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

/**
 * io_task
 *
 * This is a task to complete IO Requests for all units
 * Requests are sent here from begin_io via the dev->IDETaskMP Message port
*/
void __attribute__((noreturn)) io_task () {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct Task *task = FindTask(NULL);
    struct IOTask *itask = (struct IOTask *)task->tc_UserData;
    struct IOStdReq *ioreq;
    ULONG signalsSet = 0;
    ULONG signalMask = 0;

    itask->task = task;

    Trace("IO Task: CreatePort()\n");
    // Create the MessagePort used to send us requests
    if ((itask->iomp = L_CreatePort(NULL,0)) == NULL) {
        cleanup(itask);
        RemTask(NULL);
        Wait(0);
    }

    signalMask = (1 << itask->iomp->mp_SigBit);

    Trace("IO Task: CreatePort() ok\n");

    if (!create_timer(SysBase,&itask->timermp,&itask->tr,UNIT_MICROHZ)) {
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
    itask->paused = false;
    Signal(itask->parent,SIGF_SINGLE);

    if (itask->hasRemovables) {

        if (!create_timer(SysBase,&itask->dcTimerMp,&itask->dcTimerReq,UNIT_VBLANK)) {
            cleanup(itask);
            RemTask(NULL);
            Wait(0);
        }

        signalMask |= (1 << itask->dcTimerMp->mp_SigBit);

        run_timer(SysBase,itask->dcTimerReq,CHANGEINT_INTERVAL,0);
        itask->dcTimerArmed = true;
    }


    while (1) {
        // Main loop, handle IO Requests as they come in.
        Trace("IO Task: WaitPort()\n");
        signalsSet = Wait(signalMask);

        if (signalsSet & (1 << itask->iomp->mp_SigBit)) {
            while ((ioreq = (struct IOStdReq *)GetMsg(itask->iomp)) != NULL) {
              process_ioreq(itask,ioreq);
            }
        }

        if (itask->hasRemovables) {
            if (signalsSet & (1 << itask->dcTimerMp->mp_SigBit)) {
                WaitIO((struct IORequest *)itask->dcTimerReq);
                itask->dcTimerArmed = false;

                diskchange_check(itask);
                run_timer(SysBase,itask->dcTimerReq,CHANGEINT_INTERVAL,0);

                itask->dcTimerArmed = true;
            }
        }
    }
}
