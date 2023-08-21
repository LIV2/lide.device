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

#include <proto/expansion.h>
#include <exec/types.h>
#include <stdbool.h>

#include "flash.h"
#include "flash_constants.h"

ULONG ide_flashBase;

static inline void flash_command(UBYTE);
static inline void flash_poll(ULONG);

/** flash_writeByte
 *
 * @brief Write a byte to the Flash
 * @param address Address to write to
 * @param data The data to be written
*/
void flash_writeByte(ULONG address, UBYTE data) {
  address &= (FLASH_SIZE-1);
  address <<= 1;
  flash_unlock_sdp();
  flash_command(CMD_BYTE_PROGRAM);
  *(volatile UBYTE *)(ide_flashBase + address) = data;
  flash_poll(address);

  return;
}

/** flash_command
 *
 * @brief send a command to the Flash
 * @param command
*/
static inline void flash_command(UBYTE command) {
  *(volatile UBYTE *)(ide_flashBase + ADDR_CMD_STEP_1) = command;

  return;
}

/** flash_unlock_sdp
 *
 * @brief Send the SDP command sequence
*/
void flash_unlock_sdp() {
  *(volatile UBYTE *)(ide_flashBase + ADDR_CMD_STEP_1) = CMD_SDP_STEP_1;
  *(volatile UBYTE *)(ide_flashBase + ADDR_CMD_STEP_2) = CMD_SDP_STEP_2;

  return;
}

/** flash_erase_chip
 *
 * @brief Perform a chip erase
*/
void flash_erase_chip() {
  flash_unlock_sdp();
  flash_command(CMD_ERASE);
  flash_unlock_sdp();
  flash_command(CMD_ERASE_CHIP);

  flash_poll(0);
}

/** flash_erase_sector
 * 
 * @brief Erase a sector
 * @param address Address of sector to erase
 * 
*/
void flash_erase_sector(ULONG address) {
  address &= (FLASH_SIZE-1);
  address <<= 1;
  flash_unlock_sdp();
  flash_command(CMD_ERASE);
  flash_unlock_sdp();
  *(volatile UBYTE *)(ide_flashBase + address) = 0x30;
  flash_poll(address);
}

/** flash_poll
 *
 * @brief Poll the status bits at address, until they indicate that the operation has completed.
 * @param address Address to poll
*/
static inline void flash_poll(ULONG address) {
  address &= (FLASH_SIZE-1);
  address <<= 1;
  volatile UBYTE *read1 = ((void *)ide_flashBase + address);
  volatile UBYTE *read2 = ((void *)ide_flashBase + address);
  while (((*read1 & 1<<6) != (*read2 & 1<<6))) {;;}
}

/** flash_init
 *
 * @brief Check the manufacturer id of the device, return manuf and dev id
 * @param manuf Pointer to a UBYTE that will be updated with the returned manufacturer id
 * @param devid Pointer to a UBYTE that will be updatet with the returned device id
 * @param ide_flashBase Pointer to the Flash base address
 * @return True if the manufacturer ID matches the expected value
*/
bool flash_init(UBYTE *manuf, UBYTE *devid, ULONG *base) {
  bool ret = false;
  UBYTE manufId;
  UBYTE deviceId;
  
  ide_flashBase = (ULONG)base;

  flash_unlock_sdp();
  flash_command(CMD_ID_ENTRY);

  manufId  = *(volatile UBYTE *)ide_flashBase;
  deviceId = *(volatile UBYTE *)(ide_flashBase + 2);

  flash_command(CMD_CFI_ID_EXIT);

  if (manuf) *manuf = manufId;
  if (devid) *devid = deviceId;

  if (manufId == FLASH_MANUF && deviceId == FLASH_DEV && ide_flashBase) {
    ret = true;
  }

  return (ret);
}