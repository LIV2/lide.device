// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#define DBG_INFO  1
#define DBG_WARN  2
#define DBG_TRACE 4
#define DBG_CMD   8
#define DBG_MEM   16

#if DEBUG
#include <clib/debug_protos.h>
#endif

#if DEBUG & DBG_MEM
void * DebugAllocMem(char *file, int line, ULONG byteSize, ULONG attributes);
#undef AllocMem
#define AllocMem(x,y) DebugAllocMem(__FILE__,__LINE__,x,y)

void DebugFreeMem(char *file, int line, void *memBlock, ULONG byteSize);
#undef FreeMem
#define FreeMem(x,y) DebugFreeMem(__FILE__,__LINE__,x,y)
#endif

#if DEBUG & DBG_INFO
#define Info KPrintF
#else
#define Info
#endif

#if DEBUG & DBG_WARN
#define Warn KPrintF
#else
#define Warn
#endif

#if DEBUG & DBG_TRACE
#define Trace KPrintF
#else
#define Trace
#endif

#if DEBUG & DBG_CMD
void traceCommand(struct IOStdReq *req);
#else
#define traceCommand
#endif