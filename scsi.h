// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#define SCSI_CMD_TEST_UNIT_READY  0x00
#define SCSI_CMD_REQUEST_SENSE    0x03
#define SCSI_CMD_READ_6           0x08
#define SCSI_CMD_WRITE_6          0x0A
#define SCSI_CMD_INQUIRY          0x12
#define SCSI_CMD_MODE_SELECT_6    0x15
#define SCSI_CMD_MODE_SENSE_6     0x1A
#define SCSI_CMD_READ_CAPACITY_10 0x25
#define SCSI_CMD_READ_10          0x28
#define SCSI_CMD_WRITE_10         0x2A
#define SCSI_CMD_MODE_SELECT_10   0x55
#define SCSI_CMD_MODE_SENSE_10    0x5A
#define SCSI_CMD_START_STOP_UNIT  0x1B

#define SCSI_CHECK_CONDITION      0x02

#define SZ_CDB_10 10
#define SZ_CDB_12 12

struct __attribute__((packed)) SCSI_Inquiry {
    UBYTE peripheral_type;
    UBYTE removable_media;
    UBYTE version;
    UBYTE response_format;
    UBYTE additional_length;
    UBYTE flags[3];
    UBYTE vendor[8];
    UBYTE product[16];
    UBYTE revision[4];
    UBYTE serial[8];
};

struct __attribute__((packed)) SCSI_CDB_6 {
    UBYTE operation;
    UBYTE lba_high;
    UBYTE lba_mid;
    UBYTE lba_low;
    UBYTE length;
    UBYTE control;
};

struct __attribute__((packed)) SCSI_CDB_10 {
    UBYTE operation;
    UBYTE flags;
    ULONG lba;
    UBYTE group;
    UWORD length;
    UBYTE control;
};

struct __attribute__((packed)) SCSI_READ_CAPACITY_10 {
    UBYTE operation;
    UBYTE reserved1;
    ULONG lba;
    UWORD reserved2;
    UBYTE flags;
    UBYTE control;
};

struct __attribute__((packed)) SCSI_CAPACITY_10 {
    ULONG lba;
    ULONG block_size;
};

struct __attribute__((packed)) SCSI_FIXED_SENSE {
    UBYTE response;
    UBYTE pad;
    UBYTE senseKey;
    ULONG info;
    UBYTE additional;
    ULONG specific;
    UBYTE asc;
    UBYTE asq;
    UBYTE fru;
    UBYTE sks[3];
};

void scsi_sense(struct SCSICmd* command, ULONG info, ULONG specific, BYTE error);
struct SCSICmd * MakeSCSICmd(ULONG cdbSize);
void DeleteSCSICmd(struct SCSICmd *cmd);