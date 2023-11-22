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

#include "main.h"
#include "config.h"

#define CMD_XFER 0x1001

struct ExecBase *SysBase;
struct Config *config;

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
        if ((error = OpenDevice("lide.device",config->Unit,(struct IORequest *)req,0)) == 0) {
          req->io_Length  = config->Mode;
          req->io_Command = CMD_XFER;
          error = DoIO((struct IORequest *)req);
          if (error == 0) {
            printf("Transfer mode configured for unit %d\n",config->Unit);
          } else {
            printf("IO Error %d\n", error);
          }
        } else {
          printf("Error %d opening lide.device", error);
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