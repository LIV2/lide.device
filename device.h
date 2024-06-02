// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#ifndef _DEVICE_H
#define _DEVICE_H
#include <dos/filehandler.h>
#include <exec/semaphores.h>
#include <stdbool.h>
#define OAHR_MANUF_ID 5194
#define BSC_MANUF_ID  2092
#define A1K_MANUF_ID  2588

#define RIPPLE_PROD_ID 7

#define MAX_UNITS 4

enum xfer {
    longword_movem,
    longword_move,
    longword_move16
};

/**
 * Drive struct
 * 
 * Each register spaced 512 bytes apart
 * To use this code with other boards you may need to adjust these sizes
*/
struct Drive {
    UWORD data[256];
    UBYTE error_features[512];
    UBYTE sectorCount[512];
    UBYTE lbaLow[512];
    UBYTE lbaMid[512];
    UBYTE lbaHigh[512];
    UBYTE devHead[512];
    UBYTE status_command[512];
};

struct IDEUnit {
    struct MinNode mn_Node;
    struct Unit io_unit;
    struct ConfigDev *cd;
    struct ExecBase *SysBase;
    struct IDETask *itask;
    volatile struct Drive *drive;
    BYTE  (*write_taskfile)(struct IDEUnit *, UBYTE, ULONG, UBYTE);
    enum  xfer xferMethod;
    void  (*read_fast)(void *, void *);
    void  (*write_fast)(void *, void *);
    void  (*read_unaligned)(void *, void *);
    void  (*write_unaligned)(void *, void *);
    volatile UBYTE *shadowDevHead;
    volatile void  *changeInt;
    volatile bool  deferTUR;
    UBYTE unitNum;
    UBYTE channel;
    UBYTE deviceType;
    UBYTE last_error[6];
    bool  primary;
    bool  present;
    bool  atapi;
    bool  mediumPresent;
    bool  mediumPresentPrev;
    bool  xferMultiple;
    bool  lba;
    bool  lba48;
    UWORD openCount;
    UWORD changeCount;
    UWORD heads;
    UWORD sectorsPerTrack;
    UWORD blockSize;
    UWORD blockShift;
    ULONG cylinders;
    ULONG logicalSectors;
    struct MinList changeInts;
    UBYTE multipleCount;
};

struct DeviceBase {
    struct Library         lib;
    struct ExecBase        *SysBase;
    struct Library         *ExpansionBase;
    struct Task            *ChangeTask;
    BPTR                   saved_seg_list;
    bool                   isOpen;
    ULONG                  numUnits;
    ULONG                  highestUnit;
    UBYTE                  numTasks;
    struct MinList         units;
    struct SignalSemaphore ulSem;
    struct MinList         ideTasks;
};

struct IDETask {
    struct MinNode     mn_Node;
    struct Task        *task;
    struct Task        *parent;
    struct DeviceBase  *dev;
    struct ConfigDev   *cd;
    struct MsgPort     *iomp;
    struct MsgPort     *timermp;
    struct timerequest *tr;
    volatile bool      active;
    UBYTE              shadowDevHead;
    UBYTE              boardNum;
    UBYTE              taskNum;
    UBYTE              channel;
};

#define STR(s) #s      /* Turn s into a string literal without expanding macro definitions (however, \
                          if invoked from a macro, macro arguments are expanded). */
#define XSTR(s) STR(s) /* Turn s into a string literal after macro-expanding it. */

#ifndef LIDE_IS_SCSI
#define DEVICE_NAME "    lide.device"
#else
#define DEVICE_NAME "    scsi.device"
#endif

#define DEVICE_ID_STRING "lide " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " (" XSTR(BUILD_DATE) ") " XSTR(GIT_REF)
#define DEVICE_VERSION 40
#define DEVICE_REVISION 8
#define DEVICE_PRIORITY 0 /* Most people will not need a priority and should leave it at zero. */

#endif
