// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of loadlide
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

#include <proto/exec.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <exec/resident.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

int main(int argc, char *argv[]) {
  struct ExecBase *SysBase = *(struct ExecBase **)4UL;
  struct Library *DOSBase = OpenLibrary("dos.library",0);

  const char fn[] = "lide.device";

  char *filename = (char *)fn;

  if (argc == 2) {
    filename = argv[1];
  }

  bool foundResident = false;
  int rc = 0;

  if (DOSBase) {
    BPTR segList = LoadSeg(filename);
    if (segList) {
      UWORD *ptr = (void *)BADDR(segList);
      for (int i=0; i<4096; i++) {
        if (*ptr == 0x4AFC) {
          foundResident = true;
          InitResident((struct Resident *)ptr,segList);
          break;
        }
        ptr++;
      }
      if (!foundResident) {
        rc = 10;
        printf("Resident not found.\n");
      }
    } else {
      printf("Failed to load. check path and try again\n");
    }

    CloseLibrary(DOSBase);
  }

  return rc;
}
