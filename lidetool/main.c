// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lidetool
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
#include <exec/ports.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../device.h"
#include <devices/scsidisk.h>
#include <devices/trackdisk.h>

#include "main.h"
#include "config.h"

#define CMD_XFER 0x1001

struct ExecBase *SysBase;
struct Config *config;

/**
 * MakeSCSICmd
 * 
 * Creates an new SCSICmd struct and CDB
 * 
 * @param cdbSize Size of CDB to create
 * @returns Pointer to an initialized SCSICmd struct 
*/
struct SCSICmd * MakeSCSICmd(ULONG cdbSize) {
    UBYTE          *cdb = NULL;
    struct SCSICmd *cmd = NULL;

    if ((cdb = AllocMem(cdbSize,MEMF_ANY|MEMF_CLEAR)) == NULL) {
        return NULL;
    }

    if ((cmd = AllocMem(sizeof(struct SCSICmd),MEMF_ANY|MEMF_CLEAR)) == NULL) {
        FreeMem(cdb,cdbSize);
        return NULL;
    }

    cmd->scsi_Command   = (UBYTE *)cdb;
    cmd->scsi_CmdLength = cdbSize;
    cmd->scsi_Data      = NULL;
    cmd->scsi_Length    = 0;
    cmd->scsi_SenseData = NULL;

    return cmd;
}

/**
 * DeleteSCSICmd
 * 
 * Delete a SCSICmd and its CDB
 * 
 * @param cmd Pointer to a SCSICmd struct to be deleted
*/
void DeleteSCSICmd(struct SCSICmd *cmd) {
    if (cmd) {
        UBYTE *cdb = cmd->scsi_Command;
        if (cdb) FreeMem(cdb,(ULONG)cmd->scsi_CmdLength);
        FreeMem(cmd,sizeof(struct SCSICmd));
    }
}

/** 
 * doScsiInquiry
 * 
 * Send an INQUIRY command to the device and print the details
 * 
 * @param req An open IOStdReq 
 */
static void doScsiInquiry(struct IOStdReq *req) {
  BYTE error = 0;
  struct SCSICmd *cmd;
  struct SCSI_Inquiry *data;

  if ((cmd = MakeSCSICmd(SZ_CDB_10)) != NULL) {
    if ((data = AllocMem(100,MEMF_ANY|MEMF_CLEAR)) != NULL) {
      char vendor[9];
      char product[17];
      char revision[5];
      char serial[9];

      cmd->scsi_Command[0] = SCSI_CMD_INQUIRY;
      cmd->scsi_Command[2] = 0;
      cmd->scsi_Command[4] = sizeof(struct SCSI_Inquiry);
      cmd->scsi_Data   = (UWORD *)data;
      cmd->scsi_Length = 100;
      cmd->scsi_Flags  = SCSIF_READ;

      req->io_Actual  = 0;
      req->io_Length  = 100;
      req->io_Command = HD_SCSICMD;
      req->io_Data    = cmd;
      req->io_Offset  = 0;

      error = DoIO((struct IORequest *)req);
      if (error == 0 && cmd->scsi_Status == 0) {
        memset(vendor,0,9);
        memset(product,0,17);
        memset(revision,0,5);
        memset(serial,0,9);

        strncpy(vendor, data->vendor, 8);
        strncpy(product, data->product, 16);
        strncpy(revision, data->revision, 4);
        strncpy(serial, data->serial, 8);
        printf("Vendor:              %s\n",vendor);
        printf("Product:             %s\n", product);
        printf("Revision:            %s\n", revision);
        printf("Serial:              %s\n", serial);
      } else {
        if (error)
          printf("IO Error %d\n", error);
        
        if (cmd->scsi_Status)
          printf("SCSI Status: %d\n",cmd->scsi_Status);
      }
      FreeMem(data,100);
    } else {
      printf("Failed to allocate memory.\n");
    }

    DeleteSCSICmd(cmd);
  } else {
    printf("Failed to allocate memory.\n");
  }
}

/** 
 * DumpUnit
 * 
 * Print various details of the Unit
 * 
 * @param req An open IOStdReq 
 */
static void DumpUnit(struct IOStdReq *req) { 
  
    struct IDEUnit *unit = (struct IDEUnit *)req->io_Unit;

    printf("Device Type:         %d\n", unit->deviceType);
    printf("Transfer method:     %d\n", unit->xferMethod);
    printf("Primary:             %s\n", (unit->primary) ? "Yes" : "No");
    printf("ATAPI:               %s\n", (unit->atapi) ? "Yes" : "No");
    printf("Medium Present:      %s\n", (unit->mediumPresent) ? "Yes" : "No");
    printf("Supports LBA:        %s\n", (unit->lba) ? "Yes" : "No");
    printf("Supports LBA48:      %s\n", (unit->lba48) ? "Yes" : "No");
    printf("C/H/S:               %d/%d/%d\n", unit->cylinders, unit->heads, unit->sectorsPerTrack);
    printf("Logical Sectors:     %ld\n", (long int)unit->logicalSectors);
    printf("READ/WRITE Multiple: %s\n", (unit->xferMultiple) ? "Yes" : "No");
    printf("Multiple count:      %d\n", unit->multipleCount);
    printf("Last Error: ");
    for (int i=0; i<6; i++) {
      printf("%02x ",unit->last_error[i]);
    }
    printf("\n");

}

/**
 * setTransferMode
 * 
 * Set the requested transfer method on the unit
 * 
 * @param req An open IOStdReq 
 */
BYTE setTransferMode(struct IOStdReq *req) {
  BYTE error = 0;

  req->io_Data    = NULL;
  req->io_Offset  = 0;
  req->io_Length  = config->Mode;
  req->io_Command = CMD_XFER;
  error = DoIO((struct IORequest *)req);
  if (error == 0) {
    printf("Transfer mode configured for unit %d\n",config->Unit);
  } else {
    printf("IO Error %d\n", error);
  }

  return error;
}

/**
 * setPio
 * 
 * Set the requested pio mode on the unit
 * 
 * @param req An open IOStdReq 
 */
BYTE setPio(struct IOStdReq *req,int pio) {
  BYTE error = 0;

  req->io_Data    = NULL;
  req->io_Offset  = 0;
  req->io_Length  = pio;
  req->io_Command = CMD_PIO;
  error = DoIO((struct IORequest *)req);
  if (error == 0) {
    printf("PIO mode configured for unit %d\n",config->Unit);
  } else {
    printf("IO Error %d\n", error);
  }

  return error;
}

/**
 * ident
 * 
 * Send an IDENTIFY DEVICE command to the device
 * 
 * @param req An opened IOStdReq
 * 
 * @return non-zero on error
 */
BYTE identify(struct IOStdReq *req) {
  BYTE error = 0;

  struct SCSICmd *cmd = MakeSCSICmd(SZ_CDB_12);
  UWORD *buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR);

  if (cmd) {
    if (buf) {
      cmd->scsi_Actual = 0;
      cmd->scsi_CmdActual = 0;
      cmd->scsi_Flags = SCSIF_READ;
      cmd->scsi_Data = buf;
      cmd->scsi_Length = 512;
      cmd->scsi_Command[0] = SCSI_CMD_ATA_PASSTHROUGH;
      cmd->scsi_Command[1] = (4<<1); // PIO IN
      cmd->scsi_Command[2] = 4 | 2;  //BYT_BLOK | T_LEN etc
      cmd->scsi_Command[4] = 1;      // 1 sector (512 bytes)
      cmd->scsi_Command[9] = 0xEC;   // Identify;

      req->io_Offset  = 0;
      req->io_Actual  = 0;
      req->io_Length  = 12;
      req->io_Command = HD_SCSICMD;
      req->io_Data = cmd;

      error = DoIO((struct IORequest *)req);

      if (error == 0 && cmd->scsi_Status == 0) {
        printf("IDENTIFY DEVICE:");
        for (int i=0; i<256; i++) {
          if (i % 8 == 0) printf("\n%04x: ",i << 1);
          printf("%04x%s",__bswap16(buf[i]), ((i + 1) % 8) == 0 ? "" : "." );
        }
        printf("\n");
      }

      if (error)
        printf("IO Error %d\n", error);
      
      if (cmd->scsi_Status)
        printf("SCSI Status: %d\n",cmd->scsi_Status);

      FreeMem(buf,512);
    }
    DeleteSCSICmd(cmd);
  }


  return error;
}


/**
 * setMultiple
 * 
 * @param req An open IOStdReq
 * @param multiple Multiple count
 * 
*/
static void setMultiple(struct IOStdReq *req, int multiple) {
  struct IDEUnit *unit = (struct IDEUnit *)req->io_Unit;

  if (multiple > 0) {
    unit->xferMultiple = true;
    unit->multipleCount = multiple;
    printf("set multiple transfer: %d\n",multiple);
  } else {
    printf("Transfer multiple disabled.\n");
    unit->xferMultiple = false;
    unit->multipleCount = 1;
  }

}
int main(int argc, char *argv[])
{
  SysBase = *((struct ExecBase **)4UL);

  int rc = 0;
  int error = 0;

  if ((config = configure(argc,argv)) != NULL) {
    struct MsgPort *mp = NULL;
    struct IOStdReq *req = NULL;
    if (mp = CreateMsgPort()) {
      if (req = CreateIORequest(mp,sizeof(struct IOStdReq))) {
        if ((error = OpenDevice(config->Device,config->Unit,(struct IORequest *)req,0)) == 0) {

          if (config->Mode != -1) {
            setTransferMode(req);
          }

          if (config->DumpInfo) {
            doScsiInquiry(req);
            DumpUnit(req);
          }

          if (config->Multiple >= 0) {
            setMultiple(req,config->Multiple);
          }

          if (config->Pio >= 0) {
            setPio(req,config->Pio);
          }

          if (config->DumpIdent) {
            identify(req);
          }

          CloseDevice((struct IORequest *)req);
        } else {
          printf("Error %d opening %s", error, config->Device);
        }


        DeleteIORequest(req);
      } else {
        printf("Failed to create IO Req.\n");
      }
      DeleteMsgPort(mp);
    } else {
      printf("Failed to create Message Port.\n");
    }
  } else {
    usage();
  }

  if (config)        FreeMem(config,sizeof(struct Config));
  if (ExpansionBase) CloseLibrary((struct Library *)ExpansionBase);
  return (rc);
}

