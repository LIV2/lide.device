#ifndef _ATA_H
#define _ATA_H

#include <stdbool.h>
#include "device.h"

#define MAX_TRANSFER_SECTORS 255 // Max amount of sectors to transfer per read/write command
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

#define drv_sel_secondary (1<<4)

#define ata_flag_busy  (1<<7)
#define ata_flag_ready (1<<6)
#define ata_flag_df    (1<<5)
#define ata_flag_drq   (1<<3)
#define ata_flag_error (1<<0)


#define atapi_flag_cd (1<<0)
#define atapi_flag_io (1<<1)

#define atapi_err_abort (1<<2)
#define atapi_err_eom   (1<<1)
#define atapi_err_len   (1<<0)


#define ATA_CMD_IDENTIFY   0xEC
#define ATA_CMD_READ       0x20
#define ATA_CMD_WRITE      0x30
#define ATAPI_CMD_PACKET   0xA0
#define ATAPI_CMD_IDENTIFY 0xA1

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

enum xfer_dir {
    READ,
    WRITE
};


bool ata_init_unit(struct IDEUnit *);
bool ata_identify(struct IDEUnit *, UWORD *);
BYTE ata_transfer(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit, enum xfer_dir direction);
void ata_read_fast (void *, void *);
void ata_write_fast (void *, void *);

bool atapi_identify(struct IDEUnit *unit, UWORD *buffer);
BYTE atapi_translate(APTR io_Data,ULONG lba, ULONG count, ULONG *io_Actual, struct IDEUnit *unit, enum xfer_dir direction);
BYTE atapi_packet(struct SCSICmd *cmd, struct IDEUnit *unit);
UBYTE atapi_test_unit_ready(struct IDEUnit *unit);
UBYTE atapi_request_sense(struct IDEUnit *unit);
#endif