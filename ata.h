// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#ifndef _ATA_H
#define _ATA_H

#include <stdbool.h>
#include "device.h"
#include <exec/types.h>

#define MAX_TRANSFER_SECTORS 256 // Max amount of sectors to transfer per read/write command
#if MAX_TRANSFER_SECTORS > 256
#error "MAX_TRANSFER_SECTORS cannot be larger than 256"
#endif

#define CHANNEL_0 0x1000
#define CHANNEL_1 0x2000
#define NEXT_REG   0x200

// BYTE Offsets
#define ata_reg_data         0x000
#define ata_reg_error        0x200
#define ata_reg_features     0x200
#define ata_reg_sectorCount  0x400
#define ata_reg_lbaLow       0x600
#define ata_reg_lbaMid       0x800
#define ata_reg_lbaHigh      0xA00
#define ata_reg_devHead      0xC00
#define ata_reg_status       0xE00
#define ata_reg_command      0xE00
#define ata_reg_altStatus    0x1C00

#define drv_sel_secondary (1<<4)

#define ata_flag_busy  (1<<7)
#define ata_flag_ready (1<<6)
#define ata_flag_df    (1<<5)
#define ata_flag_drq   (1<<3)
#define ata_flag_error (1<<0)

#define ata_err_flag_aborted (1<<2)

#define ATA_CMD_DEVICE_RESET   0x08
#define ATA_CMD_IDENTIFY       0xEC
#define ATA_CMD_READ           0x20
#define ATA_CMD_READ_MULTIPLE  0xC4
#define ATA_CMD_WRITE          0x30
#define ATA_CMD_WRITE_MULTIPLE 0xC5
#define ATA_CMD_SET_MULTIPLE   0xC6

// Identify data word offsets
#define ata_identify_cylinders       1
#define ata_identify_heads           3
#define ata_identify_sectors         6
#define ata_identify_serial          10
#define ata_identify_fw_rev          23
#define ata_identify_model           27
#define ata_identify_multiple        47
#define ata_identify_capabilities    49
#define ata_identify_logical_sectors 60
#define ata_identify_pio_modes       64

#define ataf_multiple (1<<8)

#define ata_capability_lba (1<<9)
#define ata_capability_dma (1<<8)

enum xfer_dir {
    READ,
    WRITE
};

#define ATA_DRQ_WAIT_LOOP_US 1000
#define ATA_DRQ_WAIT_S 5
#define ATA_DRQ_WAIT_COUNT (ATA_DRQ_WAIT_S * 1000 * (1000 / ATA_DRQ_WAIT_LOOP_US))

#define ATA_BSY_WAIT_LOOP_US 1000
#define ATA_BSY_WAIT_S 10
#define ATA_BSY_WAIT_COUNT (ATA_BSY_WAIT_S * 1000 * (1000 / ATA_BSY_WAIT_LOOP_US))

#define ATA_RDY_WAIT_LOOP_US 1000
#define ATA_RDY_WAIT_S 3
#define ATA_RDY_WAIT_COUNT (ATA_RDY_WAIT_S * 1000 * (1000 / ATA_RDY_WAIT_LOOP_US))


bool ata_init_unit(struct IDEUnit *);
bool ata_select(struct IDEUnit *unit, UBYTE select, bool wait);
bool ata_identify(struct IDEUnit *, UWORD *);
bool ata_set_multiple(struct IDEUnit *unit, BYTE multiple);

BYTE ata_read(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit);
BYTE ata_write(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit);

void ata_read_unaligned(void *source, void *destination);
void ata_write_unaligned(void *source, void *destination);
#endif