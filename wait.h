// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#ifndef _WAIT_H
#define _WAIT_H

#include <devices/timer.h>
#include <exec/types.h>
#include <proto/exec.h>

static inline void wait_s(struct timerequest *tr, ULONG seconds) {
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_sec     = seconds;
    tr->tr_time.tv_micro   = 0;
    DoIO((struct IORequest *)tr);
}

static inline void wait_us(struct timerequest *tr, ULONG micros) {
    tr->tr_node.io_Command = TR_ADDREQUEST;
    tr->tr_time.tv_sec     = 0;
    tr->tr_time.tv_micro   = micros;
    DoIO((struct IORequest *)tr);
}

#endif