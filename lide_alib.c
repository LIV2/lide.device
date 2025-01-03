// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#include <exec/types.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <proto/exec.h>
#include "debug.h"
/*
    This file provides functions normally provided by amiga.lib
    amiga.lib requires a global SysBase variable but to make the driver ROMable we need to avoid that
*/


/**
 * L_NewList
 * 
 * Initialize a new list
 * 
 * @param new_list Pointer to a new list
 */
void L_NewList(struct List *new_list) {
    new_list->lh_Head = (struct Node *)&new_list->lh_Tail;
    new_list->lh_Tail = 0;
    new_list->lh_TailPred = (struct Node *)new_list;
}

/**
 * L_CreatePort
 * 
 * Create a new MsgPort
 * 
 * @param name (optional) name of the port
 * @param priority priority of the port
 * @returns pointer to a MsgPort
 */
struct MsgPort *L_CreatePort(STRPTR name, LONG pri) {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct MsgPort *mp = NULL;
    BYTE sigNum;
    if ((sigNum = AllocSignal(-1)) >= 0) {
        if ((mp = AllocMem(sizeof(struct MsgPort),MEMF_CLEAR|MEMF_PUBLIC))) {
            mp->mp_Node.ln_Type = NT_MSGPORT;
            mp->mp_Node.ln_Pri  = pri;
            mp->mp_Node.ln_Name = name;
            mp->mp_SigBit       = sigNum;
            mp->mp_SigTask      = FindTask(0);
            
            L_NewList(&mp->mp_MsgList);

            if (mp->mp_Node.ln_Name)
                AddPort(mp);
        }
    }
    return mp;

}

/**
 * L_DeletePort
 * 
 * Delete a MsgPort
 * 
 * @param mp Pointer to a MsgPort
 */
void L_DeletePort(struct MsgPort *mp) {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;

    if (mp) {
        if (mp->mp_Node.ln_Name)
            RemPort(mp);
        
        FreeSignal(mp->mp_SigBit);
        FreeMem(mp,sizeof(struct MsgPort));
    }
}

/**
 * L_CreateExtIO
 * 
 * Create an Extended IO Request
 * 
 * @param mp Pointer to the reply port
 * @param size Size of the IO request
 * @return pointer to the IO request
 */
struct IORequest* L_CreateExtIO(struct MsgPort *mp, ULONG size) {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct IORequest *ior = NULL;

    if (mp) {
        if ((ior=AllocMem(size,MEMF_PUBLIC|MEMF_CLEAR))) {
            ior->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
            ior->io_Message.mn_Length       = size;
            ior->io_Message.mn_ReplyPort    = mp;
        }
    }

    return ior;
}

/**
 * L_CreateStdIO
 * 
 * Create a standard IO Request
 * 
 * @param mp Pointer to the reply port
 * @return Pointer to an IOStdReq
 */
struct IOStdReq* L_CreateStdIO(struct MsgPort *mp) {
    return (struct IOStdReq *)L_CreateExtIO(mp,sizeof(struct IOStdReq));
}

/**
 * L_DeleteExtIO
 * 
 * Delete an Extended IO Request
 * 
 * @param ior Pointer to an IORequest
 */
void L_DeleteExtIO(struct IORequest *ior) {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    if (ior) {
        ior->io_Device = (APTR)-1;
        ior->io_Unit   = (APTR)-1;
        FreeMem(ior,ior->io_Message.mn_Length);
    }
}

/**
 * L_DeleteStdIO
 * 
 * Delete a Standard IO Requesrt
 * 
 * @param ior Pointer to an IOStdReq
 */
void L_DeleteStdIO(struct IOStdReq *ior) {
    L_DeleteExtIO((struct IORequest *)ior);
}
