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

#ifdef TF_IDE
#include <hardware/intbits.h>

/**
 * tf_irq_handler
 *
 * INT2 server for the TF IDE interrupt: acknowledge the drive and wake the IO task
 *
 * @param itask Pointer to the IO task (is_Data)
 * @returns 1 if the interrupt was ours
*/
ULONG __attribute__((used)) tf_irq_handler(struct IOTask *itask asm("a1")) {
    struct ExecBase *SysBase = itask->SysBase;
    volatile UBYTE *intchg = itask->tf.intchg;
    UWORD sr;

    if ((*intchg & 0x80) == 0) return 0;

    // Reading the status register makes the drive release INTRQ
    (void)itask->tf.ide[ata_reg_status];

    // Clear the pending bit like Gayle: write 0 to clear, 1 to leave alone, keep bits 0-1
    asm volatile ("move.w sr,%0\n\tor.w #0x0700,sr" : "=d" (sr) : : "cc", "memory"); // Setting interrupt mask is faster than Disable
    *intchg = (*intchg & 0x03) | 0x7C;
    asm volatile ("move.w %0,sr" : : "d" (sr) : "cc", "memory");

    Signal(itask->task,itask->irqSigMask);
    return 1;
}

/**
 * tf_irq_server
 *
 * Interrupt server entry: exec checks the Z flag rather than d0
*/
void tf_irq_server(void);
asm (
    "    .text                 \n"
    "    .even                 \n"
    "_tf_irq_server:           \n"
    "    jsr    _tf_irq_handler\n"
    "    tst.l  d0             \n"
    "    rts                   \n"
);

/**
 * tf_irq_install
 *
 * Add the INT2 server and unmask the legacy TF IDE interrupt
 * Must be called from the IO task, which owns the signal
 *
 * @param itask Pointer to the IO task
*/
static void tf_irq_install(struct IOTask *itask) {
    struct ExecBase *SysBase = itask->SysBase;

    /* TF4060 polls transfers; init leaves nIEN set and the board IRQ masked. */
    if (itask->tf.board) return;

    if ((itask->irqSigBit = AllocSignal(-1)) == -1) {
        Info("TF: No free signal, polling the drive\n");
        return;
    }

    itask->irqSigMask                = 1UL << itask->irqSigBit;
    itask->irqServer.is_Node.ln_Type = NT_INTERRUPT;
    itask->irqServer.is_Node.ln_Pri  = TF_IRQ_PRI;
    itask->irqServer.is_Node.ln_Name = ATA_TASK_NAME;
    itask->irqServer.is_Data         = itask;
    itask->irqServer.is_Code         = tf_irq_server;

    AddIntServer(INTB_PORTS,&itask->irqServer);
    itask->irqInstalled = true;

    itask->tf.ide[CHANNEL_1 + ata_reg_altStatus] = nIEN;
    *itask->tf.intena |= 0x80;
}

/**
 * tf_irq_remove
 *
 * Mask the TF IDE interrupt and remove the INT2 server
 *
 * @param itask Pointer to the IO task
*/
static void tf_irq_remove(struct IOTask *itask) {
    struct ExecBase *SysBase = itask->SysBase;

    if (!itask->irqInstalled) return;

    *itask->tf.intena &= ~0x80;
    itask->tf.ide[CHANNEL_1 + ata_reg_altStatus] = nIEN;

    RemIntServer(INTB_PORTS,&itask->irqServer);
    FreeSignal(itask->irqSigBit);
    itask->irqInstalled = false;
}
#endif

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
                direction = READ;
            case SCSI_CMD_WRITE_6:
                lba   = (((((struct SCSI_CDB_6 *)command)->lba_high & 0x1F) << 16) |
                        ((struct SCSI_CDB_6 *)command)->lba_mid << 8 |
                        ((struct SCSI_CDB_6 *)command)->lba_low);

                count = ((struct SCSI_CDB_6 *)command)->length;
                if (count == 0) count = 256;
                goto do_scsi_transfer;

            case SCSI_CMD_READ_10:
                direction = READ;
            case SCSI_CMD_WRITE_10:
                lba   = ((struct SCSI_CDB_10 *)command)->lba;
                count = ((struct SCSI_CDB_10 *)command)->length;
                goto do_scsi_transfer;

            case SCSI_CMD_READ_16:
                direction = READ;
            case SCSI_CMD_WRITE_16:
                lba   = ((struct SCSI_CDB_16 *)command)->lba;
                count = ((struct SCSI_CDB_16 *)command)->length;

    do_scsi_transfer:
                if (data == NULL || (lba + count) > unit->logicalSectors) {
                    error = IOERR_BADADDRESS;
                    fake_scsi_sense(scsi_command,lba,count,error);
                    break;
                }

                if (count > (scsi_command->scsi_Length >> unit->blockShift)) count = (scsi_command->scsi_Length >> unit->blockShift);

                error = ata_readwrite(data,lba,count,&scsi_command->scsi_Actual,unit,direction);

                if (error == 0) {
                    scsi_command->scsi_Actual = count << unit->blockShift;
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
                error = IOERR_NOCMD;
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
        if (scsi_command->scsi_SenseActual == 0)
            fake_scsi_sense(scsi_command,0,0,error);
        return HFERR_BadStatus;
    } else {
        scsi_command->scsi_Status = 0;
        return 0;
    }
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

    // Give the drives some time to get ready
    sleep_us(itask->tr,250000);

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
                if (unit->scsiCmd) DeleteSCSICmd(unit->scsiCmd);
                if (unit->senseCmd) DeleteSCSICmd(unit->senseCmd);
                FreeMem(unit,sizeof(struct IDEUnit));
            }
        }
    }

#ifdef TF_IDE
    itask->tf.timingReady = true;
    tf_apply_timing(&itask->tf);
#endif
    return num_units;
}

/**
 * cleanup
 *
 * Clean up after the task, freeing resources etc back to the system
*/
static void cleanup(struct IOTask *itask) {
    struct ExecBase *SysBase = itask->SysBase;
#ifdef TF_IDE
    tf_irq_remove(itask);
    tf_restore_timing(&itask->tf);
#endif
    if (itask->iomp)
        L_DeletePort(itask->iomp);

    delete_timer(SysBase,itask->timermp,itask->tr,false);
    delete_timer(SysBase,itask->dcTimerMp,itask->dcTimerReq,itask->dcTimerArmed);

    struct IDEUnit *unit;
    struct IDEUnit *next = NULL;
    ObtainSemaphore(&itask->dev->ulSem);
    for (unit = (struct IDEUnit *)itask->dev->units.mlh_Head;
         (next = (struct IDEUnit *)unit->mn_Node.mln_Succ) != NULL;
         unit = next) {
            if (unit->itask == itask) {
                Remove((struct Node *)unit);
                if (unit->scsiCmd != NULL) {
                    unit->scsiCmd->scsi_CmdLength = SZ_CDB_12;
                    DeleteSCSICmd(unit->scsiCmd);
                }
                if (unit->senseCmd != NULL) {
                    unit->senseCmd->scsi_CmdLength = SZ_CDB_12;
                    DeleteSCSICmd(unit->senseCmd);
                }
                FreeMem(unit,sizeof(struct IDEUnit));
            }
         }
    ReleaseSemaphore(&itask->dev->ulSem);
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
        if (unit->itask == itask && unit->flags.present && unit->flags.atapi) {

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
            SetSignal(0,SIGBREAKF_CTRL_D);
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
                ioreq->io_Actual = (atapi_test_unit_ready(unit,true) != 0);
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

            // Guard against accesses that are not divisible / aligned by the block size
            ULONG blockMask = unit->blockSize - 1;

            if (ioreq->io_Length & blockMask) {
                error = IOERR_BADLENGTH;
                break;
            }

            if (ioreq->io_Offset & blockMask) {
                error = IOERR_BADADDRESS;
                break;
            }

            blockShift = unit->blockShift;

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
                error = ata_readwrite(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit, direction);
            }
            break;

        /* SCSI Direct */
        case HD_SCSICMD:
            error = handle_scsi_command(ioreq);
            break;

        case CMD_XFER:
            if (ioreq->io_Length <= word_move
#ifdef TF_IDE
                || ioreq->io_Length == tf_block_move
#endif
            ) {
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

    ioreq->io_Error = error;
#if DEBUG & DBG_CMD
    traceCommand(ioreq);
#endif
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

#ifdef TF_IDE
    tf_irq_install(itask);
#endif

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
