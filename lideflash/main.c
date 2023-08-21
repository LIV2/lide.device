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

#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <proto/dos.h>
#include <dos/dos.h>

#include "flash.h"
#include "main.h"
#include "config.h"

#define MANUF_ID_BSC 0x082C
#define MANUF_ID_OAHR 5194

#define ROMSIZE 32768

struct Library *DosBase;
struct ExecBase *SysBase;
struct ExpansionBase *ExpansionBase = NULL;

int main(int argc, char *argv[])
{
  SysBase = *((struct ExecBase **)4UL);
  DosBase = OpenLibrary("dos.library",0);

  int rc = 0;
  int boards_found = 0;

  void *buffer = NULL;
  ULONG romSize    = 0;

  if (DosBase == NULL) {
    return(rc);
  }

  printf("LIV2 IDE Updater\n");

  struct Config *config;
  struct Task *task = FindTask(0);
  SetTaskPri(task,20);
  if ((config = configure(argc,argv)) != NULL) {

    romSize = getFileSize(config->ide_rom_filename);
    if (romSize == 0) {
      rc = 5;
      goto exit;
    }

    if (romSize > 32768) {
      printf("ROM file too large.\n");
      rc = 5;
      goto exit;
    }

    buffer = AllocMem(romSize,MEMF_ANY|MEMF_CLEAR);
    if (buffer) {
      if (readFileToBuf(config->ide_rom_filename,buffer) == false) {
        rc = 5;
        goto exit;
      }
    } else {
      printf("Couldn't allocate memory.\n");
      rc = 5;
      goto exit;
    }

    if ((ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library",0)) != NULL) {

      void *flashbase = NULL;
      struct ConfigDev *cd = NULL;
      
      while ((cd = FindConfigDev(cd,-1,-1)) != NULL) {

        switch (cd->cd_Rom.er_Manufacturer) {
          case MANUF_ID_OAHR:
            if (cd->cd_Rom.er_Product == 5) { // CIDER IDE
              printf("Found CIDER IDE");
              break;
            } else if (cd->cd_Rom.er_Product == 7) { // RIPPLE
              printf("Found RIPPLE IDE");
              break;
            } else {
              continue; // Skip this board
            }
          
          case MANUF_ID_BSC:
            if (cd->cd_Rom.er_Product == 6 && cd->cd_Rom.er_SerialNumber == 0x4C495632) { // Is it a LIV2 board pretending to be an AT-Bus 2008?
              printf("Found Unknown LIV2 IDE board");
              break;
            } else {
              continue; // Skip this board
            }

          default:
            continue; // Skip this board
        }

        printf(" at Address 0x%06x\n",(int)cd->cd_BoardAddr);
        boards_found++;

        flashbase = cd->cd_BoardAddr;

        if (cd->cd_BoardSize > 65536) {
          // Newer version of the CIDER firmware give the IDE device a 128K block, with the top 64K dedicated to the Flash
          // This means we can flash the IDE ROM without having to disable IDE
          flashbase += 65536;
        }

        UBYTE manufId,devId;
        if (flash_init(&manufId,&devId,(ULONG *)flashbase)) {

          UBYTE *sourcePtr = NULL;
          UBYTE *destPtr = cd->cd_BoardAddr;

          printf("Erasing IDE Flash...\n");
          flash_erase_chip();
          
          int progress = 0;
          int lastProgress = 1;

          fprintf(stdout,"Writing IDE ROM:     ");
          fflush(stdout);

          for (int i=0; i<romSize; i++) {

            progress = (i*100)/(romSize-1);

            if (lastProgress != progress) {
              fprintf(stdout,"\b\b\b\b%3d%%",progress);
              fflush(stdout);
              lastProgress = progress;
            }
            sourcePtr = ((void *)buffer + i);
            flash_writeByte(i,*sourcePtr);

          }

          fprintf(stdout,"\n");
          fflush(stdout);

          fprintf(stdout,"Verifying IDE ROM:     ");
          for (int i=0; i<romSize; i++) {

            progress = (i*100)/(romSize-1);

            if (lastProgress != progress) {
              fprintf(stdout,"\b\b\b\b%3d%%",progress);
              fflush(stdout);
              lastProgress = progress;
            }
            sourcePtr = ((void *)buffer + i);
            destPtr = ((void *)flashbase + (i << 1));
            if (*sourcePtr != *destPtr) {
                  printf("\nVerification failed at %06x - Expected %02X but read %02X\n",(int)destPtr,*sourcePtr,*destPtr);
                  rc = 5;
                  goto exit;
                }

          }

          fprintf(stdout,"\n");
          fflush(stdout);

        } else {
          printf("Error: IDE - Unknown Flash device Manufacturer: %02X Device: %02X\n", manufId, devId);
          if (cd->cd_BoardSize == 65535) {
            printf("Turn IDE off and try again.\n");
          }
          rc = 5;
        }
      }
    
      if (boards_found == 0) {
        printf("No IDE board(s) found\n");
      }
    } else {
      printf("Couldn't open Expansion.library.\n");
      rc = 5;
    }

  } else {
    usage();
  }

exit:

  if (buffer)        FreeMem(buffer,romSize);
  if (config)        FreeMem(config,sizeof(struct Config));
  if (ExpansionBase) CloseLibrary((struct Library *)ExpansionBase);
  if (DosBase)       CloseLibrary((struct Library *)DosBase);
  return (rc);
}

/**
 * getFileSize
 *
 * @brief return the size of a file in bytes
 * @param filename file to check the size of
 * @returns File size in bytes
*/
ULONG getFileSize(char *filename) {
  BPTR fileLock;
  ULONG fileSize = 0;
  struct FileInfoBlock *FIB;

  FIB = (struct FileInfoBlock *)AllocMem(sizeof(struct FileInfoBlock),MEMF_CLEAR);

  if ((fileLock = Lock(filename,ACCESS_READ)) != 0) {

    if (Examine(fileLock,FIB)) {
      fileSize = FIB->fib_Size;
    }

  } else {
    printf("Error opening %s\n",filename);
    fileSize = 0;
  }

  if (fileLock) UnLock(fileLock);
  if (FIB) FreeMem(FIB,sizeof(struct FileInfoBlock));

  return (fileSize);
}

/**
 * readFileToBuF
 *
 * @brief Read the rom file to a buffer
 * @param filename Name of the file to open
 * @return true on success
*/
BOOL readFileToBuf(char *filename, void *buffer) {
  ULONG romSize = getFileSize(filename);
  BOOL ret = true;

  if (romSize == 0) return false;

  BPTR fh;

  if (buffer) {
    fh = Open(filename,MODE_OLDFILE);

    if (fh) {
      Read(fh,buffer,romSize);
      Close(fh);
    } else {
      printf("Error opening %s\n",filename);
      return false;
    }

  } else {
    return false;
  }

  return ret;
}
