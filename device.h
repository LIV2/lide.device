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

// VSCode C/C++ extension doesn't like the asm("<reg>") syntax
#ifdef __INTELLISENSE__
#define asm(x)
#endif

enum xfer {
    longword_movem,
    longword_move
};

/**
 * Drive struct
 *
*/
struct Drive {
    volatile UWORD *data;
    volatile UBYTE *error_features;
    volatile UBYTE *sectorCount;
    volatile UBYTE *lbaLow;
    volatile UBYTE *lbaMid;
    volatile UBYTE *lbaHigh;
    volatile UBYTE *devHead;
    volatile UBYTE *status_command;
};

struct IDEUnit {
    struct MinNode mn_Node;
    struct Unit io_unit;
    struct ConfigDev *cd;
    struct ExecBase *SysBase;
    struct IDETask *itask;
    struct Drive drive;
    BYTE  (*write_taskfile)(struct IDEUnit *, UBYTE, ULONG, UBYTE, UBYTE);
    enum  xfer xferMethod;
    void  (*read_fast)(void * asm("a0"), void * asm("a1"));
    void  (*write_fast)(void * asm("a0"), void * asm("a1"));
    void  (*read_unaligned)(void * asm("a0"), void * asm("a1"));
    void  (*write_unaligned)(void * asm("a0"), void * asm("a1"));
    volatile UBYTE *shadowDevHead;
    volatile void  *changeInt;
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
    struct ExecBase    *SysBase;
    struct DeviceBase  *dev;
    struct ConfigDev   *cd;
    struct MsgPort     *iomp;
    struct MsgPort     *timermp;
    struct MsgPort     *dcTimerMp;
    struct timerequest *tr;
    struct timerequest *dcTimerReq;
    volatile bool      active;
    bool               hasRemovables;
    bool               dcTimerArmed;
    UBYTE              shadowDevHead;
    UBYTE              boardNum;
    UBYTE              taskNum;
    UBYTE              channel;
};

#define STR(s) #s      /* Turn s into a string literal without expanding macro definitions (however, \
                          if invoked from a macro, macro arguments are expanded). */
#define XSTR(s) STR(s) /* Turn s into a string literal after macro-expanding it. */

#ifndef LIDE_IS_SCSI
#define DEVICE_NAME "lide.device"
#else
#define DEVICE_NAME "scsi.device"
#endif

#define DEVICE_ID_STRING "lide " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " (" XSTR(BUILD_DATE) ") " XSTR(GIT_REF)

// The build process will define the version/revision based on the latest git tag
#ifndef DEVICE_VERSION
#define DEVICE_VERSION 40
#define DEVICE_REVISION 0
#endif

#define DEVICE_PRIORITY 10

#endif
