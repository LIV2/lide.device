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

#include <stdbool.h>
#include <proto/exec.h>
#include <stdio.h>

#include "main.h"
#include "config.h"

/** configure
 *
 * @brief Parse the command arguments and return the config
 * @param argc Arg count
 * @param argv Argument variables
 * @returns Pointer to a Config struct or NULL on error
*/
struct Config* configure(int argc, char *argv[]) {

  bool error = false;

  struct Config *config;
  config = (struct Config *)AllocMem(sizeof(struct Config),MEMF_CLEAR);
  
  if (config == NULL) return NULL;

  config->cdfs_filename    = NULL;
  config->eraseFlash       = false;

  for (int i=1; i<argc; i++) {
    if (argv[i][0] == '-') {
      switch(argv[i][1]) {
        case 'I':
          if (i+1 < argc) {
            config->ide_rom_filename = argv[i+1];
            i++;
          }
          break;

        case 'C':
          if (i+1 < argc) {
            config->cdfs_filename = argv[i+1];
            i++;
          }
          break;

        case 'E':
          config->eraseFlash = true;
          break;
      }
    }
  }

 if (config->ide_rom_filename == NULL && config->cdfs_filename == NULL && config->eraseFlash == false) {
      error = true;
  }

  if (error) {
    FreeMem(config,sizeof(struct Config));
    return (NULL);
  } else {
    return (config);
  }
}

/** usage
 * @brief Print the usage information
*/
void usage() {
    printf("\nUsage: lideflash [-I <ide rom>]\n\n");
    printf("       -I <ide rom> - Flash IDE ROM.\n");
    printf("       -C <CDFileSystem> - Flash CDFilesystem to ROM.\n");
    printf("       -E Erase flash.\n");

}
