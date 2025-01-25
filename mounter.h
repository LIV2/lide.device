#ifndef MOUNTER_H
#define MOUNTER_H

struct UnitStruct
{
	ULONG unitNum;
	struct ConfigDev *configDev;
};

struct MountStruct
{
	// Device name. ("myhddriver.device")
	// Offset 0.
	UBYTE *deviceName;
	// Number of units
	UWORD numUnits;
	// Name string used to set Creator field in FileSystem.resource (if KS 1.3) and in FileSystem.resource entries.
	// If NULL: use device name.
	// Offset 8.
	const UBYTE *creatorName;
	// ConfigDev: set if autoconfig board autoboot support is wanted.
	// If NULL and bootable partition found: fake ConfigDev is automatically created.
	// Offset 12.
	struct ConfigDev *configDev;
	// SysBase.
	// Offset 16.
	struct ExecBase *SysBase;
	// Array of UnitStructs
    struct UnitStruct Units[];
};


APTR W_CreateIORequest(struct MsgPort *ioReplyPort, ULONG size, struct ExecBase *SysBase);
void W_DeleteIORequest(APTR iorequest, struct ExecBase *SysBase);
struct MsgPort *W_CreateMsgPort(struct ExecBase *SysBase);
void W_DeleteMsgPort(struct MsgPort *port, struct ExecBase *SysBase);

int mount_drives(struct ConfigDev *cd, char *devName, struct MountStruct *ms);
LONG MountDrive(struct MountStruct *ms);

#endif