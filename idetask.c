#include <devices/scsidisk.h>
#include <devices/trackdisk.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/execbase.h>
#include <exec/ports.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/expansion.h>
#include <string.h>

#include "device.h"
#include "ata.h"
#include "idetask.h"
#include "scsi.h"

#if DEBUG
#include <clib/debug_protos.h>
#endif

void handle_scsi_command(struct IOStdReq *ioreq) {
    struct SCSICmd *scsi_command = ioreq->io_Data;
    char *inquiry_data;
#if DEBUG >= 2
    KPrintF("Command %04x%04x\n",*scsi_command->scsi_Command);
#endif
    switch (scsi_command->scsi_Command[0]) {
        case SCSICMD_TEST_UNIT_READY:
            ioreq->io_Error = 0;
            break;
        case SCSICMD_INQUIRY:
            inquiry_data = (char *)scsi_command->scsi_Data;
            inquiry_data[0] = 0;
            inquiry_data[1] = 0;
            inquiry_data[2] = 0;
            inquiry_data[3] = 2;
            inquiry_data[4] = 44-4;
            inquiry_data[6] = 0;
            inquiry_data[7] = 0;
            UWORD *identity = AllocMem(512,MEMF_CLEAR|MEMF_ANY);
            if (!identity) {
                ioreq->io_Error = TDERR_NotSpecified;
                break;
            }
            if (!(ata_identify((struct IDEUnit *)ioreq->io_Unit,identity))) {
                ioreq->io_Error = TDERR_NotSpecified;
                break;
            }
            CopyMem(&identity[ata_identify_model],&inquiry_data[8],24);
            CopyMem(&identity[ata_identify_fw_rev],&inquiry_data[32],4);
            CopyMem(&identity[ata_identify_serial],&inquiry_data[36],8);
            FreeMem(identity,512);
            scsi_command->scsi_SenseActual = 0;
            scsi_command->scsi_Actual = scsi_command->scsi_Length;
            scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
            ioreq->io_Error = 0;
            break;

        default:
            ioreq->io_Error = TDERR_NotSpecified;
            break;
    }

}

void ide_task () {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct Task *task = FindTask(NULL);
    struct MsgPort *mp;
    struct IOStdReq *ioreq;
    struct IDEUnit *unit;
    UWORD blocksize = 512;
    ULONG lba   = 0;
    ULONG count = 0;
#if DEBUG >= 1
    KPrintF("Task waiting for init\n");
#endif
    while (task->tc_UserData == NULL); // Wait for TaskBase to be populated
    
    struct DeviceBase *dev = (struct DeviceBase *)task->tc_UserData;
    
    if ((mp = CreatePort(NULL,0)) == NULL) {
        dev->Task = NULL;
        RemTask(NULL);
        Wait(0);
    }

    dev->TaskMP = mp;

    while (1) {
        // Main loop, handle IO Requests as they comee in.
#if DEBUG >= 2
            KPrintF("WaitPort()\n");
#endif
        Wait(1 << mp->mp_SigBit);

        while ((ioreq = (struct IOStdReq *)GetMsg(mp))) {
            unit = (struct IDEUnit *)ioreq->io_Unit;
            switch (ioreq->io_Command) {

                case CMD_READ:
#if DEBUG >= 2
                    KPrintF("CMD_READ\n");
#endif
                    ioreq->io_Actual = 0;
#if DEBUG >= 2
                    KPrintF("Offset: %04x%04x Size:%04x%04x\n", ioreq->io_Offset,ioreq->io_Length);
#endif     
                    blocksize = ((struct IDEUnit *)ioreq->io_Unit)->blockSize;
                    lba = (((long long)ioreq->io_Actual << 32 | ioreq->io_Offset) / (UWORD)blocksize);
#if DEBUG >= 2
                    KPrintF("LBA: %04x%04x\n", lba);
#endif                
                    count = (ioreq->io_Length/(UWORD)blocksize);
                    ioreq->io_Error = ata_read(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit);
                    break;

                case CMD_WRITE:
                    ioreq->io_Actual = 0;
                    blocksize = ((struct IDEUnit *)ioreq->io_Unit)->blockSize;
                    lba = (((long long)ioreq->io_Actual << 32 | ioreq->io_Offset) / (UWORD)blocksize);
                    count = (ioreq->io_Length/(UWORD)blocksize);
                    ioreq->io_Error = ata_write(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit);
                    ioreq->io_Error = 0;
                    break;

                case HD_SCSICMD:
                    handle_scsi_command(ioreq);
                    break;
                default:
                    ioreq->io_Error = IOERR_NOCMD;
                    ioreq->io_Actual = 0;
                    break;
            }

            ReplyMsg(&ioreq->io_Message);
        }
    }

}

