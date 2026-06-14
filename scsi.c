// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <exec/errors.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <string.h>
#include <limits.h>

#include "ata.h"
#include "debug.h"
#include "device.h"
#include "iotask.h"
#include "newstyle.h"
#include "scsi.h"
#include "td64.h"
/**
 * fake_scsi_sense
 *
 * Populate sense data based on the error returned by the ATA functions
 *
 * @param command A pointer to a SCSICmd
 * @param info Sense data Info long
 * @param specific Sense data Specific long
 * @param error Error code returned from ata_transfer
 *
*/
void fake_scsi_sense(struct SCSICmd* command, ULONG info, ULONG specific, BYTE error)
{
    struct SCSI_FIXED_SENSE *sense = (struct SCSI_FIXED_SENSE *)command->scsi_SenseData;
    if (!(command->scsi_Flags & SCSIF_AUTOSENSE) || error == 0 || sense == NULL || (command->scsi_SenseLength < sizeof(struct SCSI_FIXED_SENSE)))
    {
        command->scsi_SenseActual = 0;
        return;
    }

    command->scsi_SenseActual = sizeof(struct SCSI_FIXED_SENSE);

    sense->response   = 0x70; // Fixed format, current status
    sense->pad        = 0;
    sense->info       = info;
    sense->additional = (UBYTE)sizeof(sense) - 7;
    sense->specific   = specific;
    sense->fru        = error;
    switch (error) {
        case IOERR_UNITBUSY:
            sense->senseKey = 0x03;
            sense->asc      = 0x04; // Unit not ready, cause not reportable
            sense->asq      = 0x00;
            break;
        case IOERR_BADADDRESS:
            sense->senseKey = 0x05; // ILLEGAL REQUEST
            sense->asc      = 0x21; // LBA Out of range
            sense->asq      = 0x00;
            break;
        case IOERR_BADLENGTH:
            sense->senseKey = 0x05; // ILLEGAL REQUEST
            sense->asc      = 0x1A; // Parameter list length error
            sense->asq      = 0x00;
            break;
        case IOERR_NOCMD:
            sense->senseKey = 0x05; // ILLEGAL REQUEST
            sense->asc      = 0x20; // Invalid Command operation code
            sense->asq      = 0x00;
            break;
        case HFERR_BadStatus:
            sense->senseKey = 0x05; // ILLEGAL REQUEST
            sense->asc      = 0x20; // Invalid Command operation code
            sense->asq      = 0x00;
            break;
        case TDERR_NotSpecified:
        case HFERR_SelTimeout:
            sense->senseKey = 0x0B; // Unit communication failure
            sense->asc      = 0x08;
            sense->asq      = 0x00;
            break;
        default:
            sense->senseKey = 0x00; // No sense
            sense->asc      = 0x00;
            sense->asq      = 0x00;
            break;
    }
}

/**
 * MakeSCSICmd
 *
 * Creates an new SCSICmd struct and CDB
 *
 * @param cdbSize Size of CDB to create
 * @returns Pointer to an initialized SCSICmd struct
*/
struct SCSICmd *MakeSCSICmd(ULONG cdbSize) {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
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
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;

    if (cmd) {
        UBYTE *cdb = cmd->scsi_Command;
        if (cdb) FreeMem(cdb,(ULONG)cmd->scsi_CmdLength);
        FreeMem(cmd,sizeof(struct SCSICmd));
    }
}

/**
 * scsi_lazy_alloc_cmd
 *
 * Lazily allocate (on first use) and clear a reusable SCSICmd
 *
 * If the SCSICmd pointed to by *cmd has not been allocated yet it is created
 * with a SZ_CDB_12 sized CDB. The SCSICmd struct and its CDB are then cleared,
 * preserving the CDB buffer for reuse. On allocation failure *cmd is left NULL.
 * The caller is responsible for setting scsi_CmdLength and for freeing the
 * SCSICmd via DeleteSCSICmd() when it is no longer needed.
 *
 * @param cmd Address of the SCSICmd pointer to allocate / reuse
*/
static void scsi_lazy_alloc_cmd(struct SCSICmd **cmd) {
    UBYTE *cdb;
    struct SCSICmd *p;

    if (cmd == NULL) return;

    if (*cmd == NULL) { 
        *cmd = MakeSCSICmd(SZ_CDB_12);
    }
    p = *cmd;
    if (p == NULL) return;

    cdb = p->scsi_Command;
    memset(p,0,sizeof(struct SCSICmd));
    memset(cdb,0,SZ_CDB_12);
    p->scsi_Command = cdb;

}

/**
 * scsi_get_unit_cmd
 *
 * Get the unit's reusable SCSICmd, lazily allocating it on first use
 *
 * The SCSICmd is allocated once per unit and reused for subsequent commands to
 * avoid repeated alloc/free cycles that lead to memory fragmentation. The
 * command and its CDB are cleared before being returned. The caller is
 * responsible for setting scsi_CmdLength. The cached SCSICmd is freed when the
 * unit is torn down.
 *
 * NOT re-entrant: there is a single SCSICmd per unit, so a command obtained from
 * this function must not still be in use when it is called again for the same
 * unit. In particular, a function that has an outstanding SCSICmd must not call
 * another function that also obtains one (e.g. issuing REQUEST SENSE while the
 * original command is still live) - such nested users must use the separate
 * sense SCSICmd via scsi_get_sense_cmd() instead.
 *
 * @param unit Pointer to an IDEUnit struct
 * @returns Pointer to the unit's SCSICmd struct, or NULL on allocation failure
*/
struct SCSICmd *scsi_get_unit_cmd(struct IDEUnit *unit) {
    struct ExecBase *SysBase = unit->SysBase;
    if (unit->flags.scsiCmdInUse == false) {
        scsi_lazy_alloc_cmd(&unit->scsiCmd);
        unit->flags.scsiCmdInUse = true;
        return unit->scsiCmd;
    } else {
        Alert(0x01DEBAD1);
        return NULL;
    }
}

/**
 * scsi_get_sense_cmd
 *
 * Get the unit's reusable sense SCSICmd, lazily allocating it on first use
 *
 * This is a second per-unit SCSICmd, separate from the one returned by
 * scsi_get_unit_cmd(). It exists so that REQUEST SENSE / autosense can be issued
 * while a command obtained from scsi_get_unit_cmd() is still live, without the two
 * aliasing each other (see the re-entrancy note on scsi_get_unit_cmd). The caller
 * is responsible for setting scsi_CmdLength. The cached SCSICmd is freed when
 * the unit is torn down.
 *
 * @param unit Pointer to an IDEUnit struct
 * @returns Pointer to the unit's sense SCSICmd struct, or NULL on allocation failure
*/
struct SCSICmd *scsi_get_sense_cmd(struct IDEUnit *unit) {
    struct ExecBase *SysBase = unit->SysBase;
    if (unit->flags.senseCmdInUse == false) {
        scsi_lazy_alloc_cmd(&unit->senseCmd);
        unit->flags.senseCmdInUse = true;
        return unit->senseCmd;
    } else {
        Alert(0x01DEBAD2);
        return NULL;
    }
}

void scsi_release_unit_cmd(struct IDEUnit *unit) {
    unit->flags.scsiCmdInUse = false;
}

void scsi_release_sense_cmd(struct IDEUnit *unit) {
    unit->flags.senseCmdInUse = false;
}

/**
 * scsi_inquiry_emu
 *
 * Emulate SCSI-Direct INQUIRY command
 *
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
BYTE scsi_inquiry_emu(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    struct ExecBase *SysBase = unit->SysBase;

    struct SCSI_Inquiry *data = (struct SCSI_Inquiry *)scsi_command->scsi_Data;

    if (data == NULL)
        return IOERR_BADADDRESS;

    data->peripheral_type   = unit->deviceType;
    data->removable_media   = 0;
    data->version           = 2;
    data->response_format   = 2;
    data->additional_length = (sizeof(struct SCSI_Inquiry) - 4);

    UWORD *identity = AllocMem(512,MEMF_CLEAR|MEMF_ANY);
    if (!identity)
        return TDERR_NoMem;

   if (!(ata_identify(unit,identity))) {
        FreeMem(identity,512);
        return HFERR_SelTimeout;
    }

    CopyMem(&identity[ata_identify_model],&data->vendor,24);
    CopyMem(&identity[ata_identify_fw_rev],&data->revision,4);

    scsi_command->scsi_Actual = 36;

    if (scsi_command->scsi_Length >= sizeof(struct SCSI_Inquiry)) {
        CopyMem(&identity[ata_identify_serial],&data->serial,8);
        scsi_command->scsi_Actual += 8;
    }

    FreeMem(identity,512);
    scsi_command->scsi_Actual = scsi_command->scsi_Length;
    return 0;
}

/**
 * scsi_read_capacity_10_emu
 *
 * Emulate SCSI-Direct READ CAPACITY (10) command
 *
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
BYTE scsi_read_capacity_10_emu(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    struct SCSI_CAPACITY_10 *data = (struct SCSI_CAPACITY_10 *)scsi_command->scsi_Data;

    if (data == NULL)
        return IOERR_BADADDRESS;

    struct SCSI_READ_CAPACITY_10 *cdb = (struct SCSI_READ_CAPACITY_10 *)scsi_command->scsi_Command;

    data->block_size = unit->blockSize;

    if (unit->logicalSectors < ULONG_MAX) {
        if (cdb->flags & 0x01) {
            // Partial Medium Indicator - Return end of cylinder
            // Implement this so HDToolbox stops moaning about track size
            ULONG spc = unit->sectorsPerTrack * unit->heads;
            data->lba = (((cdb->lba / spc) + 1) * spc) - 1;
        } else {
            data->lba = (unit->logicalSectors) - 1;
        }
    } else {
        data->lba = ULONG_MAX;
    }


    scsi_command->scsi_Actual = 8;

    return 0;
}


/**
 * scsi_read_capacity_16_emu
 *
 * Emulate SCSI-Direct READ CAPACITY (16) command
 *
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
BYTE scsi_read_capacity_16_emu(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    struct SCSI_CAPACITY_16 *data = (struct SCSI_CAPACITY_16 *)scsi_command->scsi_Data;

    if (data == NULL)
        return IOERR_BADADDRESS;

    data->block_size = unit->blockSize;
    data->lba = unit->logicalSectors - 1;

    scsi_command->scsi_Actual = 8;

    return 0;
}

/**
 * scsi_mode_sense_emu
 *
 * Emulate SCSI-Direct MODE SENSE (6) commands
 *
 * @param unit Pointer to an IDEUnit struct
 * @param scsi_command Pointer to a SCSICmd struct
*/
BYTE scsi_mode_sense_emu(struct IDEUnit *unit, struct SCSICmd *scsi_command) {
    UBYTE *data    = (APTR)scsi_command->scsi_Data;
    UBYTE *command = (APTR)scsi_command->scsi_Command;

    if (data == NULL)
        return IOERR_BADADDRESS;

    UBYTE page    = command[2] & 0x3F;
    UBYTE subpage = command[3];

    if (subpage != 0 || (page != 0x3F && page != 0x03 && page != 0x04))
        return IOERR_NOCMD;

    if (scsi_command->scsi_Length < ((page == 0x03 || page == 0x04) ? 28 : (page == 0x3f ? 52 : 0)))
        return IOERR_BADLENGTH;

    UBYTE *data_length = data;  // Mode data length
    data[1] = unit->deviceType; // Mode parameter: Media type
    data[2] = 0;                // DPOFUA
    data[3] = 0;                // Block descriptor length

    *data_length = 3;

    UBYTE idx = 4;
    if (page == 0x3F || page == 0x03) {
        data[idx++] = 0x03; // Page Code: Format Parameters
        data[idx++] = 0x16; // Page length
        for (int i=0; i <8; i++) {
            data[idx++] = 0;
        }
        data[idx++] = (unit->sectorsPerTrack >> 8);
        data[idx++] = unit->sectorsPerTrack;
        data[idx++] = (unit->blockSize >> 8);
        data[idx++] = unit->blockSize;
        for (int i=0; i<10; i++) {
            data[idx++] = 0;
        }
    }

    if (page == 0x3F || page == 0x04) {
        data[idx++] = 0x04; // Page code: Rigid Drive Geometry Parameters
        data[idx++] = 0x16; // Page length
        data[idx++] = (unit->cylinders >> 16);
        data[idx++] = (unit->cylinders >> 8);
        data[idx++] = unit->cylinders;
        data[idx++] = unit->heads;
        for (int i=0; i<18; i++) {
            data[idx++] = 0;
        }
    }

    *data_length = idx - 1;
    scsi_command->scsi_Actual = idx;
    return 0;

}
