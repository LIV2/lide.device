#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <inline/timer.h>
#include <proto/exec.h>
#include <proto/alib.h>
#include <proto/expansion.h>
#include <exec/errors.h>

#include <stdbool.h>
#include <stdio.h>

#include "debug.h"
#include "device.h"
#include "ata.h"
#include "scsi.h"
#include "string.h"

#define XFERSIZE 1049600

int main() {
    struct SCSICmd *cmd = AllocMem(sizeof(struct SCSICmd), MEMF_CLEAR|MEMF_ANY);
    struct SCSI_CDB_10 *cdb = AllocMem(sizeof(struct SCSI_CDB_10),MEMF_ANY|MEMF_CLEAR);
    struct Library *ExpansionBase = NULL;
    UBYTE shadowDevHead;

    if (!(ExpansionBase = (struct Library *)OpenLibrary("expansion.library",0))) goto exit;

    if (cmd == NULL || cdb == NULL) goto exit;
    struct IDEUnit u;

    struct ConfigDev *cd = NULL;
    struct MsgPort *TimerMP = NULL;
    struct timerequest *TimeReq = NULL;

    #ifndef NOTIMER
        if ((TimerMP = CreatePort(NULL,0)) != NULL && (TimeReq = (struct timerequest *)CreateExtIO(TimerMP, sizeof(struct timerequest))) != NULL) {
            if (OpenDevice("timer.device",UNIT_MICROHZ,(struct IORequest *)TimeReq,0)) {
                Info("Failed to open timer.device\n");
                return NULL;
            }
        } else {
            Info("Failed to create Timer MP or Request.\n");
            return NULL;
        }
    #endif
    // Claim our boards until MAX_UNITS is hit
    cd = FindConfigDev(NULL,MANUF_ID,DEV_ID);
    u.cd = cd;
    u.SysBase = *(struct ExecBase **)4UL;
    u.TimeReq = NULL;
    u.primary = true;
    u.channel = 0;
    u.change_count = 1;
    u.device_type = 5;
    u.present = false;
    u.atapi = true;
    u.shadowDevHead = &shadowDevHead;
    u.TimeReq = TimeReq;

    UWORD *buf;
    UWORD *buf2;
    


    buf = AllocMem(512,MEMF_CLEAR|MEMF_ANY);
    buf2 = AllocMem(XFERSIZE,MEMF_ANY|MEMF_CLEAR);
    if (!buf) goto exit;
    if (!buf2) goto exit;

    ata_init_unit(&u);

    printf("Send read req...");
    cmd->scsi_Command     = (UBYTE *)cdb;
    cmd->scsi_CmdLength   = sizeof(struct SCSI_CDB_10);
    cmd->scsi_Flags       = SCSIF_READ;
    cmd->scsi_SenseLength = 0;
    cmd->scsi_SenseData   = NULL;
    cmd->scsi_Length      = XFERSIZE;
    cmd->scsi_Data        = buf2;

    cdb->operation = SCSI_CMD_READ_10;
    cdb->length    = (XFERSIZE + (u.blockSize/2)) / u.blockSize;
    
    UBYTE err = atapi_packet(cmd,&u);
    printf("Transfer returned code %d, %ld bytes transferred of %ld requested.\n",err,cmd->scsi_Actual,cmd->scsi_Length);



exit:
    if (ExpansionBase) CloseLibrary(ExpansionBase);
    if (buf) FreeMem(buf,512);
    if (buf2) FreeMem(buf2,XFERSIZE);
    if (cmd) FreeMem(cmd,sizeof(struct SCSICmd));
    if (cdb) FreeMem(cdb,sizeof(struct SCSI_CDB_10));
    return 0;
}