#ifndef _ATAPI_H
#define _ATAPI_H

#include <stdbool.h>
#include "device.h"
#include <exec/types.h>

#define atapi_flag_cd (1<<0)
#define atapi_flag_io (1<<1)

#define atapi_err_abort (1<<2)
#define atapi_err_eom   (1<<1)
#define atapi_err_len   (1<<0)

#define ATAPI_CMD_PACKET   0xA0
#define ATAPI_CMD_IDENTIFY 0xA1

bool atapi_identify(struct IDEUnit *unit, UWORD *buffer);
BYTE atapi_translate(APTR io_Data,ULONG lba, ULONG count, ULONG *io_Actual, struct IDEUnit *unit, enum xfer_dir direction);
BYTE atapi_packet(struct SCSICmd *cmd, struct IDEUnit *unit);
UBYTE atapi_test_unit_ready(struct IDEUnit *unit);
UBYTE atapi_request_sense(struct IDEUnit *unit, UBYTE *buffer, int length);
UBYTE atapi_get_capacity(struct IDEUnit *unit);
#endif