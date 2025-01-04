// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#ifndef _LIDE_ALIB_H
#define _LIDE_ALIB_H

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <proto/exec.h>

void L_NewList(struct List *new_list);
struct MsgPort *L_CreatePort(STRPTR name, LONG pri);
void L_DeletePort(struct MsgPort *mp);
struct IORequest* L_CreateExtIO(struct MsgPort *mp, ULONG size);
struct IOStdReq* L_CreateStdIO(struct MsgPort *mp);
void L_DeleteExtIO(struct IORequest *ior);
void L_DeleteStdIO(struct IOStdReq *ior);
struct Task *L_CreateTask(char * taskName, LONG priority, APTR funcEntry, ULONG stackSize, APTR userData);
#endif