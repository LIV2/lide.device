#ifndef MOUNTER_H
#define MOUNTER_H

typedef LONG (*LoadFileSys_fn)(ULONG id1, ULONG id2, void *handle);

struct MountStruct
{
	// Device name. ("myhddriver.device")
	// Offset 0.
	const UBYTE *deviceName;
	// Unused. MountDrive() always scans SCSI targets 0-7 (and LUNs 0-7
	// if luns is set). Kept so the structure layout does not change.
	// Offset 4.
	ULONG *unitNum;
	// Name string used to set Creator field in FileSystem.resource (if KS 1.3) and in FileSystem.resource entries.
	// If NULL: an empty string is used.
	// Offset 8.
	const UBYTE *creatorName;
	// ConfigDev: set if autoconfig board autoboot support is wanted.
	// If NULL, built with NO_CONFIGDEV and bootable partition found: fake ConfigDev is automatically created.
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
	// Enable CD Boot
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
