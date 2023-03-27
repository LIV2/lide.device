#include <devices/scsidisk.h>
#include <devices/timer.h>
#include <devices/trackdisk.h>
#include <exec/errors.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/expansion.h>

#include <string.h>
#include <stdio.h>

#include "ata.h"
#include "device.h"
#include "idetask.h"
#include "newstyle.h"
#include "td64.h"

#if DEBUG
#include <clib/debug_protos.h>
#endif

struct ExecBase *SysBase;

/*-----------------------------------------------------------
A library or device with a romtag should start with moveq #-1,d0 (to
safely return an error if a user tries to execute the file), followed by a
Resident structure.
------------------------------------------------------------*/
int __attribute__((no_reorder)) _start()
{
    return -1;
}

asm("romtag:                                \n"
    "       dc.w    "XSTR(RTC_MATCHWORD)"   \n"
    "       dc.l    romtag                  \n"
    "       dc.l    endcode                 \n"
    "       dc.b    "XSTR(RTF_COLDSTART)"   \n"
    "       dc.b    "XSTR(DEVICE_VERSION)"  \n"
    "       dc.b    "XSTR(NT_DEVICE)"       \n"
    "       dc.b    "XSTR(DEVICE_PRIORITY)" \n"
    "       dc.l    _device_name+4          \n"
    "       dc.l    _device_id_string       \n"
    "       dc.l    _init                   \n"
    "endcode:                               \n");

char device_name[] = DEVICE_NAME;
const char device_id_string[] = DEVICE_ID_STRING;
const char task_name[] = TASK_NAME;


/**
 * set_dev_name
 *
 * Try to set a unique drive name
 * will prepend 2nd/3rd/4th. etc to the beginning of device_name
*/
char * set_dev_name(struct DeviceBase *dev) {
    struct ExecBase *SysBase = dev->SysBase;

    ULONG device_prefix[] = {'2nd.', '3rd.', '4th.', '5th.'};
    char * devName = (device_name + 4); // Start with just the base device name, no prefix

    /* Prefix the device name if a device with the same name already exists */
    for (int i=0; i<4; i++) {
        if (FindName(&SysBase->DeviceList,devName)) {
            if (i==0) devName = device_name;
            *(ULONG *)devName = device_prefix[i]; // Add prefix to start of device name string in a hacky way
        } else {
#if DEBUG > 0
            KPrintF("Device name: %s\n",devName);
#endif
            return devName;
        }
    }
#if DEBUG > 0
    KPrintF("Couldn't set device name.\n");
#endif
    return NULL;
}

/**
 * Cleanup
 *
 * Free used resources back to the system
*/
void Cleanup(struct DeviceBase *dev) {
#if DEBUG > 0
    KPrintF("Cleaning up...\n");
#endif
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    for (int i=0; i < MAX_UNITS; i++) {
        // Un-claim the boards
        if (dev->units[i].cd != NULL) {
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
        }
    }
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
    if (dev->TimerMP) DeletePort(dev->TimerMP);
    if (dev->ExpansionBase) CloseLibrary((struct Library *)dev->ExpansionBase);
    if (dev->units) FreeMem(dev->units,sizeof(struct IDEUnit) * MAX_UNITS);
}

/**
 * init_device
 *
 * Scan for drives and initialize the driver if any are found
*/
struct Library __attribute__((used)) * init_device(struct ExecBase *SysBase asm("a6"), BPTR seg_list asm("a0"), struct DeviceBase *dev asm("d0"))
{
    dev->SysBase = SysBase;
#if DEBUG >= 1
    KPrintF("Init dev, base: %08x\n",dev);
#endif
    struct Library *ExpansionBase = NULL;

    char *devName;

    if (!(devName = set_dev_name(dev))) return NULL;

    /* save pointer to our loaded code (the SegList) */
    dev->saved_seg_list       = seg_list;
    dev->lib.lib_Node.ln_Type = NT_DEVICE;
    dev->lib.lib_Node.ln_Name = devName;
    dev->lib.lib_Flags        = LIBF_SUMUSED | LIBF_CHANGED;
    dev->lib.lib_Version      = DEVICE_VERSION;
    dev->lib.lib_Revision     = DEVICE_REVISION;
    dev->lib.lib_IdString     = (APTR)device_id_string;

    dev->is_open    = FALSE;
    dev->num_boards = 0;
    dev->num_units  = 0;
    dev->TaskMP     = NULL;
    dev->Task       = NULL;
    dev->TaskActive = false;


    if ((dev->units = AllocMem(sizeof(struct IDEUnit)*MAX_UNITS, (MEMF_ANY|MEMF_CLEAR))) == NULL)
        return NULL;
#if DEBUG >=2
    KPrintF("Dev->Units: %x%x\n",(ULONG)dev->units);
#endif
    if (!(ExpansionBase = (struct Library *)OpenLibrary("expansion.library",0))) {
        Cleanup(dev);
        return NULL;
    } else {
        dev->ExpansionBase = ExpansionBase;
    }

#ifndef NOTIMER
    if ((dev->TimerMP = CreatePort(NULL,0)) != NULL && (dev->TimeReq = (struct timerequest *)CreateExtIO(dev->TimerMP, sizeof(struct timerequest))) != NULL) {
        if (OpenDevice("timer.device",UNIT_MICROHZ,(struct IORequest *)dev->TimeReq,0)) {
#if DEBUG >= 1
    KPrintF("Failed to open timer.device\n");
#endif
            Cleanup(dev);
            return NULL;
        }
    } else {
#if DEBUG >= 1
    KPrintF("Failed to create Timer MP or Request.\n");
#endif
        Cleanup(dev);
        return NULL;
    }
#endif
    struct ConfigDev *cd, *next;
    // Claim our boards until MAX_UNITS is hit
    for (cd = NULL; (next = FindConfigDev(cd,MANUF_ID,DEV_ID)) != NULL; cd = next)
    {
        if ((cd = FindConfigDev(NULL,MANUF_ID,DEV_ID)) == NULL) {
            Cleanup(dev);
            return NULL;
        }

        if (cd->cd_Flags & CDF_CONFIGME) {
            cd->cd_Flags &= ~(CDF_CONFIGME); // Claim the board
            dev->num_boards++;
#if DEBUG >= 2
    KPrintF("Claiming board %04x%04x\n",(ULONG)cd->cd_BoardAddr);
#endif
            for (BYTE i=0; i<2; i++) {
                dev->units[i].SysBase      = SysBase;
                dev->units[i].TimeReq      = dev->TimeReq;
                dev->units[i].cd           = cd;
                dev->units[i].primary      = ((i%2) == 1) ? false : true;
                dev->units[i].channel      = ((i%4) < 2) ? 0 : 1;
                dev->units[i].change_count = 1;
                dev->units[i].device_type  = DG_DIRECT_ACCESS;
#if DEBUG >= 2
    KPrintF("testing unit %x%x\n",i);
#endif
                if (ata_init_unit(&dev->units[i])) {
                    dev->num_units++;
                }
            }
            if (dev->num_units == MAX_UNITS) break;
        }
    }

#if DEBUG >= 1
    KPrintF("Detected %x%x drives, %x%x boards\n",dev->num_units, dev->num_boards);
#endif
    if (dev->num_units > 0) {
#if DEBUG >= 1
        KPrintF("Start the Task\n");
#endif
        // Start the IDE Task
        dev->Task = CreateTask(dev->lib.lib_Node.ln_Name,TASK_PRIORITY,(APTR)ide_task,TASK_STACK_SIZE);
        if (!dev->Task) {
#if DEBUG >= 1
            KPrintF("IDE Task failed\n");
#endif
            Cleanup(dev);
            return NULL;
        } else {
#if DEBUG >= 1
            KPrintF("Task created!, waiting for init\n");
#endif
        }
        dev->Task->tc_UserData = (APTR *)dev;
        // Wait for task to init
        while (dev->TaskActive == false) {
            // If dev->task has been set to NULL it means the task failed to start
             if (dev->Task == NULL) {
#if DEBUG >= 1
    KPrintF("IDE Task failed.\n");
#endif
                Cleanup(dev);
                return NULL;
            }
        }
#if DEBUG >= 1
    KPrintF("Startup finished.\n");
#endif
        return (struct Library *)dev;
    } else {
        Cleanup(dev);
        return NULL;
    }
}

/* device dependent expunge function
!!! CAUTION: This function runs in a forbidden state !!!
This call is guaranteed to be single-threaded; only one task
will execute your Expunge at a time. */
static BPTR __attribute__((used)) expunge(struct DeviceBase *dev asm("a6"))
{
#if DEBUG >= 2
    KPrintF((CONST_STRPTR) "running expunge()\n");
#endif

    if (dev->lib.lib_OpenCnt != 0)
    {
        dev->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    if (dev->Task != NULL) {
        // Shut down ide_task

        struct MsgPort *mp = NULL;
        struct IOStdReq *ioreq = NULL;

        if ((mp = CreatePort(NULL,0)) == NULL)
            return 0;
        if ((ioreq = CreateStdIO(mp)) == NULL) {
            DeletePort(mp);
            return 0;
        }

        ioreq->io_Command = CMD_DIE; // Tell ide_task to shut down

        PutMsg(dev->TaskMP,(struct Message *)ioreq);
        WaitPort(mp);                // Wait for ide_task to signal that it is shutting down

        if (ioreq) DeleteStdIO(ioreq);
        if (mp) DeletePort(mp);
    }

    Cleanup(dev);
    BPTR seg_list = dev->saved_seg_list;
    Remove(&dev->lib.lib_Node);
    FreeMem((char *)dev - dev->lib.lib_NegSize, dev->lib.lib_NegSize + dev->lib.lib_PosSize);
    return seg_list;
}

/* device dependent open function
!!! CAUTION: This function runs in a forbidden state !!!
This call is guaranteed to be single-threaded; only one task
will execute your Open at a time. */
static void __attribute__((used)) open(struct DeviceBase *dev asm("a6"), struct IORequest *ioreq asm("a1"), ULONG unitnum asm("d0"), ULONG flags asm("d1"))
{
#if DEBUG >= 2
    KPrintF((CONST_STRPTR) "running open() for unitnum %x%x\n",unitnum);
#endif
    ioreq->io_Error = IOERR_OPENFAIL;

    if (dev->Task == NULL) {
        ioreq->io_Error = IOERR_OPENFAIL;
    }


    if (unitnum >= MAX_UNITS || dev->units[unitnum].present == false) {
        ioreq->io_Error = TDERR_BadUnitNum;
        return;
    }

    ioreq->io_Unit = (struct Unit *)&dev->units[unitnum];

    if (!dev->is_open)
    {
        dev->is_open = TRUE;
    }

    dev->lib.lib_OpenCnt++;
    ioreq->io_Error = 0; //Success
}


static void td_get_geometry(struct IOStdReq *ioreq) {
    struct DriveGeometry *geometry = (struct DriveGeometry *)ioreq->io_Data;
    struct IDEUnit *unit = (struct IDEUnit *)ioreq->io_Unit;

    geometry->dg_SectorSize   = unit->blockSize;
    geometry->dg_TotalSectors = (unit->cylinders * unit->heads * unit->sectorsPerTrack);
    geometry->dg_Cylinders    = unit->cylinders;
    geometry->dg_CylSectors   = (unit->sectorsPerTrack * unit->heads);
    geometry->dg_Heads        = unit->heads;
    geometry->dg_TrackSectors = unit->sectorsPerTrack;
    geometry->dg_BufMemType   = MEMF_PUBLIC;
    geometry->dg_DeviceType   = unit->device_type;
    geometry->dg_Flags        = 0;

    ioreq->io_Error = 0;
    ioreq->io_Actual = sizeof(struct DriveGeometry);
}


/* device dependent close function
!!! CAUTION: This function runs in a forbidden state !!!
This call is guaranteed to be single-threaded; only one task
will execute your Close at a time. */
static BPTR __attribute__((used)) close(struct DeviceBase *dev asm("a6"), struct IORequest *ioreq asm("a1"))
{
#if DEBUG >= 2
    KPrintF((CONST_STRPTR) "running close()\n");
#endif
    dev->lib.lib_OpenCnt--;

    if (dev->lib.lib_OpenCnt == 0 && (dev->lib.lib_Flags & LIBF_DELEXP))
        return expunge(dev);

    return 0;
}


static UWORD supported_commands[] =
{
    CMD_CLEAR,
    CMD_UPDATE,
    CMD_READ,
    CMD_WRITE,
    TD_PROTSTATUS,
    TD_CHANGENUM,
    TD_CHANGESTATE,
    TD_GETDRIVETYPE,
    TD_GETGEOMETRY,
    TD_PROTSTATUS,
    TD_READ64,
    TD_WRITE64,
    TD_FORMAT64,
    NSCMD_DEVICEQUERY,
    NSCMD_TD_READ64,
    NSCMD_TD_WRITE64,
    NSCMD_TD_FORMAT64,
    HD_SCSICMD,
    0
};

/**
 * begin_io
 *
 * Handle immediate requests and send any others to ide_task
*/
static void __attribute__((used)) begin_io(struct DeviceBase *dev asm("a6"), struct IOStdReq *ioreq asm("a1"))
{
#if DEBUG >= 2
    KPrintF((CONST_STRPTR) "running begin_io()\n");
#endif
    ioreq->io_Error = TDERR_NotSpecified;

    if (dev->Task == NULL) {
        ioreq->io_Error = IOERR_OPENFAIL;
    }

    if (ioreq == NULL || ioreq->io_Unit == 0) return;

    switch (ioreq->io_Command) {
        case CMD_CLEAR:
        case CMD_UPDATE:
            ioreq->io_Actual = 0;
            ioreq->io_Error  = 0;
            break;

        case TD_CHANGENUM:
            ioreq->io_Actual = ((struct IDEUnit *)ioreq->io_Unit)->change_count;
            ioreq->io_Error  = 0;
            break;

        case TD_GETDRIVETYPE:
            ioreq->io_Actual = ((struct IDEUnit *)ioreq->io_Unit)->device_type;
            ioreq->io_Error  = 0;
            break;

        case TD_CHANGESTATE:
            ioreq->io_Actual = (((struct IDEUnit *)ioreq->io_Unit)->present) ? 0 : 1;
            ioreq->io_Error  = 0;
            break;

        case TD_PROTSTATUS:
            ioreq->io_Actual = 0; // Not protected
            ioreq->io_Error  = 0;
            break;

        case TD_GETGEOMETRY:
            td_get_geometry(ioreq);
            break;

        case CMD_READ:
        case CMD_WRITE:
            ioreq->io_Actual = 0; // Clear high offset for 32-bit commands
        case TD_FORMAT:
        case TD_READ64:
        case TD_WRITE64:
        case TD_FORMAT64:
        case NSCMD_TD_READ64:
        case NSCMD_TD_WRITE64:
        case NSCMD_TD_FORMAT64:
        case HD_SCSICMD:
            // Send all of these to ide_task
            ioreq->io_Flags &= ~IOF_QUICK;
            PutMsg(dev->TaskMP,&ioreq->io_Message);
#if DEBUG >= 2
    KPrintF((CONST_STRPTR) "IO queued\n");
#endif
            return;

        case NSCMD_DEVICEQUERY:
            if (ioreq->io_Length >= sizeof(struct NSDeviceQueryResult))
            {
                struct NSDeviceQueryResult *result = ioreq->io_Data;

                result->DevQueryFormat    = 0;
                result->SizeAvailable     = sizeof(struct NSDeviceQueryResult);
                result->DeviceType        = NSDEVTYPE_TRACKDISK;
                result->DeviceSubType     = 0;
                result->SupportedCommands = supported_commands;

                ioreq->io_Actual = sizeof(struct NSDeviceQueryResult);
                ioreq->io_Error = 0;
            }
            else {
                ioreq->io_Error = IOERR_BADLENGTH;
            }
            break;

        default:
#if DEBUG >= 2
            KPrintF("Unknown command %x%x\n", ioreq->io_Command);
#endif
            ioreq->io_Error = IOERR_NOCMD;
    }
    if (!ioreq->io_Flags & IOF_QUICK) {
            ReplyMsg(&ioreq->io_Message);
    }
}

/**
 * abort_io
 *
 * Abort io request
*/
static ULONG __attribute__((used)) abort_io(struct Library *dev asm("a6"), struct IOStdReq *ioreq asm("a1"))
{
#if DEBUG >= 2
    KPrintF((CONST_STRPTR) "running abort_io()\n");
#endif
    return IOERR_NOCMD;
}


static const ULONG device_vectors[] =
    {
        (ULONG)open,
        (ULONG)close,
        (ULONG)expunge,
        0, //extFunc not used here
        (ULONG)begin_io,
        (ULONG)abort_io,
        -1 //function table end marker
    };

/**
 * init
 *
 * Create the device and add it to the system if init_device succeeds
*/
static struct Library __attribute__((used)) * init(BPTR seg_list asm("a0"))
{
    #if DEBUG >= 1
    KPrintF("Init driver.\n");
    #endif
    SysBase = *(struct ExecBase **)4UL;
    struct Device *mydev = (struct Device *)MakeLibrary((ULONG *)&device_vectors,  // Vectors
                                                        NULL,                      // InitStruct data
                                                        (APTR)init_device,         // Init function
                                                        sizeof(struct DeviceBase), // Library data size
                                                        seg_list);                 // Segment list

    if (mydev != NULL) {
    #if DEBUG >= 1
    KPrintF("Add Device.\n");
    #endif
        AddDevice((struct Device *)mydev);
    }
    return (struct Library *)mydev;
}
