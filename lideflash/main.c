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
#include <proto/alib.h>
#include <dos/dosextens.h>

#include "flash.h"
#include "main.h"
#include "config.h"
#include "matzetk.h"

#define MANUF_ID_BSC  0x082C
#define MANUF_ID_OAHR 5194

#define PROD_ID_CIDER  0x05
#define PROD_ID_RIPPLE 0x07
#define PROD_ID_RIDE   0x09

#define SERIAL_LIV2  0x4C495632

#define ROMSIZE 32768

const char ver[] = VERSION_STRING;

struct Library *DosBase;
struct ExecBase *SysBase;
struct ExpansionBase *ExpansionBase = NULL;
struct Config *config;
bool devsInhibited = false;

void bankSelect(UBYTE bank, struct ideBoard *board);

/**
 * _ColdReboot()
 *
 * Kickstart V36 (2.0+) and up contain a function for this
 * But for 1.3 we will need to provide our own function
 */
static void _ColdReboot() {
  // Copied from coldboot.asm
  // http://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node02E3.html
  asm("move.l  4,a6               \n\t" // SysBase
      "lea.l   DoIt(pc),a5        \n\t"
      "jsr     -0x1e(a6)          \n\t" // Call from Supervisor mode
      ".align 4                   \n\t" // Must be aligned!
      "DoIt:                      \n\t"
      "lea.l   0x1000000,a0       \n\t" // (ROM end)
      "sub.l   -0x14(a0),a0       \n\t" // (ROM end)-(ROM Size)
      "move.l  4(a0),a0           \n\t" // Initial PC
      "subq.l  #2,(a0)            \n\t" // Points to second RESET
      "reset                      \n\t"
      "jmp     (a0)");
}

/**
 * inhibitDosDevs
 * 
 * inhibit/uninhibit all drives
 * Some boards lock out IDE when flash mode is enabled
 * Send an ACTION_INHIBIT packet to all devices to flush the buffers to disk first
 * 
 * @param inhibit (bool) True: inhibit, False: uninhibit 
 */
bool inhibitDosDevs(bool inhibit) {
  bool success = true;
  struct MsgPort *mp = CreatePort(NULL,0);
  struct Message msg;
  struct DosPacket __aligned packet;
  struct DosList *dl;
  struct dosDev *dd;

  struct MinList devs;
  NewList((struct List *)&devs);

  if (mp) {
    packet.dp_Port = mp;
    packet.dp_Link = &msg;
    msg.mn_Node.ln_Name = (char *)&packet;

    if (SysBase->LibNode.lib_Version >= 36) {

      dl = LockDosList(LDF_DEVICES|LDF_READ);
      // Build a list of dos devices to inhibit
      // We need to send a packet to the FS to do the inhibit after releasing the lock
      // So build a list of devs to be (un)-inhibited
      while (dl = NextDosEntry(dl,LDF_DEVICES)) {
        dd = AllocMem(sizeof(struct dosDev),MEMF_ANY|MEMF_CLEAR);
        if (dd) {
          if (dl->dol_Task) { // Device has a FS process?
            dd->handler = dl->dol_Task;
            AddTail((struct List *)&devs,(struct Node *)dd);
          }
        }
      }
      UnLockDosList(LDF_DEVICES|LDF_READ);

    } else {
      // For Kickstart 1.3
      // Build a list of dos devices the old fashioned way
      struct RootNode *rn = DOSBase->dl_Root;
      struct DosInfo *di = BADDR(rn->rn_Info);

      Forbid();
      // Build a list of dos devices to inhibit
      // We need to send a packet to the FS but that can't be done while in Forbid()
      // So build a list of devs to be (un)-inhibited
      for (dl = BADDR(di->di_DevInfo); dl; dl = BADDR(dl->dol_Next)) {
        if (dl->dol_Type == DLT_DEVICE && dl->dol_Task) {
          dd = AllocMem(sizeof(struct dosDev),MEMF_ANY|MEMF_CLEAR);
          if (dd) {
            if (dl->dol_Task) { // Device has a FS process?
              dd->handler = dl->dol_Task;
              AddTail((struct List *)&devs,(struct Node *)dd);
            }
          }
        }
      }
      Permit();
    }

    struct dosDev *next = NULL;
    // Send an ACTION_INHIBIT packet directly to the FS
    for (dd = (struct dosDev *)devs.mlh_Head; dd->mn.mln_Succ; dd = next) {
      if (inhibit) {
        packet.dp_Port = mp;
        packet.dp_Type = ACTION_FLUSH;
        PutMsg(dd->handler,&msg);
        WaitPort(mp);
        GetMsg(mp);
      }

      for (int t=0; t < 3; t++) {
        packet.dp_Port = mp;
        packet.dp_Type = ACTION_INHIBIT;
        packet.dp_Arg1 = (inhibit) ? DOSTRUE : DOSFALSE;
        PutMsg(dd->handler,&msg);
        WaitPort(mp);
        GetMsg(mp);

        if (packet.dp_Res1 == DOSTRUE || packet.dp_Res2 == ERROR_ACTION_NOT_KNOWN)
          break;

        Delay(1*TICKS_PER_SECOND);
      }

      if (packet.dp_Res1 == DOSFALSE && packet.dp_Res2 != ERROR_ACTION_NOT_KNOWN) {
        success = false;
      }

      next = (struct dosDev *)dd->mn.mln_Succ;
      Remove((struct Node *)dd);
      FreeMem(dd,sizeof(struct dosDev));

    }

    DeletePort(mp);

  } else {
    success = false;
  }

  return success;
}

/**
 * setup_liv2_board
 *
 * Configure the board struct for a LIV2 Board (CIDER/RIPPLE)
 * @param board pointer to the board struct
 */
static void setup_liv2_board(struct ideBoard *board) {
  board->bootrom          = CIDER;
  board->bankSelect       = &bankSelect;
  board->writeEnable      = NULL;
  board->rebootRequired   = false;

  board->flashbase = board->cd->cd_BoardAddr;

  if (board->cd->cd_BoardSize > 65536) {
    // Newer version of the CIDER & RIPPLE firmware give the IDE device a 128K block, with the top 64K dedicated to the Flash
    // This means we can flash the IDE ROM without having to disable IDE
    board->flashbase += 65536;
  }

  if (board->cd->cd_Driver == NULL) {
    // Poke IDE register space to ensure ROM overlay turned off
    UBYTE *pokeReg = (UBYTE *)(board->cd->cd_BoardAddr + 0x1200);
    *pokeReg = 0x00;
  }

  switch (board->cd->cd_Rom.er_Product) {
    case PROD_ID_RIPPLE:
      board->banks = 2;
      break;
    case PROD_ID_RIDE:
      board->banks = 4;
      break;
    default:
      board->banks = 1;
      break;
  }
}

/**
 * promptUser
 *
 * Ask if the user wants to update this board
 * @param config pointer to the config struct
 * @return boolean true / false
 */
static bool promptUser(struct Config *config) {
  int c;
  char answer = 'y'; // Default to yes

  printf("Update this device? (Y)es/(n)o/(a)ll: ");

  if (config->assumeYes) {
    printf("y\n");
    return true;
  }

  while ((c = getchar()) != '\n' && c != EOF) answer = c;

  answer |= 0x20; // convert to lowercase;

  if (answer == 'a') {
    config->assumeYes = true;
    return true;
  }

  return (answer == 'y');
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
  void *misc_buffer   = NULL;

  ULONG romSize    = 0;
  ULONG miscSize   = 0;

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

    if (config->misc_filename) {

      miscSize = getFileSize(config->misc_filename);

      if (miscSize == 0) {
        rc = 5;
        goto exit;
      }

      if (miscSize > 32768) {
        printf("File too large!\n");
        rc = 5;
        goto exit;
      }

      misc_buffer = AllocMem(miscSize,MEMF_ANY|MEMF_CLEAR);
      if (misc_buffer) {
        if (readFileToBuf(config->misc_filename,misc_buffer) == false) {
          rc = 5;
          goto exit;
        }
      } else {
        printf("Couldn't allocate memory.\n");
        rc = 5;
        goto exit;
      }
    }
    
    if (!inhibitDosDevs(true)) {
      printf("Failed to inhibit AmigaDOS volumes, wait for disk activity to stop and try again.\n");
      rc = 5;
      inhibitDosDevs(false);
      goto exit;
    };

    devsInhibited = true;

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
            } else if (cd->cd_Rom.er_Product == PROD_ID_RIDE) {
              printf("Found RIDE IDE");
              setup_liv2_board(&board);
              break;
            } else {
              continue; // Skip this board
            }

          case MANUF_ID_A1K:
            if (cd->cd_Rom.er_Product == PROD_ID_MATZE_IDE &&
                cd->cd_Rom.er_SerialNumber == SERIAL_MATZE) {

                if (boardIsOlga(cd)) {
                  printf("Found Dicke Olga");

                  if (!matzetk_fw_supported(cd,OLGA_MIN_FW_VER,false)) {
                    continue;
                  }

                  setup_matzetk_board(&board);
                  break;

                } else if (boardIs68ec020tk(cd)) {
                  printf("Found 68EC020-TK");

                  if (!matzetk_fw_supported(cd,TK020_MIN_FW_VER,false)) {
                    continue;
                  }

                  setup_matzetk_board(&board);
                  break;
                } else if (boardIsZorroLanIDE(cd)) {
                  if (!matzetk_fw_supported(cd,LAN_IDE_MIN_FW_VER,true)) {
                    continue;
                  }
                  
                  printf("Found Zorro-LAN-IDE");

                  setup_matzetk_board(&board);
                  break;
                }

            }

            continue;

          case MANUF_ID_BSC:
            if (cd->cd_Rom.er_Product == 6) {
              // Is it a LIV2 board pretending to be an AT-Bus 2008?
              if (cd->cd_Rom.er_SerialNumber == SERIAL_LIV2) {
                printf("Found Unknown LIV2 IDE board");
                setup_liv2_board(&board);
                break;
              } else if (cd->cd_Rom.er_SerialNumber == SERIAL_MATZE) {

                if (boardIsOlga(cd)) {
                  printf("Found Dicke Olga");

                  if (!matzetk_fw_supported(cd,OLGA_MIN_FW_VER,false)) {
                    continue;
                  }

                  setup_matzetk_board(&board);
                  break;

                } else if (boardIs68ec020tk(cd)) {
                  printf("Found 68EC020-TK");

                  if (!matzetk_fw_supported(cd,TK020_MIN_FW_VER,false)) {
                    continue;
                  }

                  setup_matzetk_board(&board);
                  break;
                } else if (boardIsZorroLanIDE(cd)) {
                  if (!matzetk_fw_supported(cd,LAN_IDE_MIN_FW_VER,true)) {
                    continue;
                  }
                  
                  printf("Found Zorro-LAN-IDE");

                  setup_matzetk_board(&board);
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

        // Ask the user if they wish to update this board
        if (!promptUser(config)) continue;

        if (board.writeEnable != NULL)  // Setup board to allow flash write access
          board.writeEnable(&board);

        if (board.rebootRequired)
          config->rebootRequired = true;

        UBYTE manufId,devId;
        UWORD sectorSize;

        if (flash_init(&manufId,&devId,board.flashbase,&sectorSize)) {
          if (config->eraseFlash) {
            printf("Erasing flash.\n");
            flash_erase_chip();
          }

          if (config->ide_rom_filename) {
            if (board.bankSelect != NULL) {
              board.bankSelect(0,&board);
            }

            if (config->eraseFlash == false) {
              if (sectorSize > 0) {
                printf("Erasing IDE bank...\n");
                flash_erase_bank(sectorSize);
              } else {
                printf("Erasing IDE flash...\n");
                flash_erase_chip();
              }
            }
            printf("Writing IDE ROM.\n");
            assemble_rom(driver_buffer,driver_buffer2,romSize,board.bootrom);
            writeBufToFlash(&board,driver_buffer2,board.flashbase,romSize);
            printf("\n");
          }

          if (config->misc_filename) {

            if (config->misc_bank < board.banks && sectorSize > 0) {

              if (board.bankSelect != NULL)
                board.bankSelect(config->misc_bank,&board);

              if (config->eraseFlash == false) {
                printf("Erasing bank %d...\n",config->misc_bank);
                flash_erase_bank(sectorSize);
              }
              printf("Writing bank %d.\n",config->misc_bank);
              writeBufToFlash(&board,misc_buffer,board.flashbase,miscSize);
            } else {
              printf("This board does not support flashing bank %d.\n",config->misc_bank);
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

    if (config->rebootRequired) {
      printf("Press return to reboot.\n");
      getchar();
      if (SysBase->LibNode.lib_Version >= 36) {
        ColdReboot();
      } else {
        _ColdReboot();
      }
    }

    if (devsInhibited)
      inhibitDosDevs(false);

  } else {
    usage();
  }

exit:
  if (driver_buffer2) FreeMem(driver_buffer2,romSize);
  if (driver_buffer)  FreeMem(driver_buffer,romSize);
  if (misc_buffer)    FreeMem(misc_buffer,miscSize);
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
void bankSelect(UBYTE bank, struct ideBoard *board) {
  UBYTE *bankReg = board->cd->cd_BoardAddr + BANK_SEL_REG;
  UBYTE regbits = *bankReg;
  // Preserve the other bits
  regbits &= 0x3F;
  regbits |= (bank << 6);
  *bankReg = regbits;
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
    flash_writeByte(i,*sourcePtr);

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
