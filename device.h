// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#ifndef _DEVICE_H
#define _DEVICE_H
#include <dos/filehandler.h>
#include <stdbool.h>
#define OAHR_MANUF_ID 5194
#define BSC_MANUF_ID  2092
#define A1K_MANUF_ID  2588

#define MAX_UNITS 4

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
    struct Unit io_unit;
    struct ConfigDev *cd;
    struct ExecBase *SysBase;
    struct timerequest *TimeReq;
    volatile struct Drive *drive;
    BYTE  (*write_taskfile)(struct IDEUnit *, UBYTE, ULONG, UBYTE);
    UBYTE unitNum;
    UBYTE channel;
    UBYTE device_type;
    UBYTE last_error[6];
    BOOL  primary;
    BOOL  present;
    BOOL  atapi;
    BOOL  mediumPresent;
    BOOL  mediumPresentPrev;
    BOOL  xfer_multiple;
    BOOL  lba;
    UBYTE multiple_count;
    UWORD open_count;
    UWORD change_count;
    UWORD cylinders;
    UWORD heads;
    UWORD sectorsPerTrack;
    UWORD blockSize;
    UWORD blockShift;
    ULONG logicalSectors;
    volatile UBYTE *shadowDevHead;
    struct MinList changeInts;
    volatile void  *changeInt;
};

struct DeviceBase {
    struct Library  lib;
    struct ExecBase *SysBase;
    struct Library  *ExpansionBase;
    struct Library  *TimerBase;
    struct Task     *IDETask;
    struct MsgPort  *IDETaskMP;
    struct MsgPort  *IDETimerMP;
    struct Task     *ChangeTask;
    volatile bool   IDETaskActive;
    struct          timerequest *TimeReq;
    BPTR            saved_seg_list;
    BOOL            is_open;
    UBYTE           num_boards;
    UBYTE           num_units;
    struct          IDEUnit *units;
    UBYTE           shadowDevHeads[MAX_UNITS/2];
};


#define STR(s) #s      /* Turn s into a string literal without expanding macro definitions (however, \
                          if invoked from a macro, macro arguments are expanded). */
#define XSTR(s) STR(s) /* Turn s into a string literal after macro-expanding it. */

#ifndef LIDE_IS_SCSI
#define DEVICE_NAME "    lide.device"
#else
#define DEVICE_NAME "    scsi.device"
#endif
#define DEVICE_DATE "(" __DATE__ ")"
#define DEVICE_ID_STRING "lide " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " " DEVICE_DATE /* format is: 'name version.revision (d.m.yy)' */
#define DEVICE_VERSION 40
#define DEVICE_REVISION 4
#define DEVICE_PRIORITY 0 /* Most people will not need a priority and should leave it at zero. */

#endif
