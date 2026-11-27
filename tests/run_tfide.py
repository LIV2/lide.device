#!/usr/bin/env python3
"""Check TF4060 discovery, timing and mover MMIO sequences on the host.

Production helpers are extracted verbatim except that byte register writes
and the completion read are recorded. Exec and ConfigDev calls are mocked;
these checks do not model FPGA timing or physical data transfers.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "tfide.c").read_text()
header = re.sub(r"^#include.*\n", "", (ROOT / "tfide.h").read_text(), flags=re.M)
body = source[source.index("/* TF4060 registers"):source.rindex("#endif")]
body = re.sub(r"tf->board\[([^\]]+)\]\s*=\s*([^;]+);",
              r"write_byte(tf,\1,\2);", body)
body = re.sub(r"registers\[([^\]]+)\]\s*=\s*([^;]+);",
              r"write_byte(tf,TF4060_READ_DEST + \1,\2);", body)
body = body.replace("(void)*sync", "complete_fragment(tf)")

preamble = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UBYTE;
typedef uint16_t UWORD;
typedef uintptr_t ULONG;
typedef void *APTR;
#define Info(...) ((void)0)
#define CDF_SHUTUP 1
#define ERT_TYPEMASK 0xc0
#define ERT_ZORROII 0xc0
#define CHANNEL_1 0x1000
#define ata_reg_altStatus 0x18
#define nIEN 2
#define ata_identify_field_valid 53
#define ata_identify_pio_modes 64
#define ata_identify_features 83
#define ata_field_valid_pio 2
#define ata_pio_mode_4 2
#define MAX_TRANSFER_SECTORS 256
#define DMA_NoModify 4
static UBYTE legacy[0xc000];
#define BOARD_BASE ((ULONG)legacy)
struct Library { UWORD lib_Version; };
struct ExecBase { struct Library LibNode; };
struct ConfigDev {
    UBYTE cd_Flags;
    struct { UBYTE er_Type; } cd_Rom;
    APTR cd_BoardAddr;
    ULONG cd_BoardSize;
};
'''

mocks = r'''
struct IOTask { struct TFController tf; };
enum xfer { longword_movem, longword_move, word_move, tf_block_move };
struct IDEUnit { struct IOTask *itask; struct ExecBase *SysBase; enum xfer xferMethod; };
static struct ConfigDev config;
static bool found_exact, found_legacy;
static struct ConfigDev *FindConfigDev(void *unused, int manufacturer, int product) {
    (void)unused; assert(manufacturer==5080);
    if (product==0x94) return found_exact?&config:NULL;
    assert(product==-1); return found_legacy?&config:NULL;
}
static ULONG pre_length, post_length, post_flags, shorten;
static APTR pre_address, post_address;
static bool translate;
static unsigned pre_calls, post_calls, writes, chunks;
static ULONG chunk_address[1024], chunk_words[1024];
static ULONG write_offset[8192];
static UBYTE write_value[8192];
static APTR CachePreDMA(APTR address, ULONG *length, ULONG flags) {
    assert(flags==0); ++pre_calls; pre_address=address; pre_length=*length;
    if (shorten) *length=shorten;
    return translate?(APTR)((ULONG)address+0x1000):address;
}
static void CachePostDMA(APTR address, ULONG *length, ULONG flags) {
    ++post_calls; post_address=address; post_length=*length; post_flags=flags;
}
static void write_byte(struct TFController *tf, ULONG offset, UBYTE value) {
    assert(writes<8192);
    write_offset[writes]=offset; write_value[writes++]=value;
    tf->board[offset]=value;
}
static void complete_fragment(struct TFController *tf) {
    assert(pre_calls==1 && post_calls==0);
    assert((*tf->intena & 0x80)==0);
    assert(writes==(chunks+1)*6);
    const unsigned offsets[]={0xb4,0xb5,0xb6,0xb7,0xb8,0xb9};
    for (unsigned i=0;i<6;i++) assert(write_offset[chunks*6+i]==offsets[i]);
    ULONG a=((ULONG)tf->board[0xb4]<<24)|((ULONG)tf->board[0xb5]<<16)|
        ((ULONG)tf->board[0xb6]<<8)|tf->board[0xb7];
    ULONG n=((ULONG)tf->board[0xb8]<<8)|tf->board[0xb9];
    assert(n>0 && n<=256 && (a&0xffff)+n*2<=0x10000);
    chunk_address[chunks]=a; chunk_words[chunks++]=n;
}
static void reset_trace(void) {
    pre_calls=post_calls=writes=chunks=0;
    translate=false; shorten=0;
}
'''

tests = r'''
int main(void) {
    static UBYTE board[0x10000];
    struct IOTask task={0};
    struct TFController *tf=&task.tf;
    struct ExecBase exec={.LibNode.lib_Version=37};
    struct IDEUnit unit={.itask=&task, .SysBase=&exec, .xferMethod=tf_block_move};
    config=(struct ConfigDev){.cd_Rom.er_Type=0xc0,.cd_BoardAddr=board,.cd_BoardSize=0x10000};
    found_exact=true;
    board[0x5c]=3; board[0x68]=0x82; board[0xe000]=0xa5;
    assert(tf_ide_init(NULL,tf));
    assert(tf->ide==board+0x4000 && tf->intchg==board+0xd000 && tf->intena==board+0xe000);
    assert(board[0x5018]==2 && board[0xe000]==0x25);
    assert(tf->caps==3 && tf->timingSaved && tf->savedTiming==0x82);
    const unsigned bad_size[]={0,0xe000};
    for (unsigned i=0;i<2;i++) { config.cd_BoardSize=bad_size[i]; assert(!tf_ide_init(NULL,tf)); }
    config.cd_BoardSize=0x10000;
    config.cd_Flags=1; assert(!tf_ide_init(NULL,tf)); config.cd_Flags=0;
    config.cd_Rom.er_Type=0x80; assert(!tf_ide_init(NULL,tf)); config.cd_Rom.er_Type=0xc0;
    config.cd_BoardAddr=NULL; assert(!tf_ide_init(NULL,tf)); config.cd_BoardAddr=board;
    struct TFController old={0};
    found_exact=false; assert(!tf_ide_init(NULL,&old));
    found_legacy=true; assert(tf_ide_init(NULL,&old));
    assert(old.ide==legacy && old.intchg==legacy+0x9000 && old.intena==legacy+0xa000);
    assert(!old.board && !old.caps && !old.timingSaved);
    found_exact=true; board[0x5c]=0xff; old=(struct TFController){0};
    assert(tf_ide_init(NULL,&old) && !old.caps && !old.timingSaved);
    board[0x5c]=3;
    puts("PASS: configured mapping, invalid ConfigDev and legacy capability isolation");

    UWORD id[256]={0};
    assert(tf_drive_timing(id,3)==0);
    id[53]=2; id[64]=2;
    assert(tf_drive_timing(id,0)==0 && tf_drive_timing(id,1)==1 && tf_drive_timing(id,3)==1);
    id[0]=0x848a; id[163]=2;
    assert(tf_drive_timing(id,1)==1 && tf_drive_timing(id,3)==2);
    id[0]=0; id[83]=0x4004;
    assert(tf_drive_timing(id,3)==2);
    id[83]=4; assert(tf_drive_timing(id,3)==1);
    id[83]=0x4004; id[163]=1; assert(tf_drive_timing(id,3)==1);
    id[64]=1; assert(tf_drive_timing(id,3)==0);
    tf->timing[0]=2; tf->timing[1]=0xff;
    tf_apply_timing(tf); assert(board[0x68]==0x82); /* Probe both units first. */
    tf->timingReady=true; tf_apply_timing(tf); assert(board[0x68]==0x8f);
    tf->timing[1]=1; tf_apply_timing(tf); assert(board[0x68]==0x87);
    tf->timing[1]=0; tf_apply_timing(tf); assert(board[0x68]==0x82);
    tf->timing[1]=2; tf_apply_timing(tf); tf_restore_timing(tf); assert(board[0x68]==0x82);
    puts("PASS: timing capabilities, CFA validity, shared minimum and restoration");

    /* Each completion read follows six byte writes; fragments never cross 64 KiB. */
    const ULONG addresses[]={0x08000000,0x0800fffe,0x0800ff00,0x0800fe00,0x17fffe00};
    for (unsigned i=0;i<sizeof(addresses)/sizeof(addresses[0]);i++) {
        for (unsigned enabled=0;enabled<2;enabled++) {
            reset_trace(); *tf->intena=0x25|(enabled?0x80:0);
            assert(tf_read_block(&unit,(void *)addresses[i],1));
            ULONG a=addresses[i], total=0;
            for (unsigned j=0;j<chunks;j++) {
                assert(chunk_address[j]==a); a+=chunk_words[j]*2; total+=chunk_words[j]*2;
            }
            assert(total==512 && a==addresses[i]+512);
            assert(pre_calls==1 && post_calls==1 && pre_length==512 && post_length==512);
            assert(pre_address==(void *)addresses[i] && post_address==pre_address && !post_flags);
            assert(*tf->intena==(0x25|(enabled?0x80:0)));
        }
    }
    reset_trace(); assert(tf_read_block(&unit,(void *)0x0800fffe,16));
    assert(pre_calls==1 && post_calls==1 && pre_length==8192 && post_length==8192);
    ULONG next=0x0800fffe;
    for (unsigned j=0;j<chunks;j++) { assert(chunk_address[j]==next); next+=chunk_words[j]*2; }
    assert(next == 0x0800fffe + 8192);
    puts("PASS: mover byte order, completion reads, sector and 64-KiB boundaries");

    const ULONG invalid[]={0x07fffffe,0x08000001,0x17fffe02,0x18000000,0xfffffffe};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        reset_trace(); assert(!tf_read_block(&unit,(void *)invalid[i],1));
        assert(!pre_calls && !post_calls && !writes);
    }
    reset_trace(); assert(!tf_read_block(&unit,(void *)0x08000000,0));
    assert(!tf_read_block(&unit,(void *)0x08000000,257));
    exec.LibNode.lib_Version=36; assert(!tf_read_block(&unit,(void *)0x08000000,1));
    exec.LibNode.lib_Version=37; unit.xferMethod=word_move;
    assert(!tf_read_block(&unit,(void *)0x08000000,1)); unit.xferMethod=tf_block_move;
    tf->caps=2; assert(!tf_read_block(&unit,(void *)0x08000000,1)); tf->caps=3;
    assert(!pre_calls && !writes);
    for (unsigned translated=0;translated<2;translated++) {
        reset_trace(); translate=translated; if (!translated) shorten=256;
        assert(!tf_read_block(&unit,(void *)0x08000000,1));
        assert(pre_calls==1 && post_calls==1 && post_flags==DMA_NoModify && !writes && !chunks);
        assert(pre_address==post_address && pre_length==post_length);
    }
    puts("PASS: RAM/alignment/capability/OS/MMU fallback and balanced cache calls");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="lide-tfide-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(preamble + header + mocks + body + tests)
    subprocess.run([os.environ.get("HOSTCC", "cc"), "-std=gnu11", "-O2", "-g",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
