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
void matzetk_enable_flash(struct ideBoard *board) {
    UWORD *configReg = (UWORD *)(board->cd->cd_BoardAddr + CONFIG_REG_OFFSET);

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
  board->writeEnable      = &matzetk_enable_flash;
  board->rebootRequired   = true; // write enable turns off IDE so we must reboot afterwards


  board->flashbase = board->cd->cd_BoardAddr + 1; // Olga BootROM is on odd addresses
}

/**
 * boardIsOlga
 *
 * Returns true if the board is a Dicke Olga
*/
bool boardIsOlga(struct ConfigDev *cd) {
  struct ConfigDev *prevCd = (struct ConfigDev *)cd->cd_Node.ln_Pred;

  if (prevCd->cd_Rom.er_Manufacturer == MANUF_ID_A1K &&
      prevCd->cd_Rom.er_Product == PROD_ID_OLGA)

      return true;

  return false;
}

/**
 * boardIs68ec020tk
 *
 * Returns true the board is a 68EC020-TK
*/
bool boardIs68ec020tk(struct ConfigDev *cd) {
  struct ConfigDev *prevCd = (struct ConfigDev *)cd->cd_Node.ln_Pred;

  if (prevCd->cd_Rom.er_Manufacturer == MANUF_ID_A1K &&
      (prevCd->cd_Rom.er_Product == PROD_ID_68EC020_TK_1 ||
      (prevCd->cd_Rom.er_Product == PROD_ID_68EC020_TK_2)))

      return true;

  return false;
}
