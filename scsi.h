// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#ifndef _SCSI_H
#define _SCSI_H

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
#define SCSI_CMD_READ_TOC         0x43
#define SCSI_CMD_PLAY_AUDIO_MSF   0x47
#define SCSI_CMD_PLAY_TRACK_INDEX 0x48
#define SCSI_CMD_MODE_SELECT_10   0x55
#define SCSI_CMD_MODE_SENSE_10    0x5A
#define SCSI_CMD_START_STOP_UNIT  0x1B
#define SCSI_CMD_ATA_PASSTHROUGH  0xA1
#define SCSI_CHECK_CONDITION      0x02

#define SZ_CDB_10 10
#define SZ_CDB_12 12

#define SCSI_CD_MAX_TRACKS 100

#define SCSI_TOC_SIZE (SCSI_CD_MAX_TRACKS * 8) + 4 // SCSI_CD_MAX_TRACKS track descriptors + the toc header

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

struct __attribute__((packed)) SCSI_TOC_TRACK_DESCRIPTOR {
    UBYTE reserved1;
    UBYTE adrControl;
    UBYTE trackNumber;
    UBYTE reserved2;
    UBYTE reserved3;
    UBYTE minute;
    UBYTE second;
    UBYTE frame;
};

struct __attribute__((packed)) SCSI_CD_TOC {
    UWORD length;
    UBYTE firstTrack;
    UBYTE lastTrack;
    struct SCSI_TOC_TRACK_DESCRIPTOR td[SCSI_CD_MAX_TRACKS];
};

struct __attribute__((packed)) SCSI_TRACK_MSF {
    UBYTE minute;
    UBYTE second;
    UBYTE frame;
};

struct __attribute__((packed)) SCSI_CDB_ATA {
    UBYTE operation;
    UBYTE protocol; // Bits 7-5: MULTIPLE_COUNT, Bits 4-1: Protocol
    UBYTE length;
    UBYTE features;
    UBYTE sectorCount;
    UBYTE lbaLow;
    UBYTE lbaMid;
    UBYTE lbaHigh;
    UBYTE devHead;
    UBYTE command;
    UBYTE reserved;
    UBYTE control;
};

#define ATA_NODATA 3
#define ATA_PIO_IN 4
#define ATA_PIO_OUT 5

#define ATA_TLEN_MASK 3
#define ATA_BYT_BLOK (1<<2)

void scsi_sense(struct SCSICmd* command, ULONG info, ULONG specific, BYTE error);
struct SCSICmd * MakeSCSICmd(ULONG cdbSize);
void DeleteSCSICmd(struct SCSICmd *cmd);


#endif