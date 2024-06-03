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

enum BOOTROM {
  CIDER,
  ATBUS
};
struct ideBoard {
  struct ConfigDev *cd;
  enum BOOTROM bootrom;
  void *flashbase;
  bool (*flash_init)(UBYTE *, UBYTE *, ULONG *);
  void (*flash_erase_chip)();
  void (*flash_erase_bank)();
  void (*flash_writeByte)(ULONG, UBYTE);
  void (*bankSelect)(UBYTE, UBYTE *);
};

ULONG getFileSize(char *);
BOOL readFileToBuf(char *, void *);
BOOL writeBufToFlash(struct ideBoard *board, UBYTE *source, UBYTE *dest, ULONG size);

#endif
