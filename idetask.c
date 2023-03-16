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

#include "device.h"
#include "ata.h"
#include "idetask.h"

struct MsgPort * MakePort() {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;

    LONG sigBit;
    struct MsgPort *mp;
    
    if ((sigBit = AllocSignal(-1L)) == -1) return(NULL);

    mp = (struct MsgPort *)AllocMem(sizeof(struct MsgPort),MEMF_CLEAR|MEMF_PUBLIC);

    if (!mp) {
        FreeSignal(sigBit);
        return (NULL);
    }

    mp->mp_Node.ln_Name = NULL;
    mp->mp_Node.ln_Pri  = 0;
    mp->mp_Node.ln_Type = NT_MSGPORT;
    mp->mp_Flags        = PA_SIGNAL;
    mp->mp_SigBit       = sigBit;
    mp->mp_SigTask      = FindTask(NULL);
    NewList(&mp->mp_MsgList);

    return (mp);
}

void ide_task () {
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    struct Task *task = FindTask(NULL);
    struct MsgPort *mp;
    
    while (task->tc_UserData == NULL); // Wait for TaskData to be populated
    
    struct TaskData *td = (struct TaskData *)task->tc_UserData;
    
    while (td->dev == NULL); // Wait for DeviceBase

    if ((mp = MakePort()) == NULL) {
        td->failure = true;
        // We should delete the task here but the linker complains when i use RemoveTask... TODO...
        while (1);
    }

    td->dev->TaskMP = mp;

    while (1) {
        // Main loop, handle IO Requests as they comee in.

        
    }

}
