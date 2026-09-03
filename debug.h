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
#include <exec/types.h>
#endif

#if DEBUG & DBG_MEM
void * DebugAllocMem(char *file, int line, ULONG byteSize, ULONG attributes, struct ExecBase *SysBase asm("a6"));
#undef AllocMem
#define AllocMem(x,y) DebugAllocMem(__FILE__,__LINE__,x,y,SysBase)

void DebugFreeMem(char *file, int line, void *memBlock, ULONG byteSize, struct ExecBase *SysBase asm("a6"));
#undef FreeMem
#define FreeMem(x,y) DebugFreeMem(__FILE__,__LINE__,x,y,SysBase)
#endif

#if DEBUG & DBG_INFO
#define Info(...) KPrintF(__VA_ARGS__)
#else
#define Info(...) ((void)0)
#endif

#if DEBUG & DBG_WARN
#define Warn(...) KPrintF(__VA_ARGS__)
#else
#define Warn(...) ((void)0)
#endif

#if DEBUG & DBG_TRACE
#define Trace(...) KPrintF(__VA_ARGS__)
#else
#define Trace(...) ((void)0)
#endif

#if DEBUG & DBG_CMD
void traceCommand(struct IOStdReq *req);
#else
#define traceCommand(...) ((void)0)
#endif