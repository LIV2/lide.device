// Generic autoboot/automount RDB parser and mounter.
// - KS 1.3 support, including autoboot mode.
// - 68000 compatible.
// - Boot ROM and executable modes.
// - Autoboot capable (Boot ROM mode only).
// - Full automount support
// - Full RDB filesystem support.
//
// Copyright 2021-2022 Toni Wilen
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
#ifdef DEBUG_MOUNTER
#define USE_SERIAL_OUTPUT
#endif
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/alerts.h>
#include <exec/ports.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <devices/trackdisk.h>
#include <devices/hardblocks.h>
#include <devices/scsidisk.h>
#include <resources/filesysres.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <libraries/configvars.h>
#include <clib/alib_protos.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/doshunks.h>

#include <stdio.h>
#include <stdbool.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>

#include "ndkcompat.h"
#include "mounter.h"
#include "scsi.h"

#if TRACE
#define dbg Trace
#else
#define dbg
#endif

#define MAX_BLOCKSIZE 2048
#define LSEG_DATASIZE (512 / 4 - 5)

#if NO_CONFIGDEV
extern UBYTE entrypoint, entrypoint_end;
extern UBYTE bootblock, bootblock_end;
#endif

struct ExecBase *SysBase;
struct FileSysResource *FileSysResBase = NULL;

struct MountData
{
	struct ExecBase *SysBase;
	struct ExpansionBase *ExpansionBase;
	struct DosLibrary *DOSBase;
	struct IOExtTD *request;
	struct ConfigDev *configDev;
	const UBYTE *creator;
	const UBYTE *devicename;

	ULONG lsegblock;
	ULONG lseglongs;
	ULONG lsegoffset;
	struct LoadSegBlock *lsegbuf;
	UWORD lsegwordbuf;
	UWORD lseghasword;

	ULONG unitnum;
	LONG ret;
	UBYTE buf[MAX_BLOCKSIZE * 3];
	UBYTE zero[2];
	BOOL wasLastDev;
	int blocksize;
};

// Get Block size of unit by sending a SCSI READ CAPACITY 10 command
int GetBlockSize(struct IOStdReq *req) {
	struct SCSICmd *cmd = MakeSCSICmd(SZ_CDB_10);

	if (cmd == NULL) return 0;

	struct SCSI_READ_CAPACITY_10 *cdb = (struct SCSI_READ_CAPACITY_10 *)cmd->scsi_Command;
	BYTE err;
	int ret = -1;

	struct SCSI_CAPACITY_10 *result = AllocMem(sizeof(struct SCSI_CAPACITY_10),MEMF_CLEAR|MEMF_ANY);

	if (!result) {
		DeleteSCSICmd(cmd);
		return -1;
	}

	cdb->operation   = SCSI_CMD_READ_CAPACITY_10;
	cmd->scsi_Length = sizeof(struct SCSI_CAPACITY_10);
	cmd->scsi_Data   = (UWORD *)result;
	cmd->scsi_Flags  = SCSIF_READ;
	cdb->lba         = 0;

	req->io_Command = HD_SCSICMD;
	req->io_Data    = cmd;
	req->io_Length  = sizeof(struct SCSICmd);

	for (int retry = 0; retry < 3; retry++) {
		if ((err = DoIO((struct IORequest *)req)) == 0) break;
	}

	if (err == 0 && req->io_Length > 0) {
		ret = result->block_size;
	} else {
		ret = -1;
	}

	if (cmd)    DeleteSCSICmd(cmd);
	if (result) FreeMem(result,sizeof(struct SCSI_CAPACITY_10));

	return ret;
}

#if CDBOOT
// CheckPVD
// Check for "CDTV" or "AMIGA BOOT" as the System ID in the PVD
bool CheckPVD(struct IOStdReq *ior) {
	const char sys_id_1[] = "CDTV";
	const char sys_id_2[] = "AMIGA BOOT";
	const char iso_id[]   = "CD001";

	BYTE err = 0;
	BOOL ret = false;
	char *buf = NULL;

	if (!(buf = AllocMem(2048,MEMF_ANY|MEMF_CLEAR))) goto done;

	ior->io_Command = TD_CHANGESTATE; // Check if there's a disc in the drive

	if (err = DoIO((struct IORequest *)ior) || ior->io_Actual != 0) goto done;

	char *id_string = buf + 1;
	char *system_id = buf + 8;

	ior->io_Command = CMD_READ;
	ior->io_Data    = buf;
	ior->io_Length  = 2048;

	for (int i=0; i < 32; i++) {

		ior->io_Offset = (i + 16) << 11;

		for (int retry = 0; retry < 3; retry++) {
			if ((err = DoIO((struct IORequest*)ior)) == 0) break;
		}

		if (ior->io_Actual < 2048) break;

		// Check ISO ID String & for PVD Version & Type code
		if ((strncmp(iso_id,id_string,5) == 0) && buf[0] == 1 && buf[6] == 1) { 
			if (strncmp(sys_id_1,system_id,strlen(sys_id_1)) == 0 || strncmp(sys_id_2,system_id,strlen(sys_id_2) == 0)) {
				ret = true; // CDTV or AMIGA BOOT
			} else {
				ret = false;
			}
			break;
		} else {
			continue;
		}

	}
done:
	if (buf)  FreeMem(buf,2048);
	return ret;
}
#endif

// KS 1.3 compatibility functions
APTR W_CreateIORequest(struct MsgPort *ioReplyPort, ULONG size, struct ExecBase *SysBase)
{
	struct IORequest *ret = NULL;
	if(ioReplyPort == NULL)
		return NULL;
	ret = (struct IORequest*)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
	if(ret != NULL)
	{
		ret->io_Message.mn_ReplyPort = ioReplyPort;
		ret->io_Message.mn_Length = size;
	}
	return ret;
}
void W_DeleteIORequest(APTR iorequest, struct ExecBase *SysBase)
{
	if(iorequest != NULL) {
		FreeMem(iorequest, ((struct Message*)iorequest)->mn_Length);
	}
}
struct MsgPort *W_CreateMsgPort(struct ExecBase *SysBase)
{
	struct MsgPort *ret;
	ret = (struct MsgPort*)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
	if(ret != NULL)
	{
		BYTE sb = AllocSignal(-1);
		if (sb != -1)
		{
			ret->mp_Flags = PA_SIGNAL;
			ret->mp_Node.ln_Type = NT_MSGPORT;
  			NewList(&ret->mp_MsgList);
			ret->mp_SigBit = sb;
			ret->mp_SigTask = FindTask(NULL);
			return ret;
		}
		FreeMem(ret, sizeof(struct MsgPort));
	}
	return NULL;
}
void W_DeleteMsgPort(struct MsgPort *port, struct ExecBase *SysBase)
{
	if(port != NULL)
	{
		FreeSignal(port->mp_SigBit);
		FreeMem(port, sizeof(struct MsgPort));
	}
}

// Flush cache (Filesystem relocation)
static void cacheclear(struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	if (SysBase->LibNode.lib_Version >= 37) {
		//CacheClearU();
	}
}

// Simply memory copy.
// Only used for few short copies, it does not need to be optimal.
// Required because compiler built-in memcpy() can have
// extra dependencies which will make boot rom build
// impossible.
static void copymem(void *dstp, void *srcp, UWORD size)
{
	UBYTE *dst = (UBYTE*)dstp;
	UBYTE *src = (UBYTE*)srcp;
	while (size != 0) {
		*dst++ = *src++;
		size--;
	}
}

// Check block checksum
static UWORD checksum(UBYTE *buf, struct MountData *md)
{
	ULONG chk = 0;
	ULONG num_longs;

	num_longs = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | (buf[7]);
	if (num_longs > 65535)
		return FALSE;

	for (UWORD i = 0; i < (int)(num_longs * sizeof(LONG)); i += 4) {
		ULONG v = (buf[i + 0] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | (buf[i + 3 ] << 0);
		chk += v;
	}
	if (chk) {
		dbg("Checksum error %08"PRIx32"\n", chk);
		return FALSE;
	}
	return TRUE;
}


#define MAX_RETRIES 3

// Read single block with retries
static BOOL readblock(UBYTE *buf, ULONG block, ULONG id, struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	struct IOExtTD *request = md->request;
	UWORD i;

	request->iotd_Req.io_Command = CMD_READ;
	request->iotd_Req.io_Offset = (block * md->blocksize);
	request->iotd_Req.io_Data = buf;
	request->iotd_Req.io_Length = md->blocksize;
	for (i = 0; i < MAX_RETRIES; i++) {
		LONG err = DoIO((struct IORequest*)request);
		if (!err) {
			break;
		}
		dbg("Read block %"PRIu32" error %"PRId32"\n", block, err);
	}
	if (i == MAX_RETRIES) {
		return FALSE;
	}
	ULONG v = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
	dbg("Read block %"PRIu32" %08"PRIx32"\n", block, v);
	if (id != 0xffffffff) {
		if (v != id) {
			return FALSE;
		}
	}
	if (!checksum(buf, md)) {
		return FALSE;
	}
	return TRUE;
}

// Read multiple longs from LSEG blocks
static BOOL lseg_read_longs(struct MountData *md, ULONG longs, ULONG *data)
{
	dbg("lseg_read_longs, longs %"PRId32"  ptr %p, remaining %"PRId32"\n", longs, data, md->lseglongs);
	ULONG cnt = 0;
	md->lseghasword = FALSE;
	while (longs > cnt) {
		if (md->lseglongs > 0) {
			data[cnt] = md->lsegbuf->lsb_LoadData[md->lsegoffset];
			md->lsegoffset++;
			md->lseglongs--;
			cnt++;
			if (longs == cnt) {
				return TRUE;
			}
		}
		if (!md->lseglongs) {
			if (md->lsegblock == 0xffffffff) {
				dbg("lseg_read_long premature end!\n");
				return FALSE;
			}
			if (!readblock((UBYTE*)md->lsegbuf, md->lsegblock, IDNAME_LOADSEG, md)) {
				return FALSE;
			}
			md->lseglongs = LSEG_DATASIZE;
			md->lsegoffset = 0;
			dbg("lseg_read_long lseg block %"PRId32" loaded, next %"PRId32"\n", md->lsegblock, md->lsegbuf->lsb_Next);
			md->lsegblock = md->lsegbuf->lsb_Next;
		}
	}
	return TRUE;
}
// Read single long from LSEG blocks
static BOOL lseg_read_long(struct MountData *md, ULONG *data)
{
	BOOL v;
	if (md->lseghasword) {
		ULONG temp;
		v = lseg_read_longs(md, 1, &temp);
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
		md->lsegwordbuf = (UWORD)temp;
		md->lseghasword = TRUE;
	} else {
		v = lseg_read_longs(md, 1, data);
	}
	dbg("lseg_read_long %08"PRIx32"\n", *data);
	return v;
}
// Read single word from LSEG blocks
// Internally reads long and buffers second word.
static BOOL lseg_read_word(struct MountData *md, ULONG *data)
{
	if (md->lseghasword) {
		*data = md->lsegwordbuf;
		md->lseghasword = FALSE;
		dbg("lseg_read_word 2/2 %08"PRIx32"\n", *data);
		return TRUE;
	}
	ULONG temp;
	BOOL v = lseg_read_longs(md, 1, &temp);
	if (v) {
		md->lseghasword = TRUE;
		md->lsegwordbuf = (UWORD)temp;
		*data = temp >> 16;
	}
	dbg("lseg_read_word 1/2 %08"PRIx32"\n", *data);
	return v;
}

struct RelocHunk
{
	ULONG hunkSize;
	ULONG *hunkData;
};

// Filesystem relocator
static APTR fsrelocate(struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	ULONG data;
	struct RelocHunk *relocHunks;
	LONG firstHunk, lastHunk;
	LONG totalHunks;
	WORD hunkCnt;
	WORD ret = 0;
	APTR firstProcessedHunk = NULL;

	if (!lseg_read_long(md, &data)) {
		return NULL;
	}
	if (data != HUNK_HEADER) {
		return NULL;
	}
	// Read the size of a resident library name. This should
	// never be != 0.
	if (!lseg_read_long(md, &data) || data != 0) {
		return NULL;
	}
	// Read the size of the hunk table, which should be > 0.
	// Note that this number may be larger than the
	// difference between the last and the first hunk + 1 for
	// overlay binary files. But then this function does not
	// support overlay binary files.
	if (!lseg_read_long(md, &data) || data <= 0) {
		return NULL;
	}
	// first hunk
	if (!lseg_read_long(md, &firstHunk)) {
		return NULL;
	}
	// last hunk
	if (!lseg_read_long(md, &lastHunk)) {
		return NULL;
	}
	if (firstHunk < 0 || lastHunk < 0 || firstHunk > lastHunk) {
		return NULL;
	}
	totalHunks = lastHunk - firstHunk + 1;
	dbg("first hunk %"PRId32", last hunk %"PRId32"\n", firstHunk, lastHunk);
	relocHunks = AllocMem(totalHunks * sizeof(struct RelocHunk), MEMF_CLEAR);
	if (!relocHunks) {
		return NULL;
	}

	// Pre-allocate hunks
	ULONG *prevChunk = NULL;
	hunkCnt = 0;
	while (hunkCnt < totalHunks) {
		struct RelocHunk *rh = &relocHunks[hunkCnt];
		ULONG hunkHeadSize;
		ULONG memoryFlags = MEMF_PUBLIC;
		if (!lseg_read_long(md, &hunkHeadSize)) {
			goto end;
		}
		if ((hunkHeadSize & (HUNKF_CHIP | HUNKF_FAST)) == (HUNKF_CHIP | HUNKF_FAST)) {
			if (!lseg_read_long(md, &memoryFlags)) {
				goto end;
			}
		} else if (hunkHeadSize & HUNKF_CHIP) {
			memoryFlags |= MEMF_CHIP;
		}
		hunkHeadSize &= ~(HUNKF_CHIP | HUNKF_FAST);
		rh->hunkSize = hunkHeadSize;
		rh->hunkData = AllocMem((hunkHeadSize + 2) * sizeof(ULONG), memoryFlags | MEMF_CLEAR);
		if (!rh->hunkData) {
			goto end;
		}
		dbg("hunk %"PRId32": ptr %p, size %"PRId32", memory flags %08"PRIx32"\n", hunkCnt + firstHunk, rh->hunkData, hunkHeadSize, memoryFlags);
		rh->hunkData[0] = rh->hunkSize + 2;
		rh->hunkData[1] = MKBADDR(prevChunk);
		prevChunk = &rh->hunkData[1];
		rh->hunkData += 2;

		if (!firstProcessedHunk) {
			firstProcessedHunk = (APTR)(rh->hunkData - 1);
		}
		hunkCnt++;
	}
	dbg("hunks allocated\n");

	// Load hunks/relocate
	hunkCnt = 0;
	struct RelocHunk *rh = NULL;
	while (hunkCnt <= totalHunks) {
		ULONG hunkType;
		if (!lseg_read_long(md, &hunkType)) {
			if (hunkCnt >= totalHunks) {
				break;  // normal end
			}
			goto end;
		}
		dbg("HUNK %08"PRIx32"\n", hunkType);
		switch(hunkType)
		{
			case HUNK_CODE:
			case HUNK_DATA:
			case HUNK_BSS:
			{
				ULONG hunkSize;
				if (hunkCnt >= totalHunks) {
					goto end;  // overflow
				}
				rh = &relocHunks[hunkCnt++];
				if (!lseg_read_long(md, &hunkSize)) {
					goto end;
				}
				if (hunkSize > rh->hunkSize) {
					goto end;
				}
				if (hunkType != HUNK_BSS) {
					if (!lseg_read_longs(md, hunkSize, rh->hunkData)) {
						goto end;
					}
				}
			}
			break;
			case HUNK_RELOC32:
			case HUNK_RELOC32SHORT:
			{
				ULONG relocCnt, relocHunk;
				if (rh == NULL) {
					goto end;
				}
				for (;;) {
					if (!lseg_read_long(md, &relocCnt)) {
						goto end;
					}
					if (!relocCnt) {
						break;
					}
					if (!lseg_read_long(md, &relocHunk)) {
						goto end;
					}
					relocHunk -= firstHunk;
					if (relocHunk >= totalHunks) {
						goto end;
					}
					dbg("HUNK_RELOC32: relocs %"PRId32" hunk %"PRId32"\n", relocCnt, relocHunk + firstHunk);
					struct RelocHunk *rhr = &relocHunks[relocHunk];
					while (relocCnt != 0) {
						ULONG relocOffset;
						if (hunkType == HUNK_RELOC32SHORT) {
							if (!lseg_read_word(md, &relocOffset)) {
								goto end;
							}
						} else {
							if (!lseg_read_long(md, &relocOffset)) {
								goto end;
							}
						}
						if (relocOffset > (rh->hunkSize - 1) * sizeof(ULONG)) {
							goto end;
						}
						UBYTE *hData = (UBYTE*)rh->hunkData + relocOffset;
						if (relocOffset & 1) {
							// Odd address, 68000/010 support.
							ULONG v = (hData[0] << 24) | (hData[1] << 16) | (hData[2] << 8) | (hData[3] << 0);
							v += (ULONG)rhr->hunkData;
							hData[0] = v >> 24;
							hData[1] = v >> 16;
							hData[2] = v >>  8;
							hData[3] = v >>  0;
						} else {
							*((ULONG*)hData) += (ULONG)rhr->hunkData;
						}
						relocCnt--;
					}
				}
			}
			break;
			case HUNK_END:
			// do nothing
			if (hunkCnt >= totalHunks) {
				ret = 1;  // normal end
				goto end;
			}
			break;
			default:
			dbg("Unexpected HUNK!\n");
			goto end;
		}
	}
		ret = 1;

end:
	if (!ret) {
		dbg("reloc failed\n");
		hunkCnt = 0;
		while (hunkCnt < totalHunks) {
			struct RelocHunk *rh = &relocHunks[hunkCnt];
			if (rh->hunkData) {
				FreeMem(rh->hunkData - 2, (rh->hunkSize + 2) * sizeof(ULONG));
			}
			hunkCnt++;
		}
		firstProcessedHunk = NULL;
	} else {
		cacheclear(md);
		dbg("reloc ok, first hunk %p\n", firstProcessedHunk);
	}

	FreeMem(relocHunks, totalHunks * sizeof(struct RelocHunk));

	return firstProcessedHunk;
}

// Scan FileSystem.resource, create new if it is not found or existing entry has older version number.
static struct FileSysEntry *FSHDProcess(struct FileSysHeaderBlock *fshb, ULONG dostype, ULONG version, BOOL newOnly, struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	struct FileSysEntry *fse = NULL;
	const UBYTE *creator = md->creator ? md->creator : md->zero;
	const char resourceName[] = "FileSystem.resource";

	Forbid();
	
	struct FileSysResource *fsr = NULL;
	fsr = OpenResource(FSRNAME);
	if (!fsr) {
		// FileSystem.resource didn't exist (KS 1.3), create it.
		fsr = AllocMem(sizeof(struct FileSysResource) + strlen(resourceName) + 1 + strlen(creator) + 1, MEMF_PUBLIC | MEMF_CLEAR);
		if (fsr) {

			char *FsResName  = (UBYTE *)(fsr + 1);
			char *CreatorStr = (UBYTE *)FsResName + (strlen(resourceName) + 1);

			//char *CreatorStr = (char *)AllocMem(strlen(creator)+1,MEMF_PUBLIC|MEMF_CLEAR);
			//char *FsResName = (char *)AllocMem(strlen(resourceName)+1,MEMF_PUBLIC|MEMF_CLEAR);

			NewList(&fsr->fsr_FileSysEntries);
			fsr->fsr_Node.ln_Type = NT_RESOURCE;
			strcpy(FsResName, resourceName);
			fsr->fsr_Node.ln_Name = FsResName;
			strcpy(CreatorStr, creator);
			fsr->fsr_Creator = CreatorStr;
			AddTail(&SysBase->ResourceList, &fsr->fsr_Node);
		}
		dbg("FileSystem.resource created %p\n", fsr);
	}
	if (fsr) {
		fse = (struct FileSysEntry*)fsr->fsr_FileSysEntries.lh_Head;
		while (fse->fse_Node.ln_Succ)  {
			if (fse->fse_DosType == dostype) {
				if (fse->fse_Version >= version) {
					// FileSystem.resource filesystem is same or newer, don't update
					if (newOnly) {
						dbg("FileSystem.resource scan: %p dostype %08"PRIx32" found, FSRES version %08"PRIx32" >= FSHD version %08"PRIx32"\n", fse, dostype, fse->fse_Version, version);
						fse = NULL;
					}
					goto end;
				}
			}
			fse = (struct FileSysEntry*)fse->fse_Node.ln_Succ;
		}

		// If the FS wasn't found fse would have been pointing at the last FS in FileSystem.resource
		if (fse->fse_DosType != dostype) fse = NULL;

		if (fshb && newOnly) {
			fse = AllocMem(sizeof(struct FileSysEntry) + strlen(creator) + 1, MEMF_PUBLIC | MEMF_CLEAR);
			if (fse) {
				// Process patchflags
				ULONG *dstPatch = &fse->fse_Type;
				ULONG *srcPatch = &fshb->fhb_Type;
				ULONG patchFlags = fshb->fhb_PatchFlags;
				while (patchFlags) {
					if ((patchFlags & 1) != 0)
						*dstPatch = *srcPatch;
					dstPatch++;
					srcPatch++;
					patchFlags >>= 1;
				}
				fse->fse_DosType = fshb->fhb_DosType;
				fse->fse_Version = fshb->fhb_Version;
				fse->fse_PatchFlags = fshb->fhb_PatchFlags;
				strcpy((UBYTE*)(fse + 1), creator);
				fse->fse_Node.ln_Name = (UBYTE*)(fse + 1);
			}
			dbg("FileSystem.resource scan: dostype %08"PRIx32" not found or old version: created new\n", dostype);
		}
	}
end:
	Permit();
	return fse;
}
// Add new FileSysEntry to FileSystem.resource or free it if filesystem load failed.
static void FSHDAdd(struct FileSysEntry *fse, struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	if (fse->fse_SegList) {
		Forbid();
		struct FileSysResource *fsr = OpenResource(FSRNAME);
		if (fsr) {
			AddHead(&fsr->fsr_FileSysEntries, &fse->fse_Node);
			dbg("FileSysEntry %p added to FileSystem.resource, dostype %08"PRIx32"\n", fse, fse->fse_DosType);
			fse = NULL;
		}
		Permit();
	}
	if (fse) {
		dbg("FileSysEntry %p freed, dostype %08"PRIx32"\n", fse, fse->fse_DosType);
		FreeMem(fse, sizeof(struct FileSysEntry));
	}
}

// Parse FileSystem Header Blocks, load and relocate filesystem if needed.
static struct FileSysEntry *ParseFSHD(UBYTE *buf, ULONG block, ULONG dostype, struct MountData *md)
{
	struct FileSysHeaderBlock *fshb = (struct FileSysHeaderBlock*)buf;
	struct FileSysEntry *fse = NULL;

	for (;;) {
		if (block == 0xffffffff) {
			break;
		}
		if (!readblock(buf, block, IDNAME_FILESYSHEADER, md)) {
			break;
		}
		dbg("FSHD found, block %"PRIu32", dostype %08"PRIx32", looking for dostype %08"PRIx32"\n", block, fshb->fhb_DosType, dostype);
		if (fshb->fhb_DosType == dostype) {
			dbg("FSHD dostype match found\n");
			fse = FSHDProcess(fshb, dostype, fshb->fhb_Version, TRUE, md);
			if (fse) {
				md->lsegblock = fshb->fhb_SegListBlocks;
				md->lsegbuf = (struct LoadSegBlock*)(buf + md->blocksize);
				md->lseglongs = 0;
				APTR seg = fsrelocate(md);
				fse->fse_SegList = MKBADDR(seg);
				// Add to FileSystem.resource if succeeded, delete entry if failure.
				FSHDAdd(fse, md);
			}
			break;
		}
		block = fshb->fhb_Next;
	}
	if (!fse) {
		fse = FSHDProcess(NULL, dostype, 0, FALSE, md);
	}
	return fse;
}

#if NO_CONFIGDEV
// Create fake ConfigDev and DiagArea to support autoboot without requiring real autoconfig device.
static void CreateFakeConfigDev(struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	struct ExpansionBase *ExpansionBase = md->ExpansionBase;
	struct ConfigDev *configDev;

	configDev = AllocConfigDev();
	if (configDev) {
		configDev->cd_BoardAddr = (void*)&entrypoint;
		configDev->cd_BoardSize = (UBYTE*)&entrypoint_end - (UBYTE*)&entrypoint;
		configDev->cd_Rom.er_Type = ERTF_DIAGVALID;
		ULONG bbSize = &bootblock_end - &bootblock;
		ULONG daSize = sizeof(struct DiagArea) + bbSize;
		struct DiagArea *diagArea = AllocMem(daSize, MEMF_CLEAR | MEMF_PUBLIC);
		if (diagArea) {
			diagArea->da_Config = DAC_CONFIGTIME;
			diagArea->da_BootPoint = sizeof(struct DiagArea);
			diagArea->da_Size = (UWORD)daSize;
			copymem(diagArea + 1, &bootblock, bbSize);
			*((ULONG*)&configDev->cd_Rom.er_Reserved0c) = (ULONG)diagArea;
			cacheclear(md);
		}
		md->configDev = configDev;
	}
}
#endif

struct ParameterPacket
{
	const UBYTE *dosname;
	const UBYTE *execname;
	ULONG unitnum;
	ULONG flags;
	struct DosEnvec de;
};

static UBYTE ToUpper(UBYTE c)
{
	if (c >= 'a' || c <= 'z') {
		c |= 0x20;
	}
	return c;
}

// Case-insensitive BSTR string comparison
static BOOL CompareBSTRNoCase(const UBYTE *src1, const UBYTE *src2)
{
	UBYTE len1 = *src1++;
	UBYTE len2 = *src2++;
	if (len1 != len2) {
		return FALSE;
	}
	for (UWORD i = 0; i < len1; i++) {
		UBYTE c1 = *src1++;
		UBYTE c2 = *src2++;
		c1 = ToUpper(c1);
		c2 = ToUpper(c2);
		if (c1 != c2) {
			return FALSE;
		}
	}
	return TRUE;
}

// Check for duplicate device names
static void CheckAndFixDevName(struct MountData *md, UBYTE *bname)
{
	struct ExecBase *SysBase = md->SysBase;

	Forbid();
	struct BootNode *bn = (struct BootNode*)md->ExpansionBase->MountList.lh_Head;
	while (bn->bn_Node.ln_Succ) {
		struct DeviceNode *dn = bn->bn_DeviceNode;
		const UBYTE *bname2 = BADDR(dn->dn_Name);
		if (CompareBSTRNoCase(bname, bname2)) {
			UBYTE len = bname[0] > 30 ? 30 : bname[0];
			UBYTE *name = bname + 1;
			dbg("Duplicate device name '%s'\n", name);
			if (len > 2 && name[len - 2] == '.' && name[len - 1] >= '0' && name[len - 1] < '9') {
				// if already ends to .<digit>: increase digit by one
				name[len - 1]++;
			} else {
				// else append .1
				name[len++] = '.';
				name[len++] = '1';
				name[len] = 0;
				bname[0] += 2;
			}
			dbg("-> new device name '%s'\n", name);
			// retry
			bn = (struct BootNode*)md->ExpansionBase->MountList.lh_Head;
			continue;
		}
		bn = (struct BootNode*)bn->bn_Node.ln_Succ;
	}
	Permit();
}

// Add DeviceNode to Expansion MountList.
static void AddNode(struct PartitionBlock *part, struct ParameterPacket *pp, struct DeviceNode *dn, UBYTE *name, struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	struct ExpansionBase *ExpansionBase = md->ExpansionBase;
	struct DosLibrary *DOSBase = md->DOSBase;

	LONG bootPri = (part->pb_Flags & PBFF_BOOTABLE) ? pp->de.de_BootPri : -128;
	if (ExpansionBase->LibNode.lib_Version >= 37) {
		// KS 2.0+
		if (!md->DOSBase && bootPri > -128) {
			dbg("KS20+ Mounting as bootable: pri %08"PRIx32"\n", bootPri);
			AddBootNode(bootPri, ADNF_STARTPROC, dn, md->configDev);
		} else {
			dbg("KS20+: Mounting as non-bootable\n");
			AddDosNode(bootPri, ADNF_STARTPROC, dn);
		}
	} else {
		// KS 1.3
		if (!md->DOSBase && bootPri > -128) {
			dbg("KS13 Mounting as bootable: pri %08"PRIx32"\n", bootPri);
			// Create and insert bootnode manually.
			struct BootNode *bn = AllocMem(sizeof(struct BootNode), MEMF_CLEAR | MEMF_PUBLIC);
			if (bn) {
				bn->bn_Node.ln_Type = NT_BOOTNODE;
				bn->bn_Node.ln_Pri = (BYTE)bootPri;
				bn->bn_Node.ln_Name = (UBYTE*)md->configDev;
				bn->bn_DeviceNode = dn;
				Forbid();
				Enqueue(&md->ExpansionBase->MountList, &bn->bn_Node);
				Permit();
			}
		} else {
			dbg("KS13: Mounting as non-bootable\n");
			AddDosNode(bootPri, 0, dn);
			if (md->DOSBase) {
				// KS 1.3 ADNF_STARTPROC is not supported
				// need to use DeviceProc() to start the filesystem process.
				UWORD len = strlen(name);
				name[len++] = ':';
				name[len] = 0;
				void *mp = DeviceProc(name);
				dbg("DeviceProc() returned %p\n", mp);
			}
		}
	}
}

// Parse PART block, mount drive.
static ULONG ParsePART(UBYTE *buf, ULONG block, ULONG filesysblock, struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	struct ExpansionBase *ExpansionBase = md->ExpansionBase;
	struct PartitionBlock *part = (struct PartitionBlock*)buf;
	ULONG nextpartblock = 0xffffffff;

	if (!readblock(buf, block, IDNAME_PARTITION, md)) {
		return nextpartblock;
	}
	dbg("PART found, block %"PRIu32"\n", block);
	nextpartblock = part->pb_Next;
	if (!(part->pb_Flags & PBFF_NOMOUNT)) {
		struct ParameterPacket *pp = AllocMem(sizeof(struct ParameterPacket), MEMF_PUBLIC | MEMF_CLEAR);
		if (pp) {
			UBYTE len;
			copymem(&pp->de, &part->pb_Environment, (part->pb_Environment[0] + 1) * sizeof(ULONG));
			struct FileSysEntry *fse = ParseFSHD(buf + md->blocksize, filesysblock, pp->de.de_DosType, md);
			pp->execname = md->devicename;
			pp->unitnum = md->unitnum;
			pp->dosname = part->pb_DriveName + 1;
			len=(*part->pb_DriveName) > 30 ? 30 : (*part->pb_DriveName);
			part->pb_DriveName[len + 1] = 0;
			dbg("PART '%s'\n", pp->dosname);
			CheckAndFixDevName(md, part->pb_DriveName);
			struct DeviceNode *dn = MakeDosNode(pp);
			if (dn) {
				if (fse) {
					// Process PatchFlags
					ULONG *dstPatch = &dn->dn_Type;
					ULONG *srcPatch = &fse->fse_Type;
					ULONG patchFlags = fse->fse_PatchFlags;
					while (patchFlags) {
						if (patchFlags & 1) {
							*dstPatch = *srcPatch;
						}
						patchFlags >>= 1;
						srcPatch++;
						dstPatch++;
					}
					dbg("Mounting partition\n");
#if NO_CONFIGDEV
					if (!md->configDev && !md->DOSBase) {
						CreateFakeConfigDev(md);
					}
#endif
				}
				AddNode(part, pp, dn, part->pb_DriveName + 1, md);
				md->ret++;
			} else {
				dbg("Device node creation failed\n");
			}
			FreeMem(pp, sizeof(struct ParameterPacket));
		}
	}
	return nextpartblock;
}

// Scan PART blocks
static LONG ParseRDSK(UBYTE *buf, struct MountData *md)
{
	struct RigidDiskBlock *rdb = (struct RigidDiskBlock*)buf;
	ULONG partblock = rdb->rdb_PartitionList;
	ULONG filesysblock = rdb->rdb_FileSysHeaderList;
	ULONG flags = rdb->rdb_Flags;
	for (;;) {
		if (partblock == 0xffffffff) {
			break;
		}
		partblock = ParsePART(buf, partblock, filesysblock, md);
	}
	md->wasLastDev = (flags & RDBFF_LAST) != 0;
	return md->ret;
}

// Search for RDB
static LONG ScanRDSK(struct MountData *md)
{
	struct ExecBase *SysBase = md->SysBase;
	LONG ret = -1;
	for (UWORD i = 0; i < RDB_LOCATION_LIMIT; i++) {
		if (readblock(md->buf, i, 0xffffffff, md)) {
			struct RigidDiskBlock *rdb = (struct RigidDiskBlock*)md->buf;
			if (rdb->rdb_ID == IDNAME_RIGIDDISK) {
				dbg("RDB found, block %"PRIu32"\n", i);
				ret = ParseRDSK(md->buf, md);
				break;
			}
		}
	}
	md->request->iotd_Req.io_Command = TD_MOTOR;
	md->request->iotd_Req.io_Length  = 0;
	DoIO((struct IORequest*)md->request);
	return ret;
}
#if CDBOOT
static struct FileSysEntry *scan_filesystems(void)
{
	struct FileSysEntry *fse, *cdfs=NULL;

	/* NOTE - you should actually be in a Forbid while accessing any
	 * system list for which no other method of arbitration is available.
	 * However, for this example we will be printing the information
	 * (which would break a Forbid anyway) so we won't Forbid.
	 * In real life, you should Forbid, copy the information you need,
	 * Permit, then print the info.
	 */
	if (!(FileSysResBase = (struct FileSysResource *)OpenResource(FSRNAME))) {
		dbg("Cannot open %s\n",FSRNAME);
	} else {
		dbg("DosType   Version   Creator\n");
		dbg("------------------------------------------------\n");
		for ( fse = (struct FileSysEntry *)FileSysResBase->fsr_FileSysEntries.lh_Head;
			  fse->fse_Node.ln_Succ;
			  fse = (struct FileSysEntry *)fse->fse_Node.ln_Succ) {
#ifdef DEBUG_MOUNTER
			int x;
			for (x=24; x>=8; x-=8)
				putchar((fse->fse_DosType >> x) & 0xFF);

			putchar((fse->fse_DosType & 0xFF) < 0x30
							? (fse->fse_DosType & 0xFF) + 0x30
							: (fse->fse_DosType & 0xFF));
#endif
			dbg("	  %s%d",(fse->fse_Version >> 16)<10 ? " " : "", (fse->fse_Version >> 16));
			dbg(".%d%s",(fse->fse_Version & 0xFFFF), (fse->fse_Version & 0xFFFF)<10 ? " " : "");
			dbg("	 %s",fse->fse_Node.ln_Name);

			if (fse->fse_DosType==0x43443031) {
				cdfs=fse;
#ifndef ALL_FILESYSTEMS
				break;
#endif
			}
		}

	}
	return cdfs;
}

// Search for Bootable CDROM
static LONG ScanCDROM(struct MountData *md)
{
	struct ExpansionBase *ExpansionBase = md->ExpansionBase;
	struct FileSysEntry *fse=NULL;
	char dosName[] = "CD0";
	static unsigned int cnt = 0;
	LONG bootPri;

	// "CDTV" or "AMIGA BOOT"?
	if (CheckPVD((struct IOStdReq *)md->request)) {
		bootPri = 2;  // Yes, give priority
	} else {
		bootPri = -1; // May not be a boot disk, lower priority than HDD
	}

	struct ParameterPacket pp;

	memset(&pp,0,sizeof(struct ParameterPacket));

	pp.dosname              = dosName;
	pp.execname             = md->devicename;
	pp.unitnum              = md->unitnum;
	pp.de.de_TableSize      = sizeof(struct DosEnvec);
	pp.de.de_SizeBlock      = 2048 >> 2;
	pp.de.de_Surfaces       = 1;
	pp.de.de_SectorPerBlock = 1;
	pp.de.de_BlocksPerTrack = 1;
	pp.de.de_NumBuffers     = 5;
	pp.de.de_BufMemType     = MEMF_ANY|MEMF_CLEAR;
	pp.de.de_MaxTransfer    = 0x100000;
	pp.de.de_Mask           = 0x7FFFFFFE;
	pp.de.de_DosType        = 0x43443031; // CD01
	pp.de.de_BootPri        = bootPri;

	fse=scan_filesystems();
	if (!fse) {
		// printf("Could not load filesystem\n");
		return -1;
	}

	dosName[2]='0' + cnt;
	struct DeviceNode *node = MakeDosNode(&pp);
	if (!node) {
		// printf("Could not create DosNode\n");
		return -1;
	}

	// TODO some consistency check that this is actually
	// a bootable Amiga CDROM
	// - iso toc
	// - CDTV or CD32 disk

	// Process PatchFlags.
	ULONG *dstPatch = &node->dn_Type;
	ULONG *srcPatch = &fse->fse_Type;
	ULONG patchFlags = fse->fse_PatchFlags;
	while (patchFlags) {
		if (patchFlags & 1) {
			*dstPatch = *srcPatch;
		}
		patchFlags >>= 1;
		srcPatch++;
		dstPatch++;
	}

	AddBootNode(bootPri, ADNF_STARTPROC, node, md->configDev);
	cnt++;

	return 1;
}

#endif

// Mount drives
LONG MountDrive(struct MountStruct *ms)
{
	LONG  ret = -1;
	ULONG uidx = 0;
	int   blockSize = 0;
	struct UnitStruct *unit;
	struct MsgPort *port = NULL;
	struct IOExtTD *request = NULL;
	struct ExpansionBase *ExpansionBase;
	struct ExecBase *SysBase = ms->SysBase;

	dbg("Starting..\n");
	ExpansionBase = (struct ExpansionBase*)OpenLibrary("expansion.library", 34);
	if (ExpansionBase) {
		struct MountData *md = AllocMem(sizeof(struct MountData), MEMF_CLEAR | MEMF_PUBLIC);
		if (md) {
			md->DOSBase = (struct DosLibrary*)OpenLibrary("dos.library", 34);
			md->SysBase = SysBase;
			md->ExpansionBase = ExpansionBase;
			dbg("SysBase=%p ExpansionBase=%p DosBase=%p\n", md->SysBase, md->ExpansionBase, md->DOSBase);
			md->creator = ms->creatorName;
			port = W_CreateMsgPort(SysBase);
			if(port) {
				request = (struct IOExtTD*)W_CreateIORequest(port, sizeof(struct IOExtTD), SysBase);
				if(request) {
					UWORD unitNumCnt = ms->numUnits;
					while (unitNumCnt-- > 0) {
						unit = &ms->Units[uidx++];
						dbg("OpenDevice('%s', %"PRId32", %p, 0)\n", ms->deviceName, unit->unitNum, request);
						UBYTE err = OpenDevice(ms->deviceName, unit->unitNum, (struct IORequest*)request, 0);
						if (err == 0) {
							if ((blockSize = GetBlockSize((struct IOStdReq *)request)) > 0) {
								ret = -1;
								md->request    = request;
								md->devicename = ms->deviceName;
								md->unitnum    = unit->unitNum;
								md->blocksize  = blockSize;
								md->configDev  = unit->configDev;
#if CDBOOT
								if (unit->deviceType == DG_CDROM) {
									ret = ScanCDROM(md);
								} else {
									ret = ScanRDSK(md);
								}
#else
								ret = ScanRDSK(md);
#endif
								CloseDevice((struct IORequest*)request);

#ifndef NO_RDBLAST
								if (md->wasLastDev) {
									dbg("RDBFF_LAST exit\n");
									break;
								}
#endif
							} else {
								dbg("Couldn't get block size\n");
							}
						} else {
							dbg("OpenDevice(%s,%"PRId32") failed: %"PRId32"\n", ms->deviceName, unit->unitNum, (BYTE)err);
						}
					}
					W_DeleteIORequest(request, SysBase);
				}
				W_DeleteMsgPort(port, SysBase);
			}
			if (md->DOSBase) {
				CloseLibrary(&md->DOSBase->dl_lib);
			}
			FreeMem(md, sizeof(struct MountData));
		}
		CloseLibrary(&ExpansionBase->LibNode);
	}
	dbg("Exit code %"PRId32"\n", ret);
	return ret;
}