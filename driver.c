#include <proto/exec.h>
#include <proto/expansion.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/ports.h>
#include <libraries/dos.h>
#include <devices/trackdisk.h>
#include <string.h>
#include <proto/alib.h>
#include <libraries/dos.h>

#include "idetask.h"
#include "device.h"
#include "ata.h"

#if DEBUG
#include <clib/debug_protos.h>
#endif

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
            return devName;
        }
    }

    return NULL;
}

static struct Library __attribute__((used)) * init_device(BPTR seg_list /*asm("a0")*/, struct DeviceBase *dev /*asm("d0")*/)
{
    struct ExecBase *SysBase = *(struct Execbase **)4UL;
    struct TaskData *td;
    char *devName;

    if (!(devName = set_dev_name(dev))) return NULL;

    /* save pointer to our loaded code (the SegList) */
    dev->saved_seg_list = seg_list;
    dev->SysBase = SysBase;
    dev->lib.lib_Node.ln_Type = NT_DEVICE;
    dev->lib.lib_Node.ln_Name = devName;
    dev->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    dev->lib.lib_Version = DEVICE_VERSION;
    dev->lib.lib_Revision = DEVICE_REVISION;
    dev->lib.lib_IdString = (APTR)device_id_string;
    dev->is_open = FALSE;


    if ((dev->units = AllocMem(sizeof(struct IDEUnit)*MAX_UNITS, (MEMF_ANY|MEMF_CLEAR))) == NULL) goto err_cleanup;

    struct Library *ExpansionBase = (struct Library *)OpenLibrary("expansion.library",0);    

    if (ExpansionBase == NULL) goto err_cleanup;

    struct ConfigDev *cd, *next;

    // Claim our boards until MAX_UNITS is hit
    for (cd = NULL; (next = FindConfigDev(cd,MANUF_ID,DEV_ID)) != NULL; cd = next)
    {
        if (cd->cd_Flags & CDF_CONFIGME) {
            cd->cd_Flags &= ~(CDF_CONFIGME); // Claim the board

            for (int i=0; i<4; i++) {
                dev->units[i].cd = cd;
                dev->units[i].primary = (i%2) ? false : true;
                dev->units[i].channel = ((i%4)<2) ? CHANNEL_0 : CHANNEL_1;
                if (ata_init_unit(&dev->units[i])) {
                    dev->num_units++;
                    dev->units[i].present = true;
                } else {
                    dev->units[i].present = false;
                }
            }
        }
        if (dev->num_units == MAX_UNITS) break;
    }

    if (ExpansionBase) CloseLibrary(ExpansionBase);
    if (dev->num_units > 0) {
        if ((dev->Task = (struct Task *)CreateTask(task_name,0,&ide_task,65535)) == NULL) goto err_cleanup; // Start the IDE Task
        td = (struct TaskData *)dev->Task->tc_UserData;
        td->dev = dev; // Give the task the DeviceBase struct

        while (dev->TaskMP == NULL && td->failure == false) // Wait for task to init

        if (td->failure == true) goto err_cleanup;

        return (struct Library *)dev;
    } else {
err_cleanup:
        // Some kind of error occured, make sure to clean up after ourselves...

        if (ExpansionBase) CloseLibrary("expansion.base");
        if (dev->units) FreeMem(dev->units,sizeof(struct IDEUnit) * MAX_UNITS);

        return NULL;
    }
}

/* device dependent expunge function 
!!! CAUTION: This function runs in a forbidden state !!! 
This call is guaranteed to be single-threaded; only one task 
will execute your Expunge at a time. */
static BPTR __attribute__((used)) expunge(struct DeviceBase *dev asm("a6"))
{
    struct ExecBase *SysBase = dev->SysBase;
#if DEBUG
    KPrintF((CONST_STRPTR) "running expunge()\n");
#endif

    if (dev->lib.lib_OpenCnt != 0)
    {
        dev->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    //xyz_shutdown();

    FreeMem(dev->units,sizeof(struct IDEUnit) * MAX_UNITS);

    BPTR seg_list = dev->saved_seg_list;
    Remove(&dev->lib.lib_Node);
    FreeMem((char *)dev - dev->lib.lib_NegSize, dev->lib.lib_NegSize + dev->lib.lib_PosSize);
    return seg_list;
}

/* device dependent open function 
!!! CAUTION: This function runs in a forbidden state !!!
This call is guaranteed to be single-threaded; only one task 
will execute your Open at a time. */
//static void __attribute__((used)) open(struct DeviceBase *dev asm("a6"), struct IORequest *ioreq asm("a1"), ULONG unitnum asm("d0"), ULONG flags asm("d1"))
static void __attribute__((used)) open(struct DeviceBase *dev, struct IORequest *ioreq, ULONG unitnum, ULONG flags)
{
#if DEBUG
    KPrintF((CONST_STRPTR) "running open()\n");
#endif

    ioreq->io_Error = IOERR_OPENFAIL;
    ioreq->io_Message.mn_Node.ln_Type = NT_REPLYMSG;

    if (unitnum > MAX_UNITS || dev->units[unitnum].present == false) {
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

/* device dependent close function 
!!! CAUTION: This function runs in a forbidden state !!!
This call is guaranteed to be single-threaded; only one task 
will execute your Close at a time. */
static BPTR __attribute__((used)) close(struct DeviceBase *dev asm("a6"), struct IORequest *ioreq asm("a1"))
{
#if DEBUG
    KPrintF((CONST_STRPTR) "running close()\n");
#endif

    ioreq->io_Device = NULL;
    ioreq->io_Unit = NULL;

    dev->lib.lib_OpenCnt--;

    if (dev->lib.lib_OpenCnt == 0 && (dev->lib.lib_Flags & LIBF_DELEXP))
        return expunge(dev);

    return 0;
}

/* device dependent beginio function */
static void __attribute__((used)) begin_io(struct Library *dev asm("a6"), struct IORequest *ioreq asm("a1"))
{
#if DEBUG
    KPrintF((CONST_STRPTR) "running begin_io()\n");
#endif
}

/* device dependent abortio function */
static ULONG __attribute__((used)) abort_io(struct Library *dev asm("a6"), struct IORequest *ioreq asm("a1"))
{
#if DEBUG
    KPrintF((CONST_STRPTR) "running abort_io()\n");
#endif

    return IOERR_NOCMD;
}


static ULONG device_vectors[] =
    {
        (ULONG)open,
        (ULONG)close,
        (ULONG)expunge,
        0, //extFunc not used here
        (ULONG)begin_io,
        (ULONG)abort_io,
        -1 //function table end marker
    };


static struct Library __attribute__((used)) * init(BPTR seg_list asm("a0"), struct ExecBase *SysBase asm("d6"))
{
    struct Device *mydev = (struct Device *)MakeLibrary((ULONG *)&device_vectors,      // Vectors
                                           NULL,                      // InitStruct data
                                           (APTR)init_device,         // Init function
                                           sizeof(struct DeviceBase), // Library data size
                                           seg_list);                 // Segment list
    if (mydev != NULL) {
        AddDevice((struct Device *)mydev);
    }
    return (struct Library *)mydev;
}
