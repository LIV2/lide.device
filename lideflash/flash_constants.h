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
#ifndef FLASH_CONSTANTS_H
#define FLASH_CONSTANTS_H

// SST 39SF010
#define FLASH_MANUF      0xBF
#define FLASH_DEV        0xB5
#define FLASH_SIZE       0x8000 // Actually a 128K ROM but only 32K mapped
#define FLASH_SECTORSIZE 0x2000

// Command addresses left-shifted because Flash A0 = CPU A1
#define ADDR_CMD_STEP_1  (0x5555 << 1)
#define ADDR_CMD_STEP_2  (0x2AAA << 1)

#define CMD_SDP_STEP_1   0xAA
#define CMD_SDP_STEP_2   0x55
#define CMD_BYTE_PROGRAM 0xA0
#define CMD_ERASE        0x80
#define CMD_ERASE_SECTOR 0x30
#define CMD_ERASE_CHIP   0x10
#define CMD_ID_ENTRY     0x90
#define CMD_CFI_ID_EXIT  0xF0
#define CMD_READ_RESET   0xF0


#endif