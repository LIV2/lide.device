// SPDX-License-Identifier: GPL-2.0-only
/* This file is part of lide.device
 * Copyright (C) 2023 Matthew Harlum <matt@harlum.net>
 */
#ifndef _ATAPI_H
#define _ATAPI_H

#include <stdbool.h>
#include "device.h"
#include "scsi.h"
#include <exec/types.h>

#define atapi_flag_cd (1<<0)
#define atapi_flag_io (1<<1)

#define atapi_err_abort (1<<2)
#define atapi_err_eom   (1<<1)
#define atapi_err_len   (1<<0)

#define ATAPI_CMD_PACKET   0xA0
#define ATAPI_CMD_IDENTIFY 0xA1

#define ATAPI_DRQ_WAIT_LOOP_US 1000
#define ATAPI_DRQ_WAIT_MS 500
#define ATAPI_DRQ_WAIT_COUNT (ATAPI_DRQ_WAIT_MS * (1000 / ATAPI_DRQ_WAIT_LOOP_US))

#define ATAPI_BSY_WAIT_LOOP_US 1000
#define ATAPI_BSY_WAIT_S 5
#define ATAPI_BSY_WAIT_COUNT (ATAPI_BSY_WAIT_S * 1000 * (1000 / ATAPI_BSY_WAIT_LOOP_US))

#define IR_PIO_W   0x0
#define IR_COMMAND 0x1
#define IR_PIO_R   0x2
#define IR_STATUS  0x3

void atapi_dev_reset(struct IDEUnit *unit);
bool atapi_check_signature(struct IDEUnit *unit);
bool atapi_identify(struct IDEUnit *unit, UWORD *buffer);
BYTE atapi_translate(APTR io_Data,ULONG lba, ULONG count, ULONG *io_Actual, struct IDEUnit *unit, enum xfer_dir direction);
BYTE atapi_scsi_read_write_6 (struct SCSICmd *cmd, struct IDEUnit *unit);
BYTE atapi_packet_unaligned(struct SCSICmd *cmd, struct IDEUnit *unit);
BYTE atapi_packet(struct SCSICmd *cmd, struct IDEUnit *unit);
BYTE atapi_test_unit_ready(struct IDEUnit *unit);
BYTE atapi_get_capacity(struct IDEUnit *unit);
BYTE atapi_request_sense(struct IDEUnit *unit, UBYTE *errorCode, UBYTE *senseKey, UBYTE *asc, UBYTE *asq);
BYTE atapi_mode_sense(struct IDEUnit *unit, BYTE page_code, BYTE subpage_code, UWORD *buffer, UWORD length, UWORD *actual, BOOL dbd);
BYTE atapi_scsi_mode_sense_6(struct SCSICmd *cmd, struct IDEUnit *unit);
BYTE atapi_scsi_mode_select_6(struct SCSICmd *cmd, struct IDEUnit *unit);
BYTE atapi_start_stop_unit(struct IDEUnit *unit, bool start, bool loej);
BYTE atapi_check_wp(struct IDEUnit *unit);
bool atapi_update_presence(struct IDEUnit *unit, bool present);
void atapi_do_defer_tur(struct IDEUnit *unit, UBYTE cmd);
BYTE atapi_read_toc(struct IDEUnit *unit, BYTE *buf, ULONG bufSize);
BOOL atapi_get_track_msf(struct SCSI_CD_TOC *toc, int trackNum, struct SCSI_TRACK_MSF *msf);
BYTE atapi_play_track_index(struct IDEUnit *unit, UBYTE start, UBYTE end);
BYTE atapi_play_audio_msf(struct IDEUnit *unit, struct SCSI_TRACK_MSF *start, struct SCSI_TRACK_MSF *end);
BYTE atapi_translate_play_audio_index(struct SCSICmd *cmd, struct IDEUnit *unit);
BYTE atapi_autosense(struct SCSICmd *scsi_command, struct IDEUnit *unit);
#endif