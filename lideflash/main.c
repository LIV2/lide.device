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

#define MANUF_ID_A1K  0x0A1C
#define MANUF_ID_BSC  0x082C
#define MANUF_ID_OAHR 5194

#define PROD_ID_CIDER  0x05
#define PROD_ID_RIPPLE 0x07
#define PROD_ID_OLGA   0xD0

#define SERIAL_MATZE 0xB16B00B5
#define SERIAL_LIV2  0x4C495632

#define ROMSIZE 32768

struct Library *DosBase;
struct ExecBase *SysBase;
struct ExpansionBase *ExpansionBase = NULL;
struct Config *config;

void bankSelect(UBYTE bank, UBYTE *boardBase);

static void setup_liv2_board(struct ideBoard *board) {
  board->bootrom          = CIDER;
  board->bankSelect       = &bankSelect;
  board->flash_init       = &flash_init;
  board->flash_erase_bank = &flash_erase_bank;
  board->flash_erase_chip = &flash_erase_chip;
  board->flash_writeByte  = &flash_writeByte;

  board->flashbase = board->cd->cd_BoardAddr;

  if (board->cd->cd_BoardSize > 65536) {
    // Newer version of the CIDER & RIPPLE firmware give the IDE device a 128K block, with the top 64K dedicated to the Flash
    // This means we can flash the IDE ROM without having to disable IDE
    board->flashbase += 65536;
  }
}

static void setup_olga_board(struct ideBoard *board) {
  board->bootrom          = ATBUS;
  board->bankSelect       = NULL;
  board->flash_init       = &flash_init;
  board->flash_erase_bank = NULL;
  board->flash_erase_chip = &flash_erase_chip;
  board->flash_writeByte  = &flash_writeByte;

  board->flashbase = board->cd->cd_BoardAddr + 1; // Olga BootROM is on odd addresses
}

/**
 * assemble_rom
 * 
 * Given either lide.rom or lide-atbus.rom - adjust the file for the currebt board
 * This is needed because lideflash will update all compatible boards, and can only be supplied with one filename
*/
static void assemble_rom(char *src_buffer, char *dst_buffer, ULONG bufSize, enum BOOTROM dstRomType) {
  if (!src_buffer || !dst_buffer || bufSize == 0) return;

  char *bootstrap = NULL;
  char *driver    = NULL;

  enum BOOTROM srcRomType = (((ULONG *)src_buffer)[0] == SERIAL_LIV2) ? CIDER : ATBUS;

  memset(dst_buffer,0xFF,bufSize);

  if (srcRomType == dstRomType) {
    CopyMem(src_buffer,dst_buffer,bufSize);
  } else {
    driver = src_buffer + 0x1000;

    if (dstRomType == ATBUS) {
      // Source file was lide.rom, remove the 'LIV2' header
      bootstrap = src_buffer + 4;
      CopyMem(bootstrap,dst_buffer,0x1000);
    } else {
      // Source file was lide-atbus.rom, add the 'LIV2' header
      ((ULONG *)dst_buffer)[0] = SERIAL_LIV2;
      CopyMem(src_buffer,dst_buffer + 4, 0xFFC);
    }

    CopyMem(driver,dst_buffer + 0x1000, bufSize - 0x1000);

  } 
}

int main(int argc, char *argv[])
{
  SysBase = *((struct ExecBase **)4UL);
  DosBase = OpenLibrary("dos.library",0);

  int rc = 0;
  int boards_found = 0;

  void *driver_buffer = NULL;
  void *driver_buffer2 = NULL;
  void *cdfs_buffer   = NULL;

  ULONG romSize    = 0;
  ULONG cdfsSize   = 0;

  if (DosBase == NULL) {
    return(rc);
  }

  printf("\n==== LIDE Flash Updater ====\n");

  struct Task *task = FindTask(0);
  SetTaskPri(task,20);
  if ((config = configure(argc,argv)) != NULL) {

    if (config->ide_rom_filename) {
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

      if (romSize < 4096) {
        printf("ROM file too small.\n");
        rc = 5;
        goto exit;
      }

      driver_buffer  = AllocMem(romSize,MEMF_ANY|MEMF_CLEAR);
      driver_buffer2 = AllocMem(romSize,MEMF_ANY|MEMF_CLEAR);

      if (driver_buffer && driver_buffer2) {
        if (readFileToBuf(config->ide_rom_filename,driver_buffer) == false) {
          rc = 5;
          goto exit;
        }
      } else {
        printf("Couldn't allocate memory.\n");
        rc = 5;
        goto exit;
      }
    }

    if (config->cdfs_filename) {

      cdfsSize = getFileSize(config->cdfs_filename);

      if (cdfsSize == 0) {
        rc = 5;
        goto exit;
      }

      if (cdfsSize > 32768) {
        printf("CDFS too large!\n");
        rc = 5;
        goto exit;
      }

      cdfs_buffer = AllocMem(cdfsSize,MEMF_ANY|MEMF_CLEAR);
      if (cdfs_buffer) {
        if (readFileToBuf(config->cdfs_filename,cdfs_buffer) == false) {
          rc = 5;
          goto exit;
        }
      } else {
        printf("Couldn't allocate memory.\n");
        rc = 5;
        goto exit;
      }
    }

    if ((ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library",0)) != NULL) {

      struct ConfigDev *cd = NULL;
      struct ideBoard board;
  
      while ((cd = FindConfigDev(cd,-1,-1)) != NULL) {

        board.cd = cd;

        switch (cd->cd_Rom.er_Manufacturer) {
          case MANUF_ID_OAHR:
            if (cd->cd_Rom.er_Product == PROD_ID_CIDER) {
              printf("Found CIDER IDE");
              setup_liv2_board(&board);
              break;
            } else if (cd->cd_Rom.er_Product == PROD_ID_RIPPLE) {
              printf("Found RIPPLE IDE");
              setup_liv2_board(&board);
              break;
            } else {
              continue; // Skip this board
            }
          
          case MANUF_ID_BSC:
            if (cd->cd_Rom.er_Product == 6) {
              // Is it a LIV2 board pretending to be an AT-Bus 2008?
              if (cd->cd_Rom.er_SerialNumber == SERIAL_LIV2) {
                printf("Found Unknown LIV2 IDE board");
                setup_liv2_board(&board);
                break;
              } else if (cd->cd_Rom.er_SerialNumber == SERIAL_MATZE) {
                struct ConfigDev *tempcd;
                if ((tempcd = FindConfigDev(NULL,MANUF_ID_A1K,PROD_ID_OLGA))) {
                  printf("Found Dicke Olga");

                  if (cd->cd_Rom.er_Type & ERTF_DIAGVALID) {
                    printf("\nSet Olgas Auto-boot switch to \"off\", reboot and try again.\n\n");
                    continue;
                  }

                  setup_olga_board(&board);
                  break;                  
                }
              }
              
              continue; // Skip this board
            }

          default:
            continue; // Skip this board
        }

        printf(" at Address 0x%06x\n",(int)cd->cd_BoardAddr);
        boards_found++;

        UBYTE manufId,devId;
        if (board.flash_init(&manufId,&devId,board.flashbase)) {

          if (config->eraseFlash) {
            printf("Erasing flash.\n");
            board.flash_erase_chip();
          }

          if (config->ide_rom_filename) {
            if (board.bankSelect != NULL) {
              board.bankSelect(0,cd->cd_BoardAddr);
            }

            if (config->eraseFlash == false) {
              if (board.flash_erase_bank != NULL) {
                printf("Erasing IDE bank...\n");
                board.flash_erase_bank();
              } else {
                printf("Erasing IDE flash...\n");
                board.flash_erase_chip();
              }
            }
            printf("Writing IDE ROM.\n");
            assemble_rom(driver_buffer,driver_buffer2,romSize,board.bootrom);
            writeBufToFlash(&board,driver_buffer2,board.flashbase,romSize);
            printf("\n");
          }

          if (config->cdfs_filename) {

            if (cd->cd_Rom.er_Manufacturer == MANUF_ID_OAHR && 
                cd->cd_Rom.er_Product == PROD_ID_RIPPLE && 
                board.bankSelect != NULL) {

              board.bankSelect(1,cd->cd_BoardAddr);
              if (config->eraseFlash == false) {
                printf("Erasing CDFS bank...\n");
                board.flash_erase_bank();
              }
              printf("Writing CDFS.\n");
              writeBufToFlash(&board,cdfs_buffer,board.flashbase,cdfsSize);
            } else {
              printf("This board does not support flashing CDFS.\n");
            }
          }

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
  if (driver_buffer2) FreeMem(driver_buffer2,romSize);
  if (driver_buffer)  FreeMem(driver_buffer,romSize);
  if (cdfs_buffer)    FreeMem(cdfs_buffer,cdfsSize);
  if (config)         FreeMem(config,sizeof(struct Config));
  if (ExpansionBase)  CloseLibrary((struct Library *)ExpansionBase);
  if (DosBase)        CloseLibrary((struct Library *)DosBase);
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

/**
 * bankSelect
 *
 * Select a bank in the flash
 *
 * @param bank the bank number to select
 * @param boardBase base address of the IDE board
*/
void bankSelect(UBYTE bank, UBYTE *boardBase) {
  *(boardBase + BANK_SEL_REG) = (bank << 6);
}


/**
 * writeBufToFlash()
 *
 * Write the buffer to the currently selected flash bank
 *
 * @param source pointer to the source data
 * @param dest pointer to the flash base
 * @param size number of bytes to write
 * @returns true on success
*/
BOOL writeBufToFlash(struct ideBoard *board, UBYTE *source, UBYTE *dest, ULONG size) {
  UBYTE *sourcePtr = NULL;
  UBYTE *destPtr   = NULL;

  int progress = 0;
  int lastProgress = 1;

  fprintf(stdout,"Writing:     ");
  fflush(stdout);

  for (int i=0; i<size; i++) {

    progress = (i*100)/(size-1);

    if (lastProgress != progress) {
      fprintf(stdout,"\b\b\b\b%3d%%",progress);
      fflush(stdout);
      lastProgress = progress;
    }
    sourcePtr = ((void *)source + i);
    board->flash_writeByte(i,*sourcePtr);

  }

  fprintf(stdout,"\n");
  fflush(stdout);

  fprintf(stdout,"Verifying:     ");
  for (int i=0; i<size; i++) {

    progress = (i*100)/(size-1);

    if (lastProgress != progress) {
      fprintf(stdout,"\b\b\b\b%3d%%",progress);
      fflush(stdout);
      lastProgress = progress;
    }
    sourcePtr = ((void *)source + i);
    destPtr = ((void *)dest + (i << 1));
    if (*sourcePtr != *destPtr) {
          printf("\nVerification failed at %06x - Expected %02X but read %02X\n",(int)destPtr,*sourcePtr,*destPtr);
          return false;
    }
  }
  fprintf(stdout,"\n");
  fflush(stdout);
  return true;
}
