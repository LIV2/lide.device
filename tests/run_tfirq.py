#!/usr/bin/env python3
"""Check TF4060 polling and legacy IRQ setup using the production helpers.

Exec calls and register bytes are mocked; no interrupt timing is modeled.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def function(file, name):
    source = (root / file).read_text()
    start = re.search(r'^static (?:inline )?void ' + name + r'\s*\(', source, re.M).start()
    end = source.index('\n}', start) + 2
    return source[start:end]

source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UBYTE;
typedef uint32_t ULONG;
#define TF_IDE 1
#define Info(...) ((void)0)
#define NT_INTERRUPT 2
#define TF_IRQ_PRI 20
#define ATA_TASK_NAME "test"
#define INTB_PORTS 3
#define CHANNEL_1 0x1000
#define ata_reg_altStatus 0x18
#define nIEN 2
struct ExecBase { int unused; };
struct Interrupt {
    struct { int ln_Type, ln_Pri; const char *ln_Name; } is_Node;
    void *is_Data;
    void (*is_Code)(void);
};
struct IOTask {
    struct ExecBase *SysBase;
    struct { volatile UBYTE *board, *ide, *intena; } tf;
    int irqSigBit;
    ULONG irqSigMask;
    struct Interrupt irqServer;
    bool irqInstalled;
};
struct IDEUnit { struct IOTask *itask; struct { volatile UBYTE *alt_status; } drive; };
static unsigned allocs, adds, removes, frees;
static int alloc_result=4;
static void tf_irq_server(void) {}
static int AllocSignal(int n) { assert(n==-1); ++allocs; return alloc_result; }
static void FreeSignal(int n) { assert(n==4); ++frees; }
static void AddIntServer(int n, struct Interrupt *s) {
    assert(n==INTB_PORTS && s->is_Code==tf_irq_server);
    assert(s->is_Node.ln_Type==NT_INTERRUPT && s->is_Node.ln_Pri==TF_IRQ_PRI);
    assert(s->is_Data);
    ++adds;
}
static void RemIntServer(int n, struct Interrupt *s) { assert(n==3 && s->is_Code); ++removes; }
'''
source += function('iotask.c', 'tf_irq_install')
source += function('iotask.c', 'tf_irq_remove')
source += function('ata.h', 'ata_irq_enable')
source += r'''
int main(void) {
    UBYTE registers[0x1019]={0}, enable=0x25;
    struct IOTask task={.tf={.board=registers,.ide=registers,.intena=&enable}};
    struct IDEUnit unit={.itask=&task,.drive.alt_status=&registers[0x1018]};
    registers[0x1018]=nIEN;
    tf_irq_install(&task);
    assert(!task.irqInstalled && !allocs && !adds && enable==0x25);
    ata_irq_enable(&unit,true);
    assert(registers[0x1018]==nIEN && enable==0x25);
    ata_irq_enable(&unit,false);
    tf_irq_remove(&task);
    assert(registers[0x1018]==nIEN && enable==0x25 && !removes && !frees);
    task.tf.board=NULL;
    tf_irq_install(&task);
    assert(task.irqInstalled && allocs==1 && adds==1 && enable==0xa5);
    ata_irq_enable(&unit,true); assert(registers[0x1018]==0);
    ata_irq_enable(&unit,false); assert(registers[0x1018]==nIEN);
    tf_irq_remove(&task);
    assert(!task.irqInstalled && removes==1 && frees==1 && enable==0x25);
    alloc_result=-1;
    tf_irq_install(&task);
    assert(!task.irqInstalled && allocs==2 && adds==1 && enable==0x25);
    ata_irq_enable(&unit,true);
    tf_irq_remove(&task);
    assert(registers[0x1018]==nIEN && removes==1 && frees==1);
    puts("PASS: TF4060 never installs or enables IDE IRQs; legacy IRQ lifecycle preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='lide-noirq-') as directory:
    path=Path(directory)
    (path/'test.c').write_text(source)
    subprocess.run(['cc','-std=gnu11','-O2','-g','-fsanitize=address,undefined',
                    str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
