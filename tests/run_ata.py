#!/usr/bin/env python3
"""Exercise production ATA control flow with host-side MMIO/Exec mocks.

The functions are extracted from the driver, with only register reads and
architecture-specific delay helpers intercepted. This is not a timing or
hardware test. Run: python3 tests/run_ata.py
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(filename, name):
    source = (ROOT / filename).read_text()
    match = re.search(r"^(?:static )?(?:inline )?(?:bool|BYTE|void) "
                      + name + r"\([^\n]*\)\s*\{", source, re.M)
    assert match, name
    end = source.index("\n}", match.end()) + 2
    body = source[match.start():end]
    body = body.replace("*unit->drive.status_command", "read_status()")
    body = body.replace("*unit->drive.alt_status", "read_status()")
    line = source.count("\n", 0, match.start()) + 1
    return f'#line {line} "{filename}"\n{body}\n'


preamble = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int8_t BYTE;
typedef uint8_t UBYTE;
typedef uint16_t UWORD;
typedef uintptr_t ULONG;
#define Trace(...) ((void)0)
#define ata_flag_busy 128
#define ata_flag_ready 64
#define ata_flag_df 32
#define ata_flag_drq 8
#define ata_flag_error 1
#define TF_IRQ_POLL_LOOPS 100
#define ATA_DRQ_WAIT_LOOP_US 1000
#define ATA_DRQ_WAIT_S 5
#define ATA_DRQ_WAIT_COUNT 3
#define ATA_BSY_WAIT_COUNT 3
#define ATA_BSY_WAIT_LOOP_US 1000
#define ATA_RDY_WAIT_COUNT 3
#define MAX_TRANSFER_SECTORS 256
#define ATA_CMD_READ 0x20
#define ATA_CMD_READ_EXT 0x24
#define ATA_CMD_READ_MULTIPLE 0xc4
#define ATA_CMD_READ_MULTIPLE_EXT 0x29
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_WRITE_EXT 0x34
#define ATA_CMD_WRITE_MULTIPLE 0xc5
#define ATA_CMD_WRITE_MULTIPLE_EXT 0x39
#define ATA_NODATA 3
#define ATA_PIO_IN 4
#define ATA_PIO_OUT 5
#define ATA_BYT_BLOK 4
#define ATA_TLEN_MASK 3
#define IOERR_BADLENGTH -4
#define IOERR_BADADDRESS -5
#define IOERR_NOCMD -3
#define IOERR_UNITBUSY -6
#define IOERR_ABORTED -2
#define HFERR_SelTimeout 44
#define HFERR_BadStatus 45
#define TDERR_NotSpecified 20
#define TR_ADDREQUEST 9
struct ExecBase { int unused; };
struct MsgPort { UBYTE mp_SigBit; };
struct Message { struct MsgPort *mn_ReplyPort; };
struct IORequest { struct Message io_Message; int io_Command; };
struct timerequest {
    struct IORequest tr_node;
    struct { ULONG tv_sec, tv_micro; } tr_time;
};
struct IOTask { struct timerequest *tr; bool irqInstalled; ULONG irqSigMask; };
typedef void (*ata_xfer_func)(void *, void *);
struct IDEUnit {
    struct ExecBase *SysBase;
    struct IOTask *itask;
    struct { volatile UWORD *data; } drive;
    struct { bool primary, lba48, xferMultiple; } flags;
    UBYTE multipleCount;
    UWORD blockSize;
    ata_xfer_func write_fast, write_unaligned, read_fast, read_unaligned;
    BYTE (*write_taskfile)(struct IDEUnit *, UBYTE, uint64_t, UBYTE, UBYTE);
};
enum xfer_dir { READ, WRITE };
struct SCSI_CDB_ATA {
    UBYTE operation, protocol, length, features, sectorCount, lbaLow,
          lbaMid, lbaHigh, devHead, command, reserved, control;
};
struct SCSICmd {
    UBYTE *scsi_Command;
    UWORD *scsi_Data;
    ULONG scsi_Length, scsi_Actual;
    UWORD scsi_CmdLength, scsi_CmdActual;
};
static UBYTE status, final_status, command, features, sector_count;
static unsigned reads, transition_read, command_count, data_count, left;
static UBYTE transition_status;
static unsigned fail_command;
static bool passthrough;
static UBYTE read_status(void) {
    if (++reads == transition_read) status = transition_status;
    return status;
}
static void ata_tf_settle(void) {}
static void ata_status_reg_delay(struct IDEUnit *u) { (void)u; }
static void atapi_status_reg_delay(void) {}
static void sleep_us(struct timerequest *t, ULONG us) { (void)t; (void)us; }
static void SendIO(struct IORequest *r) { (void)r; }
static struct IORequest *CheckIO(struct IORequest *r) { return r; }
static void AbortIO(struct IORequest *r) { (void)r; }
static void WaitIO(struct IORequest *r) { (void)r; }
static ULONG Wait(ULONG mask) { return mask & 1; }
static ULONG SetSignal(ULONG v, ULONG m) { (void)v; (void)m; return 0; }
static bool ata_select(struct IDEUnit *u, UBYTE s, bool w) {
    (void)u; (void)s; (void)w; return false;
}
static bool ata_wait_ready(struct IDEUnit *u, ULONG tries) {
    (void)u; (void)tries; return true;
}
static void ata_save_error(struct IDEUnit *u) { (void)u; }
static bool ata_check_error(struct IDEUnit *u) {
    (void)u; return status & (ata_flag_error | ata_flag_df);
}
static void ata_irq_enable(struct IDEUnit *u, bool e) { (void)u; (void)e; }
static BYTE write_taskfile_lba(struct IDEUnit *u, UBYTE c, uint64_t lba,
                              UBYTE count, UBYTE f) {
    (void)u; (void)lba;
    command=c; sector_count=count; features=f; ++command_count;
    left=count ? count : 256;
    status=ata_flag_drq;
    if (passthrough) { transition_read=reads+2; transition_status=0; }
    return 0;
}
static void transfer_sector(void *src, void *dst) {
    (void)src; (void)dst; ++data_count;
    if (--left == 0) status = fail_command == command_count ? final_status : 0;
}
'''

tests = r'''
#line 1 "ata-tests"
static void reset(void) {
    status=reads=transition_read=command_count=data_count=fail_command=0;
    passthrough=false;
}
int main(void) {
    struct MsgPort port={.mp_SigBit=0};
    struct timerequest timer={.tr_node.io_Message.mn_ReplyPort=&port};
    struct IOTask task={.tr=&timer, .irqSigMask=2};
    UWORD mmio[256], buffer[257*256];
    struct IDEUnit unit={.itask=&task, .drive.data=mmio,
        .flags.primary=true, .multipleCount=1, .blockSize=512,
        .write_fast=transfer_sector, .write_unaligned=transfer_sector,
        .read_fast=transfer_sector, .read_unaligned=transfer_sector,
        .write_taskfile=write_taskfile_lba};
    /* Status bits other than BSY are undefined while BSY is set. */
    const UBYTE states[]={0x08,0x88,0x09,0x28,0x80,0x81,0xa0};
    for (unsigned i=0; i<sizeof(states); i++) {
        reset(); status=states[i];
        assert(ata_wait_drq(&unit,1,false) == (states[i]==8));
        assert(atapi_wait_drq(&unit,1) == (states[i]==8));
#ifdef TF_IDE
        assert(ata_tf_wait_status_irq(&unit,true) == (states[i]==8));
#endif
    }
    reset(); status=0x89; transition_read=3; transition_status=8;
    assert(ata_wait_drq(&unit,1,false) && reads==3);
    puts("PASS: ATA/ATAPI readiness rejects busy/error DRQ and waits for BSY");

    for (unsigned dir=0; dir<2; dir++) {
        reset(); passthrough=true;
        for (unsigned i=0;i<256;i++) { mmio[i]=0xa000+i; buffer[i]=0xb000+i; }
        struct SCSI_CDB_ATA cdb={.protocol=(dir?ATA_PIO_OUT:ATA_PIO_IN)<<1,
            .length=ATA_BYT_BLOK|2, .sectorCount=1, .features=0x55, .command=0xec};
        struct SCSI_CDB_ATA original=cdb;
        struct SCSICmd cmd={.scsi_Command=(UBYTE *)&cdb, .scsi_Data=buffer,
            .scsi_Length=512, .scsi_CmdLength=12};
        assert(scsi_ata_passthrough(&unit,&cmd)==0 && cmd.scsi_Actual==512);
        assert(sector_count==1 && features==0x55 && !memcmp(&cdb,&original,sizeof cdb));
        for (unsigned i=1;i<256;i++) assert(mmio[i]==0xa000+i);
        if (dir) assert(mmio[0]==0xb0ff);
        else for (unsigned i=0;i<256;i++) assert(buffer[i]==0xa000);
        reset(); passthrough=true; cdb.length=2; cdb.sectorCount=3;
        assert(scsi_ata_passthrough(&unit,&cmd)==IOERR_BADLENGTH && command_count==0);
    }
    puts("PASS: passthrough uses fixed port, preserves CDB and bounds odd lengths");

    /* Complete every command before counting a successful write. */
    const UBYTE endings[]={0,0x80,1,0x20,8};
    for (unsigned irq=0;irq<2;irq++) {
        task.irqInstalled=irq;
        for (unsigned i=0;i<sizeof(endings);i++) {
            reset(); final_status=endings[i]; fail_command=1;
            ULONG actual=999;
            BYTE error=ata_readwrite(buffer,0,1,&actual,&unit,WRITE);
            assert((error==0)==(endings[i]==0));
            assert(actual==(endings[i]==0?512:0));
            assert(data_count==1);
        }
        reset(); final_status=1; fail_command=2;
        ULONG actual=999;
        assert(ata_readwrite(buffer,0,257,&actual,&unit,WRITE)!=0);
        assert(actual==256*512 && command_count==2);
    }
    puts("PASS: final write BSY/ERR/DF/DRQ and partial command byte counts");

    /* Ordinary vs multiple, LBA28 vs LBA48, read vs write. */
    for (unsigned ext=0;ext<2;ext++) for (unsigned multiple=0;multiple<2;multiple++) {
        unit.flags.lba48=ext; unit.flags.xferMultiple=multiple;
        unit.multipleCount=multiple?4:1;
        for (unsigned dir=0;dir<2;dir++) {
            reset(); ULONG actual;
            assert(ata_readwrite(buffer,0,1,&actual,&unit,dir)==0 && actual==512);
            const UBYTE expected[2][2][2]={{{0x20,0x30},{0xc4,0xc5}},{{0x24,0x34},{0x29,0x39}}};
            assert(command==expected[ext][multiple][dir]);
        }
    }
    puts("PASS: LBA28/LBA48 ordinary and multiple command selection");
    return 0;
}
'''

names = ["ata_wait_drq", "ata_tf_wait_status_irq", "ata_wait_data",
         "ata_wait_complete", "ata_read", "ata_write", "ata_readwrite",
         "scsi_ata_passthrough"]
with tempfile.TemporaryDirectory(prefix="lide-ata-") as directory:
    path = Path(directory)
    for tf in (False, True):
        selected = [n for n in names if tf or n != "ata_tf_wait_status_irq"]
        source = preamble + "\n".join(function("ata.c", n) for n in selected)
        source += function("atapi.c", "atapi_wait_drq") + tests
        (path / "test.c").write_text(source)
        command = [os.environ.get("CC", "cc"), "-std=gnu11", "-O2", "-g",
                   "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        if tf:
            command.append("-DTF_IDE=1")
        subprocess.run(command + [str(path / "test.c"), "-o", str(path / "test")], check=True)
        print("TF" if tf else "Generic", flush=True)
        subprocess.run([str(path / "test")], check=True)
