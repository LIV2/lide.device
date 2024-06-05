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

#include "flash.h"
#include "main.h"
#include "config.h"
#include "olga.h"

#define CONFIG_REG_OFFSET 0xE000
#define FW_VER_OFFSET     0xA000

#define CONFIG_FLASH_EN   0x4000

/**
 * olga_enable_flash
 * 
 * Olgas config, waitstate registers etc all overlap the IDE space
 * This will poke a config bit to hide them until reboot
 * 
*/
static void olga_enable_flash(struct ConfigDev *cd) {
    UWORD *configReg = (UWORD *)(cd->cd_BoardAddr + CONFIG_REG_OFFSET);
    
    *configReg |= CONFIG_FLASH_EN;
}

bool olga_fw_supported(struct ConfigDev *cd) {
    UWORD *fwreg = (UWORD *)(cd->cd_BoardAddr + FW_VER_OFFSET);
    UWORD version = (*fwreg) >> 12;

    return (version >= OLGA_MIN_FW_VER);
}

/**
 * setup_olga_board
 * 
 * Setup the ideBoard struct for Olga
*/
void setup_olga_board(struct ideBoard *board) {
  board->bootrom          = ATBUS;
  board->bankSelect       = NULL;
  board->flash_init       = &flash_init;
  board->flash_erase_bank = NULL;
  board->flash_erase_chip = &flash_erase_chip;
  board->flash_writeByte  = &flash_writeByte;

  board->flashbase = board->cd->cd_BoardAddr + 1; // Olga BootROM is on odd addresses

  olga_enable_flash(board->cd);
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
