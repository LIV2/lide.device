// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of renamelide
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

const char origDevName[] = "lide.device";

bool renameDev(struct Library *dev);

int main () {
    SysBase = *((struct ExecBase **)4UL);
    struct Library *DevBase = NULL;

    if (DevBase = (struct Library *)FindName(&SysBase->DeviceList,origDevName)) {
        printf("Found %s\n",DevBase->lib_Node.ln_Name);
        if (renameDev(DevBase)) {
            printf("Device renamed to %s\n",DevBase->lib_Node.ln_Name);
        } else {
            printf("Couldn't rename dev\n");
        }
    } else {
        printf("%s not found\n",origDevName);
    }
}

bool renameDev(struct Library *dev) {
    ULONG device_prefix[] = {' nd.', ' rd.', ' th.'};
    char scsi_device[] = "    scsi.device";

    char * devName = (scsi_device + 4);

    /* Prefix the device name if a device with the same name already exists */
    for (int i=0; i<8; i++) {
        if (FindName(&SysBase->DeviceList,devName)) {
            if (i==0) devName = scsi_device;
            switch (i) {
                case 0:
                    *(ULONG *)devName = device_prefix[0];
                    break;
                case 1:
                    *(ULONG *)devName = device_prefix[1];
                    break;
                default:
                    *(ULONG *)devName = device_prefix[2];
                    break;
            }
            *(char *)devName = '2' + i;
        } else {
            char *newDevName = AllocMem(strlen(devName),MEMF_ANY|MEMF_CLEAR);
            if (newDevName != NULL) {
                strcpy(newDevName,devName);
                Forbid();
                dev->lib_Node.ln_Name = newDevName;
                Permit();
                return true;
            } else {
                return false;
            }
            return true;
        }
    }
    return false;
}