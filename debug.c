#include <exec/io.h>
#include <exec/errors.h>
#include <inline/exec.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>

#include "device.h"
#include "debug.h"
#include "newstyle.h"

#if DEBUG & DBG_MEM
static int memused = 0;

#define OrigAllocMem(___byteSize, ___requirements) \
      LP2(0xc6, APTR, AllocMem , ULONG, ___byteSize, d0, ULONG, ___requirements, d1,\
      , EXEC_BASE_NAME)

#define OrigFreeMem(___memoryBlock, ___byteSize) \
      LP2NR(0xd2, FreeMem , APTR, ___memoryBlock, a1, ULONG, ___byteSize, d0,\
      , EXEC_BASE_NAME)

void * DebugAllocMem(char *file, int line, ULONG byteSize, ULONG requirements)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    KPrintF("AllocMem: %s:%ld %ld %ld\n", file,line, memused, byteSize);
    memused += byteSize;
    return OrigAllocMem(byteSize,requirements);
}

void DebugFreeMem(char *file, int line, void *memBlock, ULONG byteSize) {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    KPrintF("FreeMem: %s:%ld %ld %ld\n", file, line, memused, byteSize);
    memused -= byteSize;
    OrigFreeMem(memBlock,byteSize);
}
#endif

#if DEBUG & DBG_CMD

void traceCommand(struct IOStdReq *req) {
    char *commandName;
    char *err;

    switch (req->io_Command) {
        case CMD_CLEAR:
            commandName = "CMD_CLEAR";
            break;

        case CMD_UPDATE:
            commandName = "CMD_UPDATE";
            break;

        case CMD_READ:
            commandName = "CMD_READ";
            break;

        case CMD_WRITE:
            commandName = "CMD_WRITE";
            break;

        case TD_REMCHANGEINT:
            commandName = "TD_REMCHANGEINT";
            break;

        case TD_PROTSTATUS:
            commandName = "TD_PROTSTATUS";
            break;

        case TD_CHANGENUM:
            commandName = "TD_CHANGENUM";
            break;

        case TD_CHANGESTATE:
            commandName = "TD_CHANGESTATE";
            break;

        case TD_EJECT:
            commandName = "TD_EJECT";
            break;

        case TD_GETDRIVETYPE:
            commandName = "TD_GETDRIVETYPE";
            break;

        case TD_GETGEOMETRY:
            commandName = "TD_GETGEOMETRY";
            break;

        case TD_MOTOR:
            commandName = "TD_MOTOR";
            break;

        case TD_READ64:
            commandName = "TD_READ64";
            break;

        case TD_WRITE64:
            commandName = "TD_WRITE64";
            break;

        case TD_FORMAT64:
            commandName = "TD_FORMAT64";
            break;

        case NSCMD_DEVICEQUERY:
            commandName = "NSCMD_DEVICEQUERY";
            break;

        case NSCMD_TD_READ64:
            commandName = "NSCMD_TD_READ64";
            break;

        case NSCMD_TD_WRITE64:
            commandName = "NSCMD_TD_WRITE64";
            break;

        case NSCMD_TD_FORMAT64:
            commandName = "NSCMD_TD_FORMAT64";
            break;

        case HD_SCSICMD:
            commandName = "HD_SCSICMD";
            break;

        default:
            commandName = "UNKNOWN";
            break;
    }

    switch (req->io_Error) {
        case 0:
            err = "OK";
            break;

        case IOERR_OPENFAIL:
            err = "IOERR_OPENFAIL";
            break;

        case IOERR_ABORTED:
            err = "IOERR_ABORTED";
            break;

        case IOERR_NOCMD:
            err = "IOERR_NOCMD";
            break;

        case IOERR_BADLENGTH:
            err = "IOERR_BADLENGTH";
            break;

        case IOERR_BADADDRESS:
            err = "IOERR_BADADDRESS";
            break;

        case IOERR_UNITBUSY:
            err = "IOERR_UNITBUSY";
            break;

        case IOERR_SELFTEST:
            err = "IOERR_SELFTEST";
            break;

        case HFERR_SelfUnit:
            err = "HFERR_SelfUnit";
            break;

        case HFERR_DMA:
            err = "HFERR_DMA";
            break;

        case HFERR_Phase:
            err = "HFERR_Phase";
            break;

        case HFERR_Parity:
            err = "HFERR_Parity";
            break;

        case HFERR_SelTimeout:
            err = "HFERR_SelTimeout";
            break;

        case HFERR_BadStatus:
            err = "HFERR_BadStatus";
            break;

        case HFERR_NoBoard:
            err = "HFERR_NoBoard";
            break;

        case TDERR_NotSpecified:
            err = "TDERR_NotSpecified";
            break;

        case TDERR_NoSecHdr:
            err = "TDERR_NoSecHdr";
            break;

        case TDERR_BadSecPreamble:
            err = "TDERR_BadSecPreamble";
            break;

        case TDERR_BadSecID:
            err = "TDERR_BadSecID";
            break;

        case TDERR_BadHdrSum:
            err = "TDERR_BadHdrSum";
            break;

        case TDERR_BadSecSum:
            err = "TDERR_BadSecSum";
            break;

        case TDERR_TooFewSecs:
            err = "TDERR_TooFewSecs";
            break;

        case TDERR_BadSecHdr:
            err = "TDERR_BadSecHdr";
            break;

        case TDERR_WriteProt:
            err = "TDERR_WriteProt";
            break;

        case TDERR_DiskChanged:
            err = "TDERR_DiskChanged";
            break;

        case TDERR_SeekError:
            err = "TDERR_SeekError";
            break;

        case TDERR_NoMem:
            err = "TDERR_NoMem";
            break;

        case TDERR_BadUnitNum:
            err = "TDERR_BadUnitNum";
            break;

        case TDERR_BadDriveType:
            err = "TDERR_BadDriveType";
            break;

        case TDERR_DriveInUse:
            err = "TDERR_DriveInUse";
            break;

        case TDERR_PostReset:
            err = "TDERR_PostReset";
            break;

        default:
            err = "UNKNOWN";

    }
    struct IDEUnit *unit = (struct IDEUnit *)req->io_Unit;
    LONG unitNum = unit->unitNum;

    KPrintF("Unit: %ld - %s : Error: %s Actual: %ld\n", unitNum, commandName, err, req->io_Actual);

}

#endif