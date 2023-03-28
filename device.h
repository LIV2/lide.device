#ifndef _DEVICE_H
#define _DEVICE_H

#define MANUF_ID  2092
#define DEV_ID    6
#define MAX_UNITS 4

/**
 * Drive struct
 * 
 * Each register spaced 512 bytes apart
 * To use this code with other boards you may need to adjust these sizes
*/
struct Drive {
    UWORD data[256];
    UBYTE error_features[512];
    UBYTE sectorCount[512];
    UBYTE lbaLow[512];
    UBYTE lbaMid[512];
    UBYTE lbaHigh[512];
    UBYTE devHead[512];
    UBYTE status_command[512];
};

struct IDEUnit {
    struct Unit io_unit;
    struct ConfigDev *cd;
    struct ExecBase *SysBase;
    struct timerequest *TimeReq;
    volatile struct Drive *drive;
    UBYTE unitNum;
    BOOL  primary;
    BOOL  present;
    UBYTE channel;
    UBYTE device_type;
    ULONG change_count;
    UWORD cylinders;
    UWORD heads;
    UWORD sectorsPerTrack;
    UWORD blockSize;
    UWORD blockShift;
    UBYTE last_error[5];
};

struct DeviceBase {
    struct Library  lib;
    struct ExecBase *SysBase;
    struct Library  *ExpansionBase;
    struct Library  *TimerBase;
    struct Task     *Task;
    struct MsgPort  *TaskMP;
    volatile bool   TaskActive;
    struct MsgPort  *TimerMP;
    struct   timerequest *TimeReq;
    BPTR     saved_seg_list;
    BOOL     is_open;
    UBYTE    num_boards;
    UBYTE    num_units;
    struct   IDEUnit *units;
};


#define STR(s) #s      /* Turn s into a string literal without expanding macro definitions (however, \
                          if invoked from a macro, macro arguments are expanded). */
#define XSTR(s) STR(s) /* Turn s into a string literal after macro-expanding it. */

#define DEVICE_NAME "2nd.liv2ride.device"
#define DEVICE_DATE "(3 March 2023)"
#define DEVICE_ID_STRING "liv2ride " XSTR(DEVICE_VERSION) "." XSTR(DEVICE_REVISION) " " DEVICE_DATE /* format is: 'name version.revision (d.m.yy)' */
#define DEVICE_VERSION 123
#define DEVICE_REVISION 0
#define DEVICE_PRIORITY 0 /* Most people will not need a priority and should leave it at zero. */

#endif