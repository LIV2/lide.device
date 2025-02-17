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

volatile ULONG flashbase;

static inline void flash_command(UBYTE);
static inline void flash_poll(ULONG);

static const LONG devices_supported[] = {
    0x1F03, // AT49BV512
    0xBFB5, // SST39SF010
    0xBFB6, // SST39SF020
    0xBFB7, // SST39SF040
    0xBFD5, // SST39(L/V)F010
    0xBFD6, // SST39(L/V)F020
    0xBFD7, // SST39(L/V)F040
    0xC2A4, // MX29F040C
    0x0120, // AM29F010
    0x01A4, // AM29F040
    0
};

/** flash_is_supported
 *
 * @brief Check if the device id is supported
 * @param manufacturer the manufacturer ID
 * @param device the device id
 * @returns boolean result
*/
static bool flash_is_supported(UBYTE manufacturer, UBYTE device) {
  ULONG deviceId = (manufacturer << 8) | device;
  int i=0;

  while (devices_supported[i] != 0) {
    if (devices_supported[i] == deviceId)
      return true;

    i++;
  }

  return false;
}

/** flash_get_sectorSize
 * 
 * @brief return the sector size for this device type if known
 * @param manufacturer the manufacturer ID
 * @param device the device id
 * @returns UWORD sector size in bytes
 */
static UWORD flash_get_sectorSize(UBYTE manufacturer, UBYTE device) {
    ULONG deviceId = (manufacturer << 8) | device;
    UWORD sectorSize;

    switch (deviceId) {
      case 0xBFB5: // SST39SF010
      case 0xBFB6: // SST39SF020
      case 0xBFB7: // SST39SF040
      case 0xBFD5: // SST39(L/V)F010
      case 0xBFD6: // SST39(L/V)F020
      case 0xBFD7: // SST39(L/V)F040
        sectorSize = 4096;
        break;
      case 0x0120: // AM29F010
        sectorSize = 16384;
        break;
      default:     
        // Unknown/Unsupported/Too large
        // If the device's sectorSize is greater than 32K don't bother
        sectorSize = 0;
    }

    return sectorSize;
}

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
  *(volatile UBYTE *)(flashbase + address) = data;
  flash_poll(address);

  return;
}

/** flash_command
 *
 * @brief send a command to the Flash
 * @param command
*/
static inline void flash_command(UBYTE command) {
  *(volatile UBYTE *)(flashbase + ADDR_CMD_STEP_1) = command;

  return;
}

/** flash_unlock_sdp
 *
 * @brief Send the SDP command sequence
*/
void flash_unlock_sdp() {
  *(volatile UBYTE *)(flashbase + ADDR_CMD_STEP_1) = CMD_SDP_STEP_1;
  *(volatile UBYTE *)(flashbase + ADDR_CMD_STEP_2) = CMD_SDP_STEP_2;

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


/** flash_erase_bank
 *
 * Erase the currently selected 32KB bank
 *
*/
void flash_erase_bank(UWORD sectorSize) {
  if (sectorSize > 0) {
    int count = 32768 / sectorSize;
    for (int i=0; i < count; i++) {
      flash_erase_sector(i * sectorSize);
    }
  }
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
  *(volatile UBYTE *)(flashbase + address) = CMD_ERASE_SECTOR;
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
  volatile UBYTE *read1 = ((void *)flashbase + address);
  volatile UBYTE *read2 = ((void *)flashbase + address);
  while (((*read1 & 1<<6) != (*read2 & 1<<6))) {;;}
}

/** flash_init
 *
 * @brief Check the manufacturer id of the device, return manuf and dev id
 * @param manuf Pointer to a UBYTE that will be updated with the returned manufacturer id
 * @param devid Pointer to a UBYTE that will be updatet with the returned device id
 * @param flashbase Pointer to the Flash base address
 * @return True if the manufacturer ID matches the expected value
*/
bool flash_init(UBYTE *manuf, UBYTE *devid, ULONG *base, UWORD *sectorSize) {
  bool ret = false;
  UBYTE manufId;
  UBYTE deviceId;

  flashbase = (ULONG)base;

  flash_unlock_sdp();
  flash_command(CMD_ID_ENTRY);

  manufId  = *(volatile UBYTE *)flashbase;
  deviceId = *(volatile UBYTE *)(flashbase + 2);

  flash_command(CMD_CFI_ID_EXIT);

  if (manuf) *manuf = manufId;
  if (devid) *devid = deviceId;


  if (flash_is_supported(manufId,deviceId) && flashbase) {
    ret = true;
  }

  if (sectorSize) *sectorSize = flash_get_sectorSize(manufId,deviceId);

  return (ret);
}