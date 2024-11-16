// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lideflash
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef MAIN_H
#define MAIN_H

// Bank Sel register of RIPPLE
#define BANK_SEL_REG 0x8000

#undef FindConfigDev
// NDK 1.3 definition of FindConfigDev is incorrect which causes "makes pointer from integer without a cast" warning
struct ConfigDev* FindConfigDev(struct ConfigDev*, LONG, LONG);

// NDK 1.3 lacks ColdReboot, so define it
#ifndef ColdReboot
void ColdReboot();
#endif

enum BOOTROM {
  CIDER,
  ATBUS
};

struct ideBoard {
  struct ConfigDev *cd;
  enum BOOTROM bootrom;
  void *flashbase;
  bool rebootRequired;
  bool cdfsSupported;
  void (*bankSelect)(UBYTE, struct ideBoard *);
  void (*writeEnable)(struct ideBoard *);
};

ULONG getFileSize(char *);
BOOL readFileToBuf(char *, void *);
BOOL writeBufToFlash(struct ideBoard *board, UBYTE *source, UBYTE *dest, ULONG size);

#endif
