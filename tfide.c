// SPDX-License-Identifier: GPL-2.0-only
#ifdef TF_IDE
#include <exec/execbase.h>
#include <libraries/configvars.h>
#include <proto/exec.h>
#include <proto/expansion.h>

#include "ata.h"
#include "tfide.h"
#include "debug.h"

/* TF4060 registers follow its configured board. Older TF boards use Gayle's
 * fixed window and do not expose the TF4060 timing or block-mover registers.
 */
bool tf_ide_init(struct Library *ExpansionBase, struct TFController *tf) {
    struct ConfigDev *cd = FindConfigDev(NULL,TF_MANUF_ID,TF4060_PROD_ID);

    if (cd) {
        if ((cd->cd_Flags & CDF_SHUTUP) || !cd->cd_BoardAddr ||
            (cd->cd_Rom.er_Type & ERT_TYPEMASK) != ERT_ZORROII ||
            cd->cd_BoardSize <= 0xe000) return false;

        tf->board = cd->cd_BoardAddr;
        tf->ide = tf->board + TF4060_IDE_OFFSET;
        UBYTE caps = tf->board[TF4060_CAPS];
        /* An unimplemented capability register may read as an open bus. */
        tf->caps = (caps == 0xff) ? 0 : caps & (TF_CAP_READ | TF_CAP_FAST_TIMING);
    } else {
        if (!FindConfigDev(NULL,TF_MANUF_ID,-1)) return false;
        tf->ide = (volatile UBYTE *)BOARD_BASE;
    }

    tf->intchg = tf->ide + 0x9000;
    tf->intena = tf->ide + 0xa000;
    tf->ide[CHANNEL_1 + ata_reg_altStatus] = nIEN;
    *tf->intena &= ~0x80;
    tf->timing[0] = tf->timing[1] = 0xff;
    if (tf->caps) {
        tf->savedTiming = tf->board[TF4060_TIMING];
        tf->timingSaved = true;
    }
    Info("TF: IDE %08lx capabilities %02lx\n",(ULONG)tf->ide,(ULONG)tf->caps);
    return true;
}

/* These are controller timing levels, not ATA SET FEATURES mode numbers. */
UBYTE tf_drive_timing(const UWORD *identify, UBYTE caps) {
    if (!(caps & (TF_CAP_READ | TF_CAP_FAST_TIMING)) ||
        !(identify[ata_identify_field_valid] & ata_field_valid_pio) ||
        !(identify[ata_identify_pio_modes] & ata_pio_mode_4)) return 0;

    bool cfa = identify[0] == 0x848a ||
               (identify[ata_identify_features] & 0xc004) == 0x4004;
    if (cfa && (identify[163] & 7) > 1 && (caps & TF_CAP_FAST_TIMING)) return 2;
    return 1;
}

void tf_restore_timing(struct TFController *tf) {
    if (tf->timingSaved) tf->board[TF4060_TIMING] = tf->savedTiming;
}

void tf_apply_timing(struct TFController *tf) {
    if (!tf->timingSaved || !tf->timingReady) return;
    UBYTE level = (tf->timing[0] < tf->timing[1]) ? tf->timing[0] : tf->timing[1];
    UBYTE timing = tf->savedTiming;
    if (level == 1) timing = (timing & 0x80) | 7;
    if (level == 2) timing = (timing & 0x80) | 15;
    tf->board[TF4060_TIMING] = timing;
}

/* Issue one sector at a time, splitting the hardware word count at 64 KiB.
 * The word access after the count write serializes each mover fragment.
 */
static void tf_read_sector(struct TFController *tf, ULONG address) {
    volatile UBYTE *registers = tf->board + TF4060_READ_DEST;
    volatile UWORD *sync = (volatile UWORD *)(tf->board + TF4060_READ_SYNC);
    ULONG remaining = 256;
    while (remaining) {
        ULONG words = (0x10000UL - (address & 0xffff)) >> 1;
        if (words > remaining) words = remaining;
        registers[0] = address >> 24;
        registers[1] = address >> 16;
        registers[2] = address >> 8;
        registers[3] = address;
        registers[4] = words >> 8;
        registers[5] = words;
        (void)*sync;
        address += words << 1;
        remaining -= words;
    }
}

/* Called only after DRQ, for at most one negotiated ATA data block. */
bool tf_read_block(struct IDEUnit *unit, void *buffer, ULONG sectors) {
    struct TFController *tf = &unit->itask->tf;
    struct ExecBase *SysBase = unit->SysBase;
    ULONG address = (ULONG)buffer;
    if (!(tf->caps & TF_CAP_READ) || unit->xferMethod != tf_block_move ||
        SysBase->LibNode.lib_Version < 37 || !sectors || sectors > MAX_TRANSFER_SECTORS ||
        (address & 1) || address < TF_RAM_START || address >= TF_RAM_END) return false;

    ULONG bytes = sectors << 9;
    if (bytes > TF_RAM_END - address) return false;

    ULONG length = bytes;
    APTR physical = CachePreDMA(buffer,&length,0);
    if (physical != buffer || length < bytes) {
        length = bytes;
        CachePostDMA(buffer,&length,DMA_NoModify);
        return false;
    }

    UBYTE enabled = *tf->intena & 0x80;
    *tf->intena &= ~0x80;
    for (ULONG i=0; i<sectors; i++) tf_read_sector(tf,address + (i << 9));
    length = bytes;
    CachePostDMA(buffer,&length,0);
    if (enabled) *tf->intena |= 0x80;
    return true;
}
#endif
