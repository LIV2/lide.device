#ifndef MOUNTER_H
#define MOUNTER_H

typedef LONG (*LoadFileSys_fn)(ULONG id1, ULONG id2, void *handle);

struct MountStruct
{
	// Device name. ("myhddriver.device")
	// Offset 0.
	const UBYTE *deviceName;
	// Unit number pointer or single integer value.
	// if >= 0x100 (256), pointer to array of ULONGs, first ULONG is number of unit numbers followed (for example { 2, 0, 1 }. 2 units, unit numbers 0 and 1).
	// if < 0x100 (256): used as a single unit number value.
	// Offset 4.
	ULONG *unitNum;
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
	// LoadFileSys function
	// Optional driver callback to load a filesystem from ROM on demand.
	// Set to NULL if not used
	// Offset 20
	LoadFileSys_fn LoadFileSys;
	// LoadFileSys handle.
	// Opaque context passed to the LoadFileSys function.
	// Offset 24
	void *LoadFileSysCtx;
	// LUNs
	// Offset 28.
	BOOL luns;
	// Short/Long Spinup
	// Offset 30.
	BOOL slowSpinup;
	// Enahle CD Boot
	BOOL cdBoot;
	// Ignore RDBFF_LAST flag
	BOOL ignoreLast;
	// Host controller SCSI ID - set to 255 for non-SCSI controllers
	UBYTE hostId;
};

APTR W_CreateIORequest(struct MsgPort *ioReplyPort, ULONG size, struct ExecBase *SysBase);
void W_DeleteIORequest(APTR iorequest, struct ExecBase *SysBase);
struct MsgPort *W_CreateMsgPort(struct ExecBase *SysBase);
void W_DeleteMsgPort(struct MsgPort *port, struct ExecBase *SysBase);

LONG MountDrive(struct MountStruct *ms);

#endif
