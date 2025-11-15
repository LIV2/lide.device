#ifndef __MOUNTER_H
#define __MOUNTER_H

ULONG mount(char *dev_name asm("a0"), struct ConfigDev *cd asm("a1"), struct ExecBase *SysBase asm("a6"));

#endif