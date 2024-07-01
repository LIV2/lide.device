// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#define ATA_TASK_NAME    "lide ata task"
#define CHANGE_TASK_NAME "lide change task"
#define TASK_PRIORITY 11
#define TASK_STACK_SIZE 8192

#define CHANGEINT_INTERVAL 2 // Poll units every x seconds for disk change

#define CMD_DIE  0x1000
#define CMD_XFER (CMD_DIE + 1)

struct IDETask {
    struct MinNode     mn_Node;
    struct Task        *task;
    struct Task        *parent;
    struct DeviceBase  *dev;
    struct ConfigDev   *cd;
    struct MsgPort     *iomp;
    struct MsgPort     *timermp;
    struct timerequest *tr;
    UBYTE              shadowDevHead;
    UBYTE              boardNum;
    UBYTE              taskNum;
    UBYTE              channel;
    BYTE               irqSignal;
    volatile bool      active;
};

struct INTData {
    struct ExecBase *SysBase;
    struct Task     *Task;
    APTR            intReg;
    APTR            driveStatus;
    ULONG           signalMask;
    UBYTE           intmask;
};

void ide_task();
void diskchange_task();
BYTE direct_changestate(struct IDEUnit *unit, struct DeviceBase *dev);