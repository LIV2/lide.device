// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#define DBG_INFO  1
#define DBG_WARN  2
#define DBG_TRACE 3

#if DEBUG
#include <clib/debug_protos.h>
#define Info KPrintF
#else
#define Info
#endif

#if DEBUG >= DBG_WARN
#define Warn KPrintF
#else
#define Warn
#endif

#if DEBUG >= DBG_TRACE
#define Trace KPrintF
#else
#define Trace
#endif
