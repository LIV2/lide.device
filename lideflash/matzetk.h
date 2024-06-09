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

#ifndef MATZETK_H
#define MATZETK_H

#define OLGA_MIN_FW_VER 9
#define TK020_MIN_FW_VER 4

#define MANUF_ID_A1K  0x0A1C

#define PROD_ID_OLGA       0xD0
#define PROD_ID_MATZE_IDE  0x7D
#define PROD_ID_68EC020_TK 0x08

#define SERIAL_MATZE 0xB16B00B5

bool matzetk_fw_supported(struct ConfigDev *, ULONG);
void setup_matzetk_board(struct ideBoard *);
bool find_olga();
bool find_68ec020_tk();

#endif