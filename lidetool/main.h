// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lidetool
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

#define STR(s) #s      /* Turn s into a string literal without expanding macro definitions (however, \
                          if invoked from a macro, macro arguments are expanded). */
#define XSTR(s) STR(s) /* Turn s into a string literal after macro-expanding it. */

#define VERSION_STRING "$VER: lidetool " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " (" XSTR(BUILD_DATE) ") " XSTR(GIT_REF)
struct __attribute__((packed)) SCSI_Inquiry {
    UBYTE peripheral_type;
    UBYTE removable_media;
    UBYTE version;
    UBYTE response_format;
    UBYTE additional_length;
    UBYTE flags[3];
    UBYTE vendor[8];
    UBYTE product[16];
    UBYTE revision[4];
    UBYTE serial[8];
};

struct __attribute__((packed)) SCSI_CDB_10 {
    UBYTE operation;
    UBYTE flags;
    ULONG lba;
    UBYTE group;
    UWORD length;
    UBYTE control;
};

#define SZ_CDB_10 10
#define SZ_CDB_12 12
#define SCSI_CMD_INQUIRY 0x12
#define SCSI_CMD_ATA_PASSTHROUGH 0xA1

#define CMD_XFER 0x1001
#define CMD_PIO  (CMD_XFER + 1)


#endif
