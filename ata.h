#ifndef _ATA_H
#define _ATA_H

#include <proto/exec.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/errors.h>
#include <libraries/dos.h>
#include <stdbool.h>
#include "device.h"

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

#define drv_sel_secondary (1<<4)

#define ata_busy  (1<<7)
#define ata_ready (1<<6)
#define ata_drq   (1<<3)
#define ata_error (1<<0)

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30

// Identify data word offsets
#define ata_identify_cylinders       1
#define ata_identify_heads           3
#define ata_identify_sectorsize      5
#define ata_identify_sectors         6
#define ata_identify_serial          10
#define ata_identify_fw_rev          23
#define ata_identify_model           27
#define ata_identify_capabilities    49
#define ata_identify_logical_sectors 60
#define ata_identify_pio_modes       64

#define ata_capability_lba (1<<9)
#define ata_capability_dma (1<<8)

bool ata_wait_busy(struct IDEUnit *);
bool ata_wait_ready(struct IDEUnit *);
bool ata_wait_drq(struct IDEUnit *);

bool ata_init_unit(struct IDEUnit *);
bool ata_identify(struct IDEUnit *, UWORD *);
BYTE ata_read(APTR *buffer, ULONG lba, UBYTE count, ULONG *actual, struct IDEUnit *unit);
BYTE ata_write(APTR *buffer, ULONG lba, UBYTE count, ULONG *actual, struct IDEUnit *unit);


#endif