// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <inline/timer.h>
#include <proto/exec.h>
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
#include "blockcopy.h"
#include "sleep.h"
#include "lide_alib.h"

static BYTE write_taskfile_lba(struct IDEUnit *unit, UBYTE command, uint64_t lba, UBYTE sectorCount, UBYTE features);
static BYTE write_taskfile_lba48(struct IDEUnit *unit, UBYTE command, uint64_t lba, UBYTE sectorCount, UBYTE features);
static BYTE write_taskfile_chs(struct IDEUnit *unit, UBYTE command, uint64_t lba, UBYTE sectorCount, UBYTE features);

/**
 * ata_cia_settle
 *
 * We need a short delay before actually checking the status register to let the drive update the status
 * A CIA access takes a fixed number of E-clock cycles regardless of CPU speed
 *
*/
static inline void ata_cia_settle(void) {
    asm volatile (
        "tst.b 0xBFE001"
        : : : "cc"
    );
}

/**
 * ata_save_error
 *
 * Save the contents of the drive registers so that errors can be reported in sense data
 *
*/
static void ata_save_error(struct IDEUnit *unit) {
    unit->last_error[0] = *unit->drive.error_features;
    unit->last_error[1] = *unit->drive.lbaHigh;
    unit->last_error[2] = *unit->drive.lbaMid;
    unit->last_error[3] = *unit->drive.lbaLow;
    unit->last_error[4] = *unit->drive.status_command;
    unit->last_error[5] = *unit->drive.devHead;
}

/**
 * ata_check_error
 *
 * Check for ATA error / fault
 *
 * @param unit Pointer to an IDEUnit struct
 * @returns True if error is indicated
*/
static bool ata_check_error(struct IDEUnit *unit) {
    return (*unit->drive.status_command & (ata_flag_error | ata_flag_df));
}

/**
 * ata_wait_drq
 *
 * Poll DRQ in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
 * @param fast More aggressive polling, 1000 tries before timer wait vs 100
*/
static bool ata_wait_drq(struct IDEUnit *unit, ULONG tries, bool fast) {
    struct timerequest *tr = unit->itask->tr;
    Trace("wait_drq enter\n");
    UBYTE status;

    int loops = (fast) ? 1000 : 100;

    ata_cia_settle();

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<loops; j++) {
            status = *unit->drive.status_command;
            // Fast path: BSY clear, DRQ set and no error
            if ((status & (ata_flag_busy | ata_flag_drq | ata_flag_error | ata_flag_df)) == ata_flag_drq) return true;
            // Status bits other than BSY are only valid once BSY is clear
            if ((status & ata_flag_busy) == 0 && (status & (ata_flag_error | ata_flag_df))) return false;
        }
        sleep_us(tr,ATA_DRQ_WAIT_LOOP_US);
    }
    Trace("wait_drq timeout\n");
    return false;
}

#ifdef TF_IDE
/**
 * ata_tf_wait_status_slow
 *
 * Wait for data or command completion after the initial status polls
 *
 * Status is checked through the alternate status register, which does not clear INTRQ.
 * The interrupt signal is only ever cleared by Wait(), so an interrupt that arrives
 * before the check, or one left over from an earlier poll, only causes one more check.
 * Without an interrupt server, poll status and check the same timeout periodically.
 *
 * @param unit Pointer to an IDEUnit struct
 * @param data True for DRQ set, false for DRQ clear at command completion
 * @returns True on the expected status, false on error or timeout
*/
static bool __attribute__((noinline)) ata_tf_wait_status_slow(struct IDEUnit *unit, bool data) {
    struct ExecBase *SysBase    = unit->SysBase;
    struct IOTask *itask        = unit->itask;
    struct timerequest *tr      = itask->tr;
    ULONG timerSig              = 1UL << tr->tr_node.io_Message.mn_ReplyPort->mp_SigBit;
    bool timedOut               = false;
    bool result                 = false;
    UWORD polls                 = 0;
    UBYTE status;

    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_sec     = ATA_DRQ_WAIT_S;
    tr->tr_time.tv_micro   = 0;
    SendIO((struct IORequest *)tr);

    for (;;) {
        status = *unit->drive.alt_status;
        if ((status & ata_flag_busy) == 0) {
            if (status & (ata_flag_error | ata_flag_df)) break;
            if (!!(status & ata_flag_drq) == data) {
                result = true;
                break;
            }
        }

        if (timedOut) {
            Trace("wait_status_irq timeout\n");
            break;
        }

        if (!itask->irqInstalled) {
            if (++polls == TF_STATUS_POLL_LOOPS) {
                polls = 0;
                timedOut = (CheckIO((struct IORequest *)tr) != NULL);
            }
            continue;
        }

        if (Wait(itask->irqSigMask | timerSig) & timerSig) {
            timedOut = (CheckIO((struct IORequest *)tr) != NULL);
        }
    }

    if (!CheckIO((struct IORequest *)tr)) AbortIO((struct IORequest *)tr);
    WaitIO((struct IORequest *)tr);
    SetSignal(0,timerSig);

    return result;
}

/* Keep the common ready path free of the timer wait's register saves.
 * Callers supply the settling interval before the first status read.
 */
static inline bool __attribute__((always_inline)) ata_tf_wait_status(struct IDEUnit *unit, bool data) {
    UBYTE expected = data ? ata_flag_drq : 0;

    for (int i=0; i<TF_STATUS_POLL_LOOPS; i++) {
        UBYTE status = *unit->drive.alt_status;
        if ((status & (ata_flag_busy | ata_flag_drq | ata_flag_error | ata_flag_df)) == expected)
            return true;
        if ((status & ata_flag_busy) == 0 && (status & (ata_flag_error | ata_flag_df)))
            return false;
    }

    return ata_tf_wait_status_slow(unit,data);
}
#endif

/**
 * ata_wait_data
 *
 * Wait until the drive is ready to transfer the next DRQ block
 *
 * @param unit Pointer to an IDEUnit struct
 * @param irq True if the drive raises INTRQ for this block: every PIO in block, and
 *            every PIO out block except the first
 * @returns True if DRQ was set, false on error or timeout
*/
static inline bool ata_wait_data(struct IDEUnit *unit, bool irq) {
#ifdef TF_IDE
    if (irq && unit->itask->irqInstalled) {
        ata_cia_settle();
        return ata_tf_wait_status(unit,true);
    }
#endif
    return ata_wait_drq(unit,ATA_DRQ_WAIT_COUNT,true);
}

/* Between PIO read blocks, one discarded alternate-status access supplies
 * the required PIO cycle without acknowledging the drive interrupt.
 * Command starts and other transfer paths retain the CIA settling delay.
 */
static inline bool ata_wait_read_data(struct IDEUnit *unit, bool firstBlock) {
#ifdef TF_IDE
    if (unit->itask->tf.board && unit->xferMethod == word_move) {
        if (firstBlock) ata_cia_settle();
        else (void)*unit->drive.alt_status;
        return ata_tf_wait_status(unit,true);
    }
#endif
    return ata_wait_data(unit,true);
}

/* A command is complete only after both BSY and DRQ have cleared. */
static bool ata_wait_complete(struct IDEUnit *unit, bool irq) {
#ifdef TF_IDE
    if (irq && unit->itask->irqInstalled) {
        ata_cia_settle();
        return ata_tf_wait_status(unit,false);
    }
#endif
    ata_cia_settle();
    for (ULONG i=0; i<ATA_BSY_WAIT_COUNT; i++) {
        for (int j=0; j<100; j++) {
            UBYTE status = *unit->drive.status_command;
            if ((status & ata_flag_busy) == 0) {
                if (status & (ata_flag_error | ata_flag_df)) return false;
                if ((status & ata_flag_drq) == 0) return true;
            }
        }
        sleep_us(unit->itask->tr,ATA_BSY_WAIT_LOOP_US);
    }
    return false;
}

/**
 * ata_wait_not_busy
 *
 * Poll BSY in the status register until clear or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool ata_wait_not_busy(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->itask->tr;

    ata_cia_settle();

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<100; j++) {
            if ((*unit->drive.status_command & ata_flag_busy) == 0) return true;
        }
        sleep_us(tr,ATA_BSY_WAIT_LOOP_US);
    }
    return false;
}

/**
 * ata_wait_ready
 *
 * Poll RDY in the status register until set or timeout
 * @param unit Pointer to an IDEUnit struct
 * @param tries Tries, sets the timeout
*/
static bool ata_wait_ready(struct IDEUnit *unit, ULONG tries) {
    struct timerequest *tr = unit->itask->tr;

    ata_cia_settle();

    for (int i=0; i < tries; i++) {
        // Try a bunch of times before imposing the speed penalty of the timer...
        for (int j=0; j<1000; j++) {
            if ((*unit->drive.status_command & (ata_flag_ready | ata_flag_busy)) == ata_flag_ready) return true;
        }
        sleep_us(tr,ATA_RDY_WAIT_LOOP_US);
    }
    return false;
}

/**
 * ata_select
 *
 * Selects the drive by writing to the dev head register
 * If the request would not change the drive select then this will be a no-op to save time
 *
 * @param unit Pointer to an IDEUnit struct
 * @param select the dev head value to set
 * @param wait whether to wait for the drive to clear BSY after selection
 * @returns true if the drive selection was changed
*/
bool ata_select(struct IDEUnit *unit, UBYTE select, bool wait)
{
    bool changed = false;
    volatile UBYTE *shadowDevHead = unit->shadowDevHead;

    if (!unit->flags.lba) select &= ~(0x40);

    if (*shadowDevHead == select) {
        return false;
    }

    if ((*shadowDevHead & 0xF0) != (select & 0xF0)) changed = true;

    // Wait for BSY to clear before changing drive unless there's no drive selected
    if (*shadowDevHead != 0 && changed) ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT);

    *unit->drive.devHead = select;

    if (changed && wait) {
        sleep_us(unit->itask->tr,5); // Could possibly be replaced with call to ata_cia_settle
        ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT);
    }

    *shadowDevHead = select;

    return changed;
}

/**
 * ata_identify
 *
 * Send an IDENTIFY command to the device and place the results in the buffer
 * @param unit Pointer to an IDEUnit struct
 * @param buffer Pointer to the destination buffer
 * @return false on error
*/
bool ata_identify(struct IDEUnit *unit, UWORD *buffer)
{
    UBYTE drvSel = (unit->flags.primary) ? 0xE0 : 0xF0; // Select drive

    ata_select(unit,drvSel,false);

    if (!ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT)) return false;

    ata_irq_enable(unit,false);

    *unit->drive.sectorCount    = 0;
    *unit->drive.lbaLow         = 0;
    *unit->drive.lbaMid         = 0;
    *unit->drive.lbaHigh        = 0;
    *unit->drive.error_features = 0;
    *unit->drive.devHead        = drvSel;
    *unit->drive.status_command = ATA_CMD_IDENTIFY;

    if (!ata_wait_drq(unit,500,false)) {
        Warn("ATA: IDENTIFY Status: Error\n");
        Warn("ATA: last_error: %08lx\n",&unit->last_error[0]);
        // Save error information
        ata_save_error(unit);
        return false;
    }

    if (buffer) {
        UWORD read_data;
        for (int i=0; i<256; i++) {
            read_data = *unit->drive.data; //autoincrement on the ide-side
            // Interface is byte-swapped, so swap the identify data back.
            buffer[i] = ((read_data >> 8) | (read_data << 8));
        }
    }

    return true;
}

/**
 * ata_set_xfer
 *
 * Sets the transfer routine for the unit
 *
 * @param unit Pointer to an IDEUnit struct
 * @param method Transfer routine
 */
void ata_set_xfer(struct IDEUnit *unit, enum xfer method) {
#ifdef TF_IDE
    bool mover = method == tf_block_move && !unit->flags.atapi &&
                 (unit->itask->tf.caps & TF_CAP_READ) &&
                 unit->SysBase->LibNode.lib_Version >= 37;
    if (!mover && method != longword_move) method = word_move;
#endif
    switch (method) {
#ifdef TF_IDE
        case tf_block_move:
#endif
        case word_move:
            unit->read_fast       = ata_read_word_move;
            unit->read_unaligned  = ata_read_unaligned_word;
            unit->write_fast      = ata_write_word_move;
            unit->write_unaligned = ata_write_unaligned_word;

            unit->xferMethod = method;
            break;

        default:
#if NEXT_REG >= 512
        case longword_movem:
            unit->read_fast       = ata_read_long_movem;
            unit->read_unaligned  = ata_read_unaligned_long;
            unit->write_fast      = ata_write_long_movem;
            unit->write_unaligned = ata_write_unaligned_long;

            unit->xferMethod = longword_movem;
            break;
#endif
        case longword_move:
            unit->read_fast       = ata_read_long_move;
            unit->read_unaligned  = ata_read_unaligned_long;
            unit->write_fast      = ata_write_long_move;
            unit->write_unaligned = ata_write_unaligned_long;

            unit->xferMethod = longword_move;
            break;
    }
}

/**
 * ata_init_unit
 *
 * Initialize a unit, check if it is there and responding
 * @param unit Pointer to an IDEUnit struct
 * @param base Base address of the drive registers
 * @returns false on error
*/
bool ata_init_unit(struct IDEUnit *unit, void *base) {
    struct ExecBase *SysBase = unit->SysBase;
    bool dev_found = false;
    UWORD *buf;

    unit->cylinders           = 0;
    unit->heads               = 0;
    unit->sectorsPerTrack     = 0;
    unit->blockSize           = 0;
    unit->flags.present       = false;
    unit->flags.mediumPresent = false;


    unit->drive.data           = (UWORD*) (base + ata_reg_data);
    unit->drive.error_features = (UBYTE*) (base + ata_reg_error);
    unit->drive.sectorCount    = (UBYTE*) (base + ata_reg_sectorCount);
    unit->drive.lbaLow         = (UBYTE*) (base + ata_reg_lbaLow);
    unit->drive.lbaMid         = (UBYTE*) (base + ata_reg_lbaMid);
    unit->drive.lbaHigh        = (UBYTE*) (base + ata_reg_lbaHigh);
    unit->drive.devHead        = (UBYTE*) (base + ata_reg_devHead);
    unit->drive.status_command = (UBYTE*) (base + ata_reg_status);
    *unit->shadowDevHead = *unit->drive.devHead = (unit->flags.primary) ? 0xE0 : 0xF0; // Select drive

#ifdef TF_IDE
    unit->drive.alt_status = (UBYTE*) (base + CHANNEL_1 + ata_reg_altStatus);
    *unit->drive.alt_status = nIEN; // Disable interrupts
#endif

    ata_set_xfer(unit,longword_movem);

    for (int i=0; i<(8*NEXT_REG); i+=NEXT_REG) {
        // Check if the bus is floating (D7/6 pulled-up with resistors)
        if ((i != ata_reg_devHead) && (*((volatile UBYTE *)base + i) & 0xC0) != 0xC0) {
            dev_found = true;
            Trace("INIT: Unit base: %08lx; Drive base %08lx\n",unit, unit->drive);
            break;
        }
    }

    if (dev_found == false || !ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT)) {
        return false;
    }

    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) { // Allocate buffer for IDENTIFY result
        return false;
    }

    Trace("INIT: IDENTIFY\n");
    if (ata_identify(unit,buf) == true) {
        Info("INIT: ATA Drive found!\n");

        unit->flags.lba           = (buf[ata_identify_capabilities] & ata_capability_lba) != 0;
        unit->cylinders           = buf[ata_identify_cylinders];
        unit->heads               = buf[ata_identify_heads];
        unit->sectorsPerTrack     = buf[ata_identify_sectors];
        unit->blockSize           = 512;
        unit->logicalSectors      = (uint64_t)buf[ata_identify_logical_sectors+1] << 16 | buf[ata_identify_logical_sectors];
        unit->blockShift          = 0;
        unit->flags.mediumPresent = true;
        unit->multipleCount       = buf[ata_identify_multiple] & 0xFF;

        if (unit->multipleCount > 0 && (ata_set_multiple(unit,unit->multipleCount) == 0)) {
            unit->flags.xferMultiple = true;
        } else {
            unit->flags.xferMultiple = false;
            unit->multipleCount = 1;
        }

#ifdef TF_IDE
        unit->itask->tf.maxTiming[unit->flags.primary ? 0 : 1] =
            tf_drive_timing(buf,unit->itask->tf.caps);
        unit->itask->tf.timing[unit->flags.primary ? 0 : 1] = 0;
        // Change host timing only after the drive accepts the transfer mode.
        if (buf[ata_identify_field_valid] & ata_field_valid_pio) {
            BYTE pioError = IOERR_NOCMD;
            if (buf[ata_identify_pio_modes] & ata_pio_mode_4)
                pioError = ata_set_pio(unit,4);
            if (pioError && (buf[ata_identify_pio_modes] & ata_pio_mode_3))
                ata_set_pio(unit,3);
        }
#endif

        if ((buf[ata_identify_features] & ata_feature_lba48) && unit->logicalSectors >= 0xFFFFFFF) {

            unit->flags.lba48 = true;
            Info("INIT: Large drive, using LBA48 mode \n");
            unit->logicalSectors = 0;
            unit->logicalSectors |= ((uint64_t)buf[ata_identify_lba48_sectors])   << 0;
            unit->logicalSectors |= ((uint64_t)buf[ata_identify_lba48_sectors+1]) << 16;
            unit->logicalSectors |= ((uint64_t)buf[ata_identify_lba48_sectors+2]) << 32;

            unit->write_taskfile = &write_taskfile_lba48;

        } else if (unit->flags.lba == true) {
            // LBA-28 up to 127GB
            unit->write_taskfile = &write_taskfile_lba;
        } else {
            // CHS Mode
            Warn("INIT: Drive doesn't support LBA mode\n");
            unit->write_taskfile = &write_taskfile_chs;
            unit->logicalSectors = (unit->cylinders * unit->heads * unit->sectorsPerTrack);
        }

        if (unit->flags.lba48) {
            Info("INIT: Logical sectors: 0x%lx%08lx\n",(uint64_t)unit->logicalSectors);
        } else {
            Info("INIT: Logical sectors: %ld\n",(ULONG)unit->logicalSectors);
        }

        if (unit->logicalSectors >= 267382800) {
            // For drives larger than 127GB fudge the geometry
            unit->heads           = 64;
            unit->sectorsPerTrack = 256;
            unit->cylinders       = unit->logicalSectors >> 14;
        } else if (unit->logicalSectors >= 16514064) {
            // If a drive is larger than 8GB then the drive will report a geometry of 16383/16/63 (CHS)
            // In this case generate a new Cylinders value
            unit->heads           = 16;
            unit->sectorsPerTrack = 255;
            unit->cylinders       = ((ULONG)unit->logicalSectors / (16*255));
            Info("INIT: Adjusting geometry, new geometry; 16/255/%ld\n",unit->cylinders);
        }

        if (unit->heads > 0 &&
            unit->logicalSectors > 0 &&
            unit->cylinders > 0 &&
            unit->blockSize > 0) {

            unit->flags.present = true;

            if (unit->flags.lba48) {
                Info("INIT: LBAs 0x%lx%08lx Blocksize: %ld\n",(uint64_t)unit->logicalSectors,unit->blockSize);
            } else {
                Info("INIT: LBAs %ld Blocksize: %ld\n",(ULONG)unit->logicalSectors,unit->blockSize);
            }

        } else {
            Warn("INIT: Error! bs %ld ls %ld cyl %ld hd %ld\n", unit->blockSize, unit->logicalSectors, unit->cylinders, unit->heads);
        }

    } else if (atapi_init_unit(unit,buf)) {
        unit->flags.present = true;
#ifdef TF_IDE
        unit->itask->tf.timing[unit->flags.primary ? 0 : 1] = 0;
#endif
    }

    if (unit->flags.present) {
#ifdef TF_IDE
        if (!unit->flags.atapi) ata_set_xfer(unit,tf_block_move);
#endif
        while ((unit->blockSize >> unit->blockShift) > 1) {
            unit->blockShift++;
        }
        Info("INIT: Blockshift: %ld\n",unit->blockShift);
    }

    if (buf) FreeMem(buf,512);
    return unit->flags.present;
}

/**
 * ata_set_multiple
 *
 * Configure the DRQ block size for READ MULTIPLE and WRITE MULTIPLE
 * @param unit Pointer to an IDEUnit struct
 * @param multiple DRQ Block size
 * @return non-zero on error
*/
BYTE ata_set_multiple(struct IDEUnit *unit, BYTE multiple) {
    UBYTE drvSel = (unit->flags.primary) ? 0xE0 : 0xF0; // Select drive

    ata_select(unit,drvSel,true);

    ata_irq_enable(unit,false);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
            return HFERR_SelTimeout;

    *unit->drive.sectorCount    = multiple;
    *unit->drive.lbaLow         = 0;
    *unit->drive.lbaMid         = 0;
    *unit->drive.lbaHigh        = 0;
    *unit->drive.error_features = 0;
    *unit->drive.status_command = ATA_CMD_SET_MULTIPLE;

    if (!ata_wait_not_busy(unit,ATA_BSY_WAIT_COUNT))
        return IOERR_UNITBUSY;

    if (ata_check_error(unit)) {
        if (*unit->drive.error_features & ata_err_flag_aborted) {
            return IOERR_ABORTED;
        } else {
            return TDERR_NotSpecified;
        }
    }

    return 0;
}

// -fno-unroll-loops: amiga-gcc's -O3 defaults to -funroll-loops, which bloats
// the PIO transfer/poll loops below and hurts perf
// -fno-tree-scev-cprop: stops gcc replacing the per-sector buffer steps with
// buffer + (block << 9) after the loop, which costs a 68000 two slow shifts per DRQ block
#pragma GCC push_options
#pragma GCC optimize ("-O3","-fno-unroll-loops","-fno-tree-scev-cprop")

/**
 * ata_read
 *
 * Read blocks from the unit
 * @param buffer destination buffer
 * @param lba LBA Address
 * @param count Number of blocks to transfer
 * @param actual Completed byte count
 * @param unit Pointer to the unit structure
 * @returns error
*/
static BYTE ata_read(void *buffer, uint64_t lba, ULONG count, ULONG *actual, struct IDEUnit *unit) {
    Trace("ata_read enter\n");
    Trace("ATA: Request sector count: %ld\n",count);

    UBYTE error = 0;
    ULONG txn_count; // Amount of sectors to transfer in the current READ/WRITE command

    UBYTE command;
    UBYTE multipleCount = unit->multipleCount;
    volatile void *dataRegister = unit->drive.data;
#ifdef TF_IDE
    bool tfRead = unit->xferMethod == tf_block_move;
#endif

    if (unit->flags.lba48) {
        command = (unit->flags.xferMultiple) ? ATA_CMD_READ_MULTIPLE_EXT : ATA_CMD_READ_EXT;
    } else {
        command = (unit->flags.xferMultiple) ? ATA_CMD_READ_MULTIPLE : ATA_CMD_READ;
    }

    ata_xfer_func ata_xfer;

    /* If the buffer is not word-aligned we need to use a slower routine */
    if (((ULONG)buffer) & 0x01) {
        ata_xfer = unit->read_unaligned;
    } else {
        ata_xfer = unit->read_fast;
    }

    UBYTE drvSel = (unit->flags.primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)) {
        ata_save_error(unit);
        return HFERR_SelTimeout;
    }

    /**
     * Transfer up-to MAX_TRANSFER_SECTORS per ATA command invocation
     *
     * count:          Number of sectors to transfer for this io request
     * txn_count:      Number of sectors to transfer to/from the drive in one ATA command transaction
     * multiple_count: Max number of sectors that can be transferred before polling DRQ
     */
    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            txn_count = MAX_TRANSFER_SECTORS;
        } else {
            txn_count = count;               // Get any remainders
        }
        count -= txn_count;

        Trace("ATA: XFER Count: %ld, txn_count: %ld\n",count,txn_count);

        if ((error = unit->write_taskfile(unit,command,lba,txn_count,0)) != 0) {
            ata_save_error(unit);
            return error;
        }

        lba += txn_count;

        ULONG txn_bytes = txn_count << 9;
        bool firstBlock = true;

        while (txn_count) {
            if (!ata_wait_read_data(unit,firstBlock)) {
                ata_save_error(unit);
                return IOERR_UNITBUSY;
            }
            firstBlock = false;

            ULONG block = (txn_count < multipleCount) ? txn_count : multipleCount;
            txn_count -= block;
#ifdef TF_IDE
            if (tfRead && tf_read_block(unit,buffer,block)) {
                buffer += block << 9;
                continue;
            }
#endif
            // Step the buffer per sector: a 68000 has no barrel shifter, so block << 9 is costly
            do {
                ata_xfer((void *)dataRegister,buffer);
                buffer += 512;
            } while (--block);
        }

        if (!ata_wait_complete(unit,false)) {
            ata_save_error(unit);
            return TDERR_NotSpecified;
        }
        *actual += txn_bytes;
    }

    return 0;
}

/**
 * ata_write
 *
 * Write blocks to the unit
 * @param buffer source buffer
 * @param lba LBA Address
 * @param count Number of blocks to transfer
 * @param actual Completed byte count
 * @param unit Pointer to the unit structure
 * @returns error
*/
static BYTE ata_write(void *buffer, uint64_t lba, ULONG count, ULONG *actual, struct IDEUnit *unit) {
    Trace("ata_write enter\n");
    Trace("ATA: Request sector count: %ld\n",count);

    UBYTE error = 0;

    ULONG txn_count; // Amount of sectors to transfer in the current READ/WRITE command

    UBYTE command;
    UBYTE multipleCount = unit->multipleCount;
    volatile void *dataRegister = unit->drive.data;

    if (unit->flags.lba48) {
        command = (unit->flags.xferMultiple) ? ATA_CMD_WRITE_MULTIPLE_EXT : ATA_CMD_WRITE_EXT;
    } else {
        command = (unit->flags.xferMultiple) ? ATA_CMD_WRITE_MULTIPLE : ATA_CMD_WRITE;
    }

    ata_xfer_func ata_xfer;

    /* If the buffer is not word-aligned we need to use a slower routine */
    if ((ULONG)buffer & 0x01) {
        ata_xfer = unit->write_unaligned;
    } else {
        ata_xfer = unit->write_fast;
    }

    UBYTE drvSel = (unit->flags.primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)) {
        ata_save_error(unit);
        return HFERR_SelTimeout;
    }

    /**
     * Transfer up-to MAX_TRANSFER_SECTORS per ATA command invocation
     *
     * count:          Number of sectors to transfer for this io request
     * txn_count:      Number of sectors to transfer to/from the drive in one ATA command transaction
     * multiple_count: Max number of sectors that can be transferred before polling DRQ
     */
    while (count > 0) {
        if (count >= MAX_TRANSFER_SECTORS) { // Transfer 256 Sectors at a time
            txn_count = MAX_TRANSFER_SECTORS;
        } else {
            txn_count = count;               // Get any remainders
        }
        count -= txn_count;

        Trace("ATA: XFER Count: %ld, txn_count: %ld\n",count,txn_count);

        if ((error = unit->write_taskfile(unit,command,lba,txn_count,0)) != 0) {
            ata_save_error(unit);
            return error;
        }

        lba += txn_count;

        ULONG txn_bytes = txn_count * 512;
        bool firstBlock = true; // The drive does not interrupt for the first PIO out block

        while (txn_count) {
            if (!ata_wait_data(unit,!firstBlock)) {
                ata_save_error(unit);
                return IOERR_UNITBUSY;
            }
            firstBlock = false;

            /* Transfer up to (multiple_count) sectors before polling DRQ again */
            ULONG block = (txn_count < multipleCount) ? txn_count : multipleCount;
            txn_count -= block;
            do {
                ata_xfer(buffer,(void *)dataRegister);
                buffer += 512;
            } while (--block);
        }

        if (!ata_wait_complete(unit,true)) {
            ata_save_error(unit);
            return TDERR_NotSpecified;
        }
        *actual += txn_bytes;

    }

    return 0;
}

/**
 * ata_readwrite
 *
 * A wrapper around ata_read/write
 *
 * @param buffer destination buffer
 * @param lba LBA Address
 * @param count Number of blocks to transfer
 * @param actual Receives the completed byte count, including on error
 * @param unit Pointer to the unit structure
 * @param dir Direction (read/write)
 * @returns error
 */
BYTE ata_readwrite(void *buffer, uint64_t lba, ULONG count, ULONG *actual, struct IDEUnit *unit, enum xfer_dir dir) {
    BYTE error;
    *actual = 0;
    ata_irq_enable(unit,true);
    if (dir == READ) {
        error = ata_read(buffer,lba,count,actual,unit);
    } else {
        error = ata_write(buffer,lba,count,actual,unit);
    }
    ata_irq_enable(unit,false);

    return error;
}

/**
 * ata_read_unaligned_long
 *
 * Read data to an unaligned buffer
 * @param source Pointer to the drive data port
 * @param destination Pointer to the data buffer
*/
void ata_read_unaligned_long(void *source asm("a0"), void *destination asm("a1")) {
    ULONG readLong;
    UBYTE *dest = (UBYTE *)destination;

    for (int i=0; i<(512/4); i++) {          // Read (512 / 4) Long words from drive
        readLong = *(volatile ULONG *)source;
        dest[0] = ((readLong >> 24) & 0xFF); // Write it out in 4 bytes
        dest[1] = ((readLong >> 16) & 0xFF);
        dest[2] = ((readLong >> 8) & 0xFF);
        dest[3] = (readLong & 0xFF);
        dest += 4;                           // Increment dest pointer by 4 bytes
    }
}

/**
 * ata_write_unaligned_long
 *
 * Write data from an unaligned buffer
 * @param source Pointer to the data buffer
 * @param destination Pointer to the drive data port
*/
void ata_write_unaligned_long(void *source asm("a0"), void *destination asm("a1")) {
    UBYTE *src = (UBYTE *)source;
    for (int i=0; i<(512/4); i++) {  // Write (512 / 4) Long words to drive
        *(volatile ULONG *)destination = (src[0] << 24 | src[1] << 16 | src[2] << 8 | src[3]);
        src += 4;
    }
}

/**
 * ata_read_unaligned_word
 *
 * Read data to an unaligned buffer from a word-only data port
 * @param source Pointer to the drive data port
 * @param destination Pointer to the data buffer
*/
void ata_read_unaligned_word(void *source asm("a0"), void *destination asm("a1")) {
    UWORD readWord;
    UBYTE *dest = (UBYTE *)destination;

    for (int i=0; i<(512/2); i++) {          // Read (512 / 2) words from drive
        readWord = *(volatile UWORD *)source;
        dest[0] = ((readWord >> 8) & 0xFF);
        dest[1] = (readWord & 0xFF);
        dest += 2;
    }
}

/**
 * ata_write_unaligned_word
 *
 * Write data from an unaligned buffer to a word-only data port
 * @param source Pointer to the data buffer
 * @param destination Pointer to the drive data port
*/
void ata_write_unaligned_word(void *source asm("a0"), void *destination asm("a1")) {
    UBYTE *src = (UBYTE *)source;
    for (int i=0; i<(512/2); i++) {  // Write (512 / 2) words to drive
        *(volatile UWORD *)destination = (src[0] << 8 | src[1]);
        src += 2;
    }
}

/**
 * write_taskfile_chs
 *
 * @param unit Pointer to an IDEUnit struct
 * @param lba  Pointer to the LBA variable
*/
static BYTE write_taskfile_chs(struct IDEUnit *unit, UBYTE command, uint64_t lba, UBYTE sectorCount, UBYTE features) {

    UWORD cylinder = ((ULONG)lba / (unit->heads * unit->sectorsPerTrack));
    UBYTE head     = (((ULONG)lba / unit->sectorsPerTrack) % unit->heads) & 0xF;
    UBYTE sector   = ((ULONG)lba % unit->sectorsPerTrack) + 1;

    BYTE devHead;

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
        return HFERR_SelTimeout;

    devHead = ((unit->flags.primary) ? 0xA0 : 0xB0) | (head & 0x0F);

    *unit->shadowDevHead         = devHead;
    *unit->drive.devHead        = devHead;
    *unit->drive.sectorCount    = sectorCount; // Count value of 0 indicates to transfer 256 sectors
    *unit->drive.lbaLow         = (UBYTE)(sector);
    *unit->drive.lbaMid         = (UBYTE)(cylinder);
    *unit->drive.lbaHigh        = (UBYTE)(cylinder >> 8);
    *unit->drive.error_features = features;
    *unit->drive.status_command = command;

    return 0;
}

/**
 * write_taskfile_lba
 *
 * @param unit Pointer to an IDEUnit struct
 * @param lba  Pointer to the LBA variable
*/
static BYTE write_taskfile_lba(struct IDEUnit *unit, UBYTE command, uint64_t lba, UBYTE sectorCount, UBYTE features) {
    BYTE devHead;

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
        return HFERR_SelTimeout;

    devHead = ((unit->flags.primary) ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F);

    *unit->shadowDevHead         = devHead;
    *unit->drive.devHead        = devHead;
    *unit->drive.sectorCount    = sectorCount; // Count value of 0 indicates to transfer 256 sectors
    *unit->drive.lbaLow         = (UBYTE)(lba);
    *unit->drive.lbaMid         = (UBYTE)(lba >> 8);
    *unit->drive.lbaHigh        = (UBYTE)(lba >> 16);
    *unit->drive.error_features = features;
    *unit->drive.status_command = command;

    return 0;
}

/**
 * write_taskfile_lba48
 *
 * @param unit Pointer to an IDEUnit struct
 * @param lba  Pointer to the LBA variable
*/
static BYTE write_taskfile_lba48(struct IDEUnit *unit, UBYTE command, uint64_t lba, UBYTE sectorCount, UBYTE features) {

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
        return HFERR_SelTimeout;

    *unit->drive.sectorCount    = (sectorCount == 0) ? 1 : 0;
    *unit->drive.lbaHigh        = (UBYTE)(lba >> 40);
    *unit->drive.lbaMid         = (UBYTE)(lba >> 32);
    *unit->drive.lbaLow         = (UBYTE)(lba >> 24);
    *unit->drive.sectorCount    = sectorCount; // Count value of 0 indicates to transfer 256 sectors
    *unit->drive.lbaHigh        = (UBYTE)(lba >> 16);
    *unit->drive.lbaMid         = (UBYTE)(lba >> 8);
    *unit->drive.lbaLow         = (UBYTE)(lba);
    *unit->drive.error_features = features;
    *unit->drive.status_command = command;

    return 0;
}

#pragma GCC pop_options

/**
 * ata_set_pio
 *
 * @param unit Pointer to an IDEUnit struct
 * @param pio pio mode
*/
BYTE ata_set_pio(struct IDEUnit *unit, UBYTE pio) {
    BYTE error = 0;

    if (pio > 4) return IOERR_BADADDRESS;

#ifdef TF_IDE
    struct TFController *tf = &unit->itask->tf;
    UBYTE timing = pio == 4 ? tf->maxTiming[unit->flags.primary ? 0 : 1] : 0;
    tf_restore_timing(tf);
    tf->timing[unit->flags.primary ? 0 : 1] = 0;
#endif
    if (pio > 0) pio |= 0x08;

    UBYTE drvSel = (unit->flags.primary) ? 0xE0 : 0xF0;
    ata_select(unit,drvSel,true);

    ata_irq_enable(unit,false);

    if ((error = write_taskfile_lba(unit,ATA_CMD_SET_FEATURES,0,pio,0x03)) != 0)
        return error;

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT))
        return IOERR_UNITBUSY;

    if (ata_check_error(unit)) return IOERR_BADLENGTH;

#ifdef TF_IDE
    tf->timing[unit->flags.primary ? 0 : 1] = timing;
    tf_apply_timing(tf);
#endif
    return 0;
}

/**
 * scsi_ata_passthrough
 *
 * Handle SCSI ATA PASSTHROUGH (12) command to send ATA commands to the drive
 *
 * Requires that the cmd->scsi_Data buffer is word aligned
 *
 * @param unit Pointer to an IDEUnit struct
 * @param cmd Pointer to a SCSICmd struct
 * @return non-zero on error
*/
BYTE scsi_ata_passthrough(struct IDEUnit *unit, struct SCSICmd *cmd) {

    struct SCSI_CDB_ATA *cdb = (struct SCSI_CDB_ATA *)cmd->scsi_Command;

    bool byt_blok  = (cdb->length & ATA_BYT_BLOK) ? true : false;
    UBYTE protocol = (cdb->protocol >> 1) & 0x0F;
    UBYTE t_length = cdb->length & ATA_TLEN_MASK;
    
    ULONG lba = ((cdb->devHead & 0x0F) << 24) |
                (cdb->lbaHigh << 16) |
                (cdb->lbaMid << 8) | 
                cdb->lbaLow;

    ULONG count = 0;
    BYTE  error = 0;

    volatile UWORD *src  = NULL;
    volatile UWORD *dest = NULL;

    cmd->scsi_CmdActual = cmd->scsi_CmdLength;
    cmd->scsi_Actual = 0;

    switch (t_length) {
        case 0x00: // No Data transferred
            break;
        case 0x01: // Transfer length in feature field
            count = cdb->features;
            break;
        case 0x02: // Transfer length in sector_count field
            count = cdb->sectorCount;
            break;
        default:
            return IOERR_BADLENGTH;
    }

    if (byt_blok) count *= unit->blockSize;

    if (count > (512*256)) return IOERR_BADLENGTH; // Can't be bothered supporting larger transfers

    if (count > cmd->scsi_Length) return IOERR_BADLENGTH;

    switch (protocol) {
        case ATA_NODATA:
            if (count != 0) return IOERR_BADLENGTH;
            break;

        case ATA_PIO_IN: // Data to Host
            if (count < 2) return IOERR_BADLENGTH;
            if (!cmd->scsi_Data) return IOERR_BADADDRESS;
            if ((ULONG)cmd->scsi_Data & 1) return IOERR_BADADDRESS;
            src = (UWORD *)unit->drive.data;
            dest = cmd->scsi_Data;
            break;

        case ATA_PIO_OUT: // Data to Drive
            if (count < 2) return IOERR_BADLENGTH;
            if (!cmd->scsi_Data) return IOERR_BADADDRESS;
            if ((ULONG)cmd->scsi_Data & 1) return IOERR_BADADDRESS;
            src = cmd->scsi_Data;
            dest = (UWORD *)unit->drive.data;
            break;

        default:
            return IOERR_NOCMD;

    }

    /* PIO is word-wide; reject odd lengths before touching the command or buffer. */
    if (count & 1) return IOERR_BADLENGTH;

    UBYTE drvSel = (unit->flags.primary) ? 0xE0 : 0xF0;

    ata_select(unit,drvSel,true);

    if (!ata_wait_ready(unit,ATA_RDY_WAIT_COUNT)) {
        ata_save_error(unit);
        return HFERR_SelTimeout;
    }

    ata_irq_enable(unit,false);

#ifdef TF_IDE
    if (cdb->command == ATA_CMD_SET_FEATURES && cdb->features == 3) {
        /* A raw transfer-mode change invalidates the negotiated host timing. */
        tf_restore_timing(&unit->itask->tf);
        unit->itask->tf.timing[unit->flags.primary ? 0 : 1] = 0;
    }
#endif
    if ((error = write_taskfile_lba(unit,cdb->command,lba,cdb->sectorCount,cdb->features)) != 0) {
        ata_save_error(unit);
        return error;
    }

    if (protocol == ATA_PIO_IN || protocol == ATA_PIO_OUT) {
        for (int i = 0; i < count/2; i++) {
            if (i % 256 == 0) {
                if (!ata_wait_drq(unit,ATA_DRQ_WAIT_COUNT,true)) {
                    ata_save_error(unit);
                    return IOERR_UNITBUSY;
                }
            }
            if (protocol == ATA_PIO_IN) {
                dest[i] = *src;
            } else {
                *dest = src[i];
            }
        }
    }

    if (!ata_wait_complete(unit,false)) {
        ata_save_error(unit);
        return HFERR_BadStatus;
    }

    if (ata_check_error(unit)){
        ata_save_error(unit);
        return IOERR_ABORTED;
    }

    cmd->scsi_Actual = count;

    return 0;
}
