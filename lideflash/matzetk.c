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
#include <stdbool.h>
#include <stdio.h>

#include "flash.h"
#include "main.h"
#include "config.h"
#include "matzetk.h"

#define CONFIG_REG_OFFSET 0xE000
#define FW_VER_OFFSET     0xA000

#define CONFIG_FLASH_EN   0x4000

/**
 * matzetk_enable_flash
 * 
 * Config, waitstate registers etc all overlap the IDE space
 * This will poke a config bit to hide them until reboot
 * 
*/
static void matzetk_enable_flash(struct ConfigDev *cd) {
    UWORD *configReg = (UWORD *)(cd->cd_BoardAddr + CONFIG_REG_OFFSET);
    
    *configReg |= CONFIG_FLASH_EN;
}

bool matzetk_fw_supported(struct ConfigDev *cd, ULONG minVersion) {
    UWORD *fwreg = (UWORD *)(cd->cd_BoardAddr + FW_VER_OFFSET);
    UWORD version = (*fwreg) >> 12;

    if (version < minVersion)
      printf("\nFirmware version %d or newer is required, please update and try again.\n\n",minVersion);

    return (version >= minVersion);
}

/**
 * setup_matzetk_board
 * 
 * Setup the ideBoard struct
*/
void setup_matzetk_board(struct ideBoard *board) {
  board->bootrom          = ATBUS;
  board->bankSelect       = NULL;
  board->flash_init       = &flash_init;
  board->flash_erase_bank = NULL;
  board->flash_erase_chip = &flash_erase_chip;
  board->flash_writeByte  = &flash_writeByte;

  board->flashbase = board->cd->cd_BoardAddr + 1; // Olga BootROM is on odd addresses

  matzetk_enable_flash(board->cd);
}

/**
 * find_olga
 * 
 * Returns true if Olga is present
*/
bool find_olga() {
  struct ConfigDev *cd;
    if ((cd = FindConfigDev(NULL,MANUF_ID_A1K,PROD_ID_OLGA)) != NULL) {
      return true;
    } else {
      return false;
    }
}

/**
 * find_68ec020_tk
 * 
 * Returns true if 68EC020-TK is present
*/
bool find_68ec020_tk() {
  struct ConfigDev *cd;
    if ((cd = FindConfigDev(NULL,MANUF_ID_A1K,PROD_ID_68EC020_TK)) != NULL) {
      return true;
    } else {
      return false;
    }
}
