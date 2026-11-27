// SPDX-License-Identifier: GPL-2.0-only
#ifndef _TFIDE_H
#define _TFIDE_H

#include <exec/types.h>
#include <stdbool.h>

#define TF_MANUF_ID          5080
#define TF4060_PROD_ID       0x94
#define TF4060_IDE_OFFSET    0x4000
#define TF4060_CAPS          0x005c
#define TF4060_TIMING        0x0068
#define TF4060_READ_DEST     0x00b4
#define TF4060_READ_WORDS    0x00b8
#define TF4060_READ_SYNC     0x6000
#define TF_CAP_READ          0x01
#define TF_CAP_FAST_TIMING   0x02
#define TF_RAM_START         0x08000000UL
#define TF_RAM_END           0x18000000UL

struct TFController {
    volatile UBYTE *ide;
    volatile UBYTE *board;
    volatile UBYTE *intchg;
    volatile UBYTE *intena;
    UBYTE caps;
    UBYTE savedTiming;
    UBYTE timing[2];
    UBYTE maxTiming[2];
    bool timingSaved;
    bool timingReady;
};

struct Library;
struct IDEUnit;
struct IOTask;

bool tf_ide_init(struct Library *ExpansionBase, struct TFController *tf);
UBYTE tf_drive_timing(const UWORD *identify, UBYTE caps);
void tf_apply_timing(struct TFController *tf);
void tf_restore_timing(struct TFController *tf);
bool tf_read_block(struct IDEUnit *unit, void *buffer, ULONG sectors);

#endif
