
liv2ride:     file format amiga


Disassembly of section .text:

00000000 00000000 __start:
Resident structure.
------------------------------------------------------------*/
int __attribute__((no_reorder)) _start()
{
    return -1;
}
       0:	70ff           	moveq #-1,d0
       2:	4e75           	rts
       4:	4afc           	illegal
       6:	0000 0004      	.long __stext+0x4
       a:	0000 3314      	.long _NewList
       e:	017b           	.short 0x017b
      10:	0300           	btst d1,d0
      12:	0000 0004      	.long _device_name+0x4
      16:	0000 001e      	.long _device_id_string
      1a:	0000 0cf6      	.long _init_device+0xb16

0000001e 0000001e _device_id_string:
      1e:	6c69           	bge.s 89 89 _set_dev_name+0x4d
      20:	7632           	moveq #50,d3
      22:	7269           	moveq #105,d1
      24:	6465           	bcc.s 8b 8b _set_dev_name+0x4f
      26:	2031 3233      	move.l (51,a1,d3.w*2),d0
      2a:	2e30 2028      	move.l (40,a0,d2.w),d7
      2e:	3320           	move.w -(a0),-(a1)
      30:	4d61           	.short 0x4d61
      32:	7263           	moveq #99,d1
      34:	6820           	bvc.s 56 56 _set_dev_name+0x1a
      36:	3230 3233      	move.w (51,a0,d3.w*2),d1
      3a:	2900           	move.l d0,-(a4)

0000003c 0000003c _set_dev_name:
 * set_dev_name
 *
 * Try to set a unique drive name
 * will prepend 2nd/3rd/4th. etc to the beginning of device_name
*/
char * set_dev_name(struct DeviceBase *dev) {
      3c:	48e7 2022      	movem.l d2/a2/a6,-(sp)
    struct ExecBase *SysBase = dev->SysBase;
      40:	206f 0010      	movea.l 16(sp),a0
      44:	2c68 0022      	movea.l 34(a0),a6
      48:	240e           	move.l a6,d2
      4a:	0682 0000 015e 	addi.l #350,d2
    ULONG device_prefix[] = {' nd.', ' rd.', ' th.'};
    char * devName = (device_name + 4); // Start with just the base device name, no prefix

    /* Prefix the device name if a device with the same name already exists */
    for (int i=0; i<8; i++) {
        if (FindName(&SysBase->DeviceList,devName)) {
      50:	43f9 0000 0004 	lea 4 4 __sdata+0x4,a1
      56:	2042           	movea.l d2,a0
      58:	4eae feec      	jsr -276(a6)
      5c:	4a80           	tst.l d0
      5e:	6700 0154      	beq.w 1b4 1b4 _set_dev_name+0x178
            if (i==0) devName = device_name;
            switch (i) {
                case 0:
                    *(ULONG *)devName = device_prefix[0];
      62:	45f9 0000 0000 	lea 0 0 __sdata,a2
      68:	13fc 006e 0000 	move.b #110,1 1 __sdata+0x1
      6e:	0001 
      70:	13fc 0064 0000 	move.b #100,2 2 __sdata+0x2
      76:	0002 
      78:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
      7e:	0003 
                    break;
                default:
                    *(ULONG *)devName = device_prefix[2];
                    break;
            }
            *(char *)devName = '2' + i;
      80:	14bc 0032      	move.b #50,(a2)
        if (FindName(&SysBase->DeviceList,devName)) {
      84:	224a           	movea.l a2,a1
      86:	2042           	movea.l d2,a0
      88:	4eae feec      	jsr -276(a6)
      8c:	4a80           	tst.l d0
      8e:	6700 011c      	beq.w 1ac 1ac _set_dev_name+0x170
                    *(ULONG *)devName = device_prefix[1];
      92:	13fc 0072 0000 	move.b #114,1 1 __sdata+0x1
      98:	0001 
      9a:	13fc 0064 0000 	move.b #100,2 2 __sdata+0x2
      a0:	0002 
      a2:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
      a8:	0003 
            *(char *)devName = '2' + i;
      aa:	14bc 0033      	move.b #51,(a2)
        if (FindName(&SysBase->DeviceList,devName)) {
      ae:	224a           	movea.l a2,a1
      b0:	2042           	movea.l d2,a0
      b2:	4eae feec      	jsr -276(a6)
      b6:	4a80           	tst.l d0
      b8:	6700 00f2      	beq.w 1ac 1ac _set_dev_name+0x170
                    *(ULONG *)devName = device_prefix[2];
      bc:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
      c2:	0001 
      c4:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
      ca:	0002 
      cc:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
      d2:	0003 
            *(char *)devName = '2' + i;
      d4:	14bc 0034      	move.b #52,(a2)
        if (FindName(&SysBase->DeviceList,devName)) {
      d8:	224a           	movea.l a2,a1
      da:	2042           	movea.l d2,a0
      dc:	4eae feec      	jsr -276(a6)
      e0:	4a80           	tst.l d0
      e2:	6700 00c8      	beq.w 1ac 1ac _set_dev_name+0x170
                    *(ULONG *)devName = device_prefix[2];
      e6:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
      ec:	0001 
      ee:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
      f4:	0002 
      f6:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
      fc:	0003 
            *(char *)devName = '2' + i;
      fe:	14bc 0035      	move.b #53,(a2)
        if (FindName(&SysBase->DeviceList,devName)) {
     102:	224a           	movea.l a2,a1
     104:	2042           	movea.l d2,a0
     106:	4eae feec      	jsr -276(a6)
     10a:	4a80           	tst.l d0
     10c:	6700 009e      	beq.w 1ac 1ac _set_dev_name+0x170
                    *(ULONG *)devName = device_prefix[2];
     110:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     116:	0001 
     118:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     11e:	0002 
     120:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     126:	0003 
            *(char *)devName = '2' + i;
     128:	14bc 0036      	move.b #54,(a2)
        if (FindName(&SysBase->DeviceList,devName)) {
     12c:	224a           	movea.l a2,a1
     12e:	2042           	movea.l d2,a0
     130:	4eae feec      	jsr -276(a6)
     134:	4a80           	tst.l d0
     136:	6774           	beq.s 1ac 1ac _set_dev_name+0x170
                    *(ULONG *)devName = device_prefix[2];
     138:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     13e:	0001 
     140:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     146:	0002 
     148:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     14e:	0003 
            *(char *)devName = '2' + i;
     150:	14bc 0037      	move.b #55,(a2)
        if (FindName(&SysBase->DeviceList,devName)) {
     154:	224a           	movea.l a2,a1
     156:	2042           	movea.l d2,a0
     158:	4eae feec      	jsr -276(a6)
     15c:	4a80           	tst.l d0
     15e:	674c           	beq.s 1ac 1ac _set_dev_name+0x170
                    *(ULONG *)devName = device_prefix[2];
     160:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     166:	0001 
     168:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     16e:	0002 
     170:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     176:	0003 
            *(char *)devName = '2' + i;
     178:	14bc 0038      	move.b #56,(a2)
        if (FindName(&SysBase->DeviceList,devName)) {
     17c:	224a           	movea.l a2,a1
     17e:	2042           	movea.l d2,a0
     180:	4eae feec      	jsr -276(a6)
     184:	4a80           	tst.l d0
     186:	6724           	beq.s 1ac 1ac _set_dev_name+0x170
                    *(ULONG *)devName = device_prefix[2];
     188:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     18e:	0001 
     190:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     196:	0002 
     198:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     19e:	0003 
            *(char *)devName = '2' + i;
     1a0:	14bc 0039      	move.b #57,(a2)
            Info("Device name: %s\n",devName);
            return devName;
        }
    }
    Info("Couldn't set device name.\n");
    return NULL;
     1a4:	7000           	moveq #0,d0
}
     1a6:	4cdf 4404      	movem.l (sp)+,d2/a2/a6
     1aa:	4e75           	rts
        if (FindName(&SysBase->DeviceList,devName)) {
     1ac:	200a           	move.l a2,d0
}
     1ae:	4cdf 4404      	movem.l (sp)+,d2/a2/a6
     1b2:	4e75           	rts
    char * devName = (device_name + 4); // Start with just the base device name, no prefix
     1b4:	203c 0000 0004 	move.l #4,d0
}
     1ba:	4cdf 4404      	movem.l (sp)+,d2/a2/a6
     1be:	4e75           	rts
     1c0:	6578           	bcs.s 23a 23a _init_device+0x5a
     1c2:	7061           	moveq #97,d0
     1c4:	6e73           	bgt.s 239 239 _init_device+0x59
     1c6:	696f           	bvs.s 237 237 _init_device+0x57
     1c8:	6e2e           	bgt.s 1f8 1f8 _init_device+0x18
     1ca:	6c69           	bge.s 235 235 _init_device+0x55
     1cc:	6272           	bhi.s 240 240 _init_device+0x60
     1ce:	6172           	bsr.s 242 242 _init_device+0x62
     1d0:	7900           	.short 0x7900
     1d2:	7469           	moveq #105,d2
     1d4:	6d65           	blt.s 23b 23b _init_device+0x5b
     1d6:	722e           	moveq #46,d1
     1d8:	6465           	bcc.s 23f 23f _init_device+0x5f
     1da:	7669           	moveq #105,d3
     1dc:	6365           	bls.s 243 243 _init_device+0x63
     1de:	0000           	Address 0x00000000000001e0 is out of bounds.
.short 0x0000

000001e0 000001e0 _init_device:
 *
 * Scan for drives and initialize the driver if any are found
*/
struct Library __attribute__((used, saveds)) * init_device(struct ExecBase *SysBase asm("a6"), BPTR seg_list asm("a0"), struct DeviceBase *dev asm("d0"))
//struct Library __attribute__((used)) * init_device(struct ExecBase *SysBase, BPTR seg_list, struct DeviceBase *dev)
{
     1e0:	598f           	subq.l #4,sp
     1e2:	48e7 3832      	movem.l d2-d4/a2-a3/a6,-(sp)
     1e6:	260e           	move.l a6,d3
     1e8:	2440           	movea.l d0,a2
     1ea:	2808           	move.l a0,d4
    dev->SysBase = SysBase;
     1ec:	254e 0022      	move.l a6,34(a2)
     1f0:	243c 0000 015e 	move.l #350,d2
     1f6:	d48e           	add.l a6,d2
        if (FindName(&SysBase->DeviceList,devName)) {
     1f8:	43f9 0000 0004 	lea 4 4 __sdata+0x4,a1
     1fe:	2042           	movea.l d2,a0
     200:	4eae feec      	jsr -276(a6)
     204:	4a80           	tst.l d0
     206:	6700 0150      	beq.w 358 358 _init_device+0x178
                    *(ULONG *)devName = device_prefix[0];
     20a:	47f9 0000 0000 	lea 0 0 __sdata,a3
     210:	13fc 006e 0000 	move.b #110,1 1 __sdata+0x1
     216:	0001 
     218:	13fc 0064 0000 	move.b #100,2 2 __sdata+0x2
     21e:	0002 
     220:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     226:	0003 
            *(char *)devName = '2' + i;
     228:	16bc 0032      	move.b #50,(a3)
        if (FindName(&SysBase->DeviceList,devName)) {
     22c:	224b           	movea.l a3,a1
     22e:	2042           	movea.l d2,a0
     230:	4eae feec      	jsr -276(a6)
     234:	4a80           	tst.l d0
     236:	6700 0126      	beq.w 35e 35e _init_device+0x17e
                    *(ULONG *)devName = device_prefix[1];
     23a:	13fc 0072 0000 	move.b #114,1 1 __sdata+0x1
     240:	0001 
     242:	13fc 0064 0000 	move.b #100,2 2 __sdata+0x2
     248:	0002 
     24a:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     250:	0003 
            *(char *)devName = '2' + i;
     252:	16bc 0033      	move.b #51,(a3)
        if (FindName(&SysBase->DeviceList,devName)) {
     256:	224b           	movea.l a3,a1
     258:	2042           	movea.l d2,a0
     25a:	4eae feec      	jsr -276(a6)
     25e:	4a80           	tst.l d0
     260:	6700 00fc      	beq.w 35e 35e _init_device+0x17e
                    *(ULONG *)devName = device_prefix[2];
     264:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     26a:	0001 
     26c:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     272:	0002 
     274:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     27a:	0003 
            *(char *)devName = '2' + i;
     27c:	16bc 0034      	move.b #52,(a3)
        if (FindName(&SysBase->DeviceList,devName)) {
     280:	224b           	movea.l a3,a1
     282:	2042           	movea.l d2,a0
     284:	4eae feec      	jsr -276(a6)
     288:	4a80           	tst.l d0
     28a:	6700 00d2      	beq.w 35e 35e _init_device+0x17e
                    *(ULONG *)devName = device_prefix[2];
     28e:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     294:	0001 
     296:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     29c:	0002 
     29e:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     2a4:	0003 
            *(char *)devName = '2' + i;
     2a6:	16bc 0035      	move.b #53,(a3)
        if (FindName(&SysBase->DeviceList,devName)) {
     2aa:	224b           	movea.l a3,a1
     2ac:	2042           	movea.l d2,a0
     2ae:	4eae feec      	jsr -276(a6)
     2b2:	4a80           	tst.l d0
     2b4:	6700 00a8      	beq.w 35e 35e _init_device+0x17e
                    *(ULONG *)devName = device_prefix[2];
     2b8:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     2be:	0001 
     2c0:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     2c6:	0002 
     2c8:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     2ce:	0003 
            *(char *)devName = '2' + i;
     2d0:	16bc 0036      	move.b #54,(a3)
        if (FindName(&SysBase->DeviceList,devName)) {
     2d4:	224b           	movea.l a3,a1
     2d6:	2042           	movea.l d2,a0
     2d8:	4eae feec      	jsr -276(a6)
     2dc:	4a80           	tst.l d0
     2de:	677e           	beq.s 35e 35e _init_device+0x17e
                    *(ULONG *)devName = device_prefix[2];
     2e0:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     2e6:	0001 
     2e8:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     2ee:	0002 
     2f0:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     2f6:	0003 
            *(char *)devName = '2' + i;
     2f8:	16bc 0037      	move.b #55,(a3)
        if (FindName(&SysBase->DeviceList,devName)) {
     2fc:	224b           	movea.l a3,a1
     2fe:	2042           	movea.l d2,a0
     300:	4eae feec      	jsr -276(a6)
     304:	4a80           	tst.l d0
     306:	6756           	beq.s 35e 35e _init_device+0x17e
                    *(ULONG *)devName = device_prefix[2];
     308:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     30e:	0001 
     310:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     316:	0002 
     318:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     31e:	0003 
            *(char *)devName = '2' + i;
     320:	16bc 0038      	move.b #56,(a3)
        if (FindName(&SysBase->DeviceList,devName)) {
     324:	224b           	movea.l a3,a1
     326:	2042           	movea.l d2,a0
     328:	4eae feec      	jsr -276(a6)
     32c:	4a80           	tst.l d0
     32e:	672e           	beq.s 35e 35e _init_device+0x17e
                    *(ULONG *)devName = device_prefix[2];
     330:	13fc 0074 0000 	move.b #116,1 1 __sdata+0x1
     336:	0001 
     338:	13fc 0068 0000 	move.b #104,2 2 __sdata+0x2
     33e:	0002 
     340:	13fc 002e 0000 	move.b #46,3 3 __sdata+0x3
     346:	0003 
            *(char *)devName = '2' + i;
     348:	16bc 0039      	move.b #57,(a3)
        }
        Info("Startup finished.\n");
        return (struct Library *)dev;
    } else {
        Cleanup(dev);
        return NULL;
     34c:	91c8           	suba.l a0,a0
    }
}
     34e:	2008           	move.l a0,d0
     350:	4cdf 4c1c      	movem.l (sp)+,d2-d4/a2-a3/a6
     354:	588f           	addq.l #4,sp
     356:	4e75           	rts
    char * devName = (device_name + 4); // Start with just the base device name, no prefix
     358:	47f9 0000 0004 	lea 4 4 __sdata+0x4,a3
    dev->saved_seg_list       = seg_list;
     35e:	2544 0040      	move.l d4,64(a2)
    dev->lib.lib_Node.ln_Type = NT_DEVICE;
     362:	157c 0003 0008 	move.b #3,8(a2)
    dev->lib.lib_Node.ln_Name = devName;
     368:	254b 000a      	move.l a3,10(a2)
    dev->lib.lib_Flags        = LIBF_SUMUSED | LIBF_CHANGED;
     36c:	157c 0006 000e 	move.b #6,14(a2)
    dev->lib.lib_Version      = DEVICE_VERSION;
     372:	357c 007b 0014 	move.w #123,20(a2)
    dev->lib.lib_Revision     = DEVICE_REVISION;
     378:	426a 0016      	clr.w 22(a2)
    dev->lib.lib_IdString     = (APTR)device_id_string;
     37c:	257c 0000 001e 	move.l #30,24(a2)
     382:	0018 
    dev->is_open    = FALSE;
     384:	426a 0044      	clr.w 68(a2)
    dev->num_boards = 0;
     388:	422a 0046      	clr.b 70(a2)
    dev->num_units  = 0;
     38c:	422a 0047      	clr.b 71(a2)
    dev->TaskMP     = NULL;
     390:	42aa 0032      	clr.l 50(a2)
    dev->Task       = NULL;
     394:	42aa 002e      	clr.l 46(a2)
    dev->TaskActive = false;
     398:	157c 0000 0036 	move.b #0,54(a2)
    if ((dev->units = AllocMem(sizeof(struct IDEUnit)*MAX_UNITS, (MEMF_ANY|MEMF_CLEAR))) == NULL)
     39e:	2c43           	movea.l d3,a6
     3a0:	203c 0000 0148 	move.l #328,d0
     3a6:	7201           	moveq #1,d1
     3a8:	4841           	swap d1
     3aa:	4eae ff3a      	jsr -198(a6)
     3ae:	2540 0048      	move.l d0,72(a2)
     3b2:	6798           	beq.s 34c 34c _init_device+0x16c
    if (!(ExpansionBase = (struct Library *)OpenLibrary("expansion.library",0))) {
     3b4:	7000           	moveq #0,d0
     3b6:	43fa fe08      	lea 1c0 1c0 _set_dev_name+0x184(pc),a1
     3ba:	4eae fdd8      	jsr -552(a6)
     3be:	2640           	movea.l d0,a3
     3c0:	4a80           	tst.l d0
     3c2:	6700 02c0      	beq.w 684 684 _init_device+0x4a4
        dev->ExpansionBase = ExpansionBase;
     3c6:	2540 0026      	move.l d0,38(a2)
    if ((dev->TimerMP = CreatePort(NULL,0)) != NULL && (dev->TimeReq = (struct timerequest *)CreateExtIO(dev->TimerMP, sizeof(struct timerequest))) != NULL) {
     3ca:	42a7           	clr.l -(sp)
     3cc:	42a7           	clr.l -(sp)
     3ce:	4eb9 0000 3328 	jsr 3328 3328 _CreatePort
     3d4:	2540 0038      	move.l d0,56(a2)
     3d8:	508f           	addq.l #8,sp
     3da:	6700 0240      	beq.w 61c 61c _init_device+0x43c
     3de:	4878 0028      	pea 28 28 _device_id_string+0xa
     3e2:	2f00           	move.l d0,-(sp)
     3e4:	4eb9 0000 33e4 	jsr 33e4 33e4 _CreateExtIO
     3ea:	2240           	movea.l d0,a1
     3ec:	2540 003c      	move.l d0,60(a2)
     3f0:	508f           	addq.l #8,sp
     3f2:	6700 022c      	beq.w 620 620 _init_device+0x440
        if (OpenDevice("timer.device",UNIT_MICROHZ,(struct IORequest *)dev->TimeReq,0)) {
     3f6:	7000           	moveq #0,d0
     3f8:	2200           	move.l d0,d1
     3fa:	41fa fdd6      	lea 1d2 1d2 _set_dev_name+0x196(pc),a0
     3fe:	4eae fe44      	jsr -444(a6)
     402:	4a00           	tst.b d0
     404:	6600 01ae      	bne.w 5b4 5b4 _init_device+0x3d4
    cd = FindConfigDev(NULL,MANUF_ID,DEV_ID);
     408:	2c4b           	movea.l a3,a6
     40a:	91c8           	suba.l a0,a0
     40c:	203c 0000 082c 	move.l #2092,d0
     412:	7206           	moveq #6,d1
     414:	4eae ffb8      	jsr -72(a6)
     418:	2040           	movea.l d0,a0
    while (cd != NULL)
     41a:	4a80           	tst.l d0
     41c:	6700 00b8      	beq.w 4d6 4d6 _init_device+0x2f6
        if (cd->cd_Flags & CDF_CONFIGME) {
     420:	1028 000e      	move.b 14(a0),d0
     424:	0800 0001      	btst #1,d0
     428:	6622           	bne.s 44c 44c _init_device+0x26c
        cd = FindConfigDev(cd,MANUF_ID,DEV_ID);
     42a:	243c 0000 082c 	move.l #2092,d2
     430:	2c4b           	movea.l a3,a6
     432:	2002           	move.l d2,d0
     434:	7206           	moveq #6,d1
     436:	4eae ffb8      	jsr -72(a6)
     43a:	2040           	movea.l d0,a0
    while (cd != NULL)
     43c:	4a80           	tst.l d0
     43e:	6700 0096      	beq.w 4d6 4d6 _init_device+0x2f6
        if (cd->cd_Flags & CDF_CONFIGME) {
     442:	1028 000e      	move.b 14(a0),d0
     446:	0800 0001      	btst #1,d0
     44a:	67e4           	beq.s 430 430 _init_device+0x250
            cd->cd_Flags &= ~(CDF_CONFIGME); // Claim the board
     44c:	0200 00fd      	andi.b #-3,d0
     450:	1140 000e      	move.b d0,14(a0)
            dev->num_boards++;
     454:	522a 0046      	addq.b #1,70(a2)
                dev->units[i].SysBase      = SysBase;
     458:	226a 0048      	movea.l 72(a2),a1
     45c:	2343 002a      	move.l d3,42(a1)
                dev->units[i].TimeReq      = dev->TimeReq;
     460:	236a 003c 002e 	move.l 60(a2),46(a1)
                dev->units[i].cd           = cd;
     466:	2348 0026      	move.l a0,38(a1)
                dev->units[i].primary      = ((i%2) == 1) ? false : true;
     46a:	337c 0001 003e 	move.w #1,62(a1)
                dev->units[i].channel      = ((i%4) < 2) ? 0 : 1;
     470:	4229 0037      	clr.b 55(a1)
                dev->units[i].change_count = 1;
     474:	337c 0001 0042 	move.w #1,66(a1)
                dev->units[i].device_type  = DG_DIRECT_ACCESS;
     47a:	4229 0038      	clr.b 56(a1)
                dev->units[i].present      = false;
     47e:	4269 0040      	clr.w 64(a1)
                if (ata_init_unit(&dev->units[i])) {
     482:	2f09           	move.l a1,-(sp)
     484:	47f9 0000 0eb2 	lea eb2 eb2 _ata_init_unit,a3
     48a:	2f48 001c      	move.l a0,28(sp)
     48e:	4e93           	jsr (a3)
     490:	588f           	addq.l #4,sp
     492:	206f 0018      	movea.l 24(sp),a0
     496:	4a00           	tst.b d0
     498:	6704           	beq.s 49e 49e _init_device+0x2be
                    dev->num_units++;
     49a:	522a 0047      	addq.b #1,71(a2)
                dev->units[i].SysBase      = SysBase;
     49e:	226a 0048      	movea.l 72(a2),a1
     4a2:	2343 007c      	move.l d3,124(a1)
                dev->units[i].TimeReq      = dev->TimeReq;
     4a6:	236a 003c 0080 	move.l 60(a2),128(a1)
                dev->units[i].cd           = cd;
     4ac:	2348 0078      	move.l a0,120(a1)
                dev->units[i].primary      = ((i%2) == 1) ? false : true;
     4b0:	4269 0090      	clr.w 144(a1)
                dev->units[i].channel      = ((i%4) < 2) ? 0 : 1;
     4b4:	4229 0089      	clr.b 137(a1)
                dev->units[i].change_count = 1;
     4b8:	337c 0001 0094 	move.w #1,148(a1)
                dev->units[i].device_type  = DG_DIRECT_ACCESS;
     4be:	4229 008a      	clr.b 138(a1)
                dev->units[i].present      = false;
     4c2:	4269 0092      	clr.w 146(a1)
                if (ata_init_unit(&dev->units[i])) {
     4c6:	4869 0052      	pea 82(a1)
     4ca:	4e93           	jsr (a3)
     4cc:	588f           	addq.l #4,sp
     4ce:	4a00           	tst.b d0
     4d0:	6704           	beq.s 4d6 4d6 _init_device+0x2f6
                    dev->num_units++;
     4d2:	522a 0047      	addq.b #1,71(a2)
    if (dev->num_units > 0) {
     4d6:	4a2a 0047      	tst.b 71(a2)
     4da:	673c           	beq.s 518 518 _init_device+0x338
        dev->Task = CreateTask(dev->lib.lib_Node.ln_Name,TASK_PRIORITY,(APTR)ide_task,TASK_STACK_SIZE);
     4dc:	2f3c 0000 ffff 	move.l #65535,-(sp)
     4e2:	4879 0000 1584 	pea 1584 1584 _ide_task
     4e8:	42a7           	clr.l -(sp)
     4ea:	2f2a 000a      	move.l 10(a2),-(sp)
     4ee:	4eb9 0000 3460 	jsr 3460 3460 _CreateTask
     4f4:	2540 002e      	move.l d0,46(a2)
        if (!dev->Task) {
     4f8:	4fef 0010      	lea 16(sp),sp
     4fc:	6700 0226      	beq.w 724 724 _init_device+0x544
        dev->Task->tc_UserData = dev;
     500:	2040           	movea.l d0,a0
     502:	214a 0058      	move.l a2,88(a0)
        while (dev->TaskActive == false) {
     506:	102a 0036      	move.b 54(a2),d0
     50a:	67fa           	beq.s 506 506 _init_device+0x326
        return (struct Library *)dev;
     50c:	204a           	movea.l a2,a0
}
     50e:	2008           	move.l a0,d0
     510:	4cdf 4c1c      	movem.l (sp)+,d2-d4/a2-a3/a6
     514:	588f           	addq.l #4,sp
     516:	4e75           	rts
     518:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     51c:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     520:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     524:	2009           	move.l a1,d0
     526:	6706           	beq.s 52e 52e _init_device+0x34e
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     528:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     52e:	2268 0078      	movea.l 120(a0),a1
     532:	2009           	move.l a1,d0
     534:	6706           	beq.s 53c 53c _init_device+0x35c
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     536:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     53c:	2268 00ca      	movea.l 202(a0),a1
     540:	2009           	move.l a1,d0
     542:	6706           	beq.s 54a 54a _init_device+0x36a
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     544:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     54a:	2068 011c      	movea.l 284(a0),a0
     54e:	2008           	move.l a0,d0
     550:	6706           	beq.s 558 558 _init_device+0x378
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     552:	0028 0002 000e 	ori.b #2,14(a0)
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
     558:	226a 003c      	movea.l 60(a2),a1
     55c:	4aa9 0014      	tst.l 20(a1)
     560:	670a           	beq.s 56c 56c _init_device+0x38c
     562:	2c42           	movea.l d2,a6
     564:	4eae fe3e      	jsr -450(a6)
     568:	226a 003c      	movea.l 60(a2),a1
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
     56c:	2009           	move.l a1,d0
     56e:	670a           	beq.s 57a 57a _init_device+0x39a
     570:	2f09           	move.l a1,-(sp)
     572:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
     578:	588f           	addq.l #4,sp
    if (dev->TimerMP) DeletePort(dev->TimerMP);
     57a:	202a 0038      	move.l 56(a2),d0
     57e:	670a           	beq.s 58a 58a _init_device+0x3aa
     580:	2f00           	move.l d0,-(sp)
     582:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
     588:	588f           	addq.l #4,sp
    if (dev->ExpansionBase) CloseLibrary((struct Library *)dev->ExpansionBase);
     58a:	226a 0026      	movea.l 38(a2),a1
     58e:	2009           	move.l a1,d0
     590:	6706           	beq.s 598 598 _init_device+0x3b8
     592:	2c42           	movea.l d2,a6
     594:	4eae fe62      	jsr -414(a6)
    if (dev->units) FreeMem(dev->units,sizeof(struct IDEUnit) * MAX_UNITS);
     598:	226a 0048      	movea.l 72(a2),a1
     59c:	2009           	move.l a1,d0
     59e:	6700 fdac      	beq.w 34c 34c _init_device+0x16c
     5a2:	203c 0000 0148 	move.l #328,d0
     5a8:	2c42           	movea.l d2,a6
     5aa:	4eae ff2e      	jsr -210(a6)
        return NULL;
     5ae:	91c8           	suba.l a0,a0
     5b0:	6000 fd9c      	bra.w 34e 34e _init_device+0x16e
     5b4:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     5b8:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     5bc:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     5c0:	2009           	move.l a1,d0
     5c2:	6706           	beq.s 5ca 5ca _init_device+0x3ea
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     5c4:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     5ca:	2268 0078      	movea.l 120(a0),a1
     5ce:	2009           	move.l a1,d0
     5d0:	6706           	beq.s 5d8 5d8 _init_device+0x3f8
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     5d2:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     5d8:	2268 00ca      	movea.l 202(a0),a1
     5dc:	2009           	move.l a1,d0
     5de:	6706           	beq.s 5e6 5e6 _init_device+0x406
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     5e0:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     5e6:	2068 011c      	movea.l 284(a0),a0
     5ea:	2008           	move.l a0,d0
     5ec:	6706           	beq.s 5f4 5f4 _init_device+0x414
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     5ee:	0028 0002 000e 	ori.b #2,14(a0)
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
     5f4:	226a 003c      	movea.l 60(a2),a1
     5f8:	4aa9 0014      	tst.l 20(a1)
     5fc:	670a           	beq.s 608 608 _init_device+0x428
     5fe:	2c42           	movea.l d2,a6
     600:	4eae fe3e      	jsr -450(a6)
     604:	226a 003c      	movea.l 60(a2),a1
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
     608:	2009           	move.l a1,d0
     60a:	6700 ff6e      	beq.w 57a 57a _init_device+0x39a
     60e:	2f09           	move.l a1,-(sp)
     610:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
     616:	588f           	addq.l #4,sp
     618:	6000 ff60      	bra.w 57a 57a _init_device+0x39a
     61c:	226a 003c      	movea.l 60(a2),a1
     620:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     624:	2668 0026      	movea.l 38(a0),a3
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     628:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     62c:	200b           	move.l a3,d0
     62e:	6706           	beq.s 636 636 _init_device+0x456
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     630:	002b 0002 000e 	ori.b #2,14(a3)
        if (dev->units[i].cd != NULL) {
     636:	2668 0078      	movea.l 120(a0),a3
     63a:	200b           	move.l a3,d0
     63c:	6706           	beq.s 644 644 _init_device+0x464
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     63e:	002b 0002 000e 	ori.b #2,14(a3)
        if (dev->units[i].cd != NULL) {
     644:	2668 00ca      	movea.l 202(a0),a3
     648:	200b           	move.l a3,d0
     64a:	6706           	beq.s 652 652 _init_device+0x472
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     64c:	002b 0002 000e 	ori.b #2,14(a3)
        if (dev->units[i].cd != NULL) {
     652:	2068 011c      	movea.l 284(a0),a0
     656:	2008           	move.l a0,d0
     658:	6706           	beq.s 660 660 _init_device+0x480
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     65a:	0028 0002 000e 	ori.b #2,14(a0)
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
     660:	4aa9 0014      	tst.l 20(a1)
     664:	670a           	beq.s 670 670 _init_device+0x490
     666:	2c42           	movea.l d2,a6
     668:	4eae fe3e      	jsr -450(a6)
     66c:	226a 003c      	movea.l 60(a2),a1
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
     670:	2009           	move.l a1,d0
     672:	6700 ff06      	beq.w 57a 57a _init_device+0x39a
     676:	2f09           	move.l a1,-(sp)
     678:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
     67e:	588f           	addq.l #4,sp
     680:	6000 fef8      	bra.w 57a 57a _init_device+0x39a
     684:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     688:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     68c:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     690:	2009           	move.l a1,d0
     692:	6706           	beq.s 69a 69a _init_device+0x4ba
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     694:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     69a:	2268 0078      	movea.l 120(a0),a1
     69e:	2009           	move.l a1,d0
     6a0:	6706           	beq.s 6a8 6a8 _init_device+0x4c8
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     6a2:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     6a8:	2268 00ca      	movea.l 202(a0),a1
     6ac:	2009           	move.l a1,d0
     6ae:	6706           	beq.s 6b6 6b6 _init_device+0x4d6
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     6b0:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     6b6:	2068 011c      	movea.l 284(a0),a0
     6ba:	2008           	move.l a0,d0
     6bc:	6706           	beq.s 6c4 6c4 _init_device+0x4e4
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     6be:	0028 0002 000e 	ori.b #2,14(a0)
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
     6c4:	226a 003c      	movea.l 60(a2),a1
     6c8:	4aa9 0014      	tst.l 20(a1)
     6cc:	670a           	beq.s 6d8 6d8 _init_device+0x4f8
     6ce:	2c42           	movea.l d2,a6
     6d0:	4eae fe3e      	jsr -450(a6)
     6d4:	226a 003c      	movea.l 60(a2),a1
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
     6d8:	2009           	move.l a1,d0
     6da:	670a           	beq.s 6e6 6e6 _init_device+0x506
     6dc:	2f09           	move.l a1,-(sp)
     6de:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
     6e4:	588f           	addq.l #4,sp
    if (dev->TimerMP) DeletePort(dev->TimerMP);
     6e6:	202a 0038      	move.l 56(a2),d0
     6ea:	670a           	beq.s 6f6 6f6 _init_device+0x516
     6ec:	2f00           	move.l d0,-(sp)
     6ee:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
     6f4:	588f           	addq.l #4,sp
    if (dev->ExpansionBase) CloseLibrary((struct Library *)dev->ExpansionBase);
     6f6:	226a 0026      	movea.l 38(a2),a1
     6fa:	2009           	move.l a1,d0
     6fc:	6706           	beq.s 704 704 _init_device+0x524
     6fe:	2c42           	movea.l d2,a6
     700:	4eae fe62      	jsr -414(a6)
    if (dev->units) FreeMem(dev->units,sizeof(struct IDEUnit) * MAX_UNITS);
     704:	226a 0048      	movea.l 72(a2),a1
     708:	2009           	move.l a1,d0
     70a:	6700 fc40      	beq.w 34c 34c _init_device+0x16c
     70e:	203c 0000 0148 	move.l #328,d0
     714:	2c42           	movea.l d2,a6
     716:	4eae ff2e      	jsr -210(a6)
}
     71a:	200b           	move.l a3,d0
     71c:	4cdf 4c1c      	movem.l (sp)+,d2-d4/a2-a3/a6
     720:	588f           	addq.l #4,sp
     722:	4e75           	rts
     724:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     728:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     72c:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     730:	2009           	move.l a1,d0
     732:	6706           	beq.s 73a 73a _init_device+0x55a
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     734:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     73a:	2268 0078      	movea.l 120(a0),a1
     73e:	2009           	move.l a1,d0
     740:	6706           	beq.s 748 748 _init_device+0x568
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     742:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     748:	2268 00ca      	movea.l 202(a0),a1
     74c:	2009           	move.l a1,d0
     74e:	6706           	beq.s 756 756 _init_device+0x576
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     750:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     756:	2068 011c      	movea.l 284(a0),a0
     75a:	2008           	move.l a0,d0
     75c:	6706           	beq.s 764 764 _init_device+0x584
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     75e:	0028 0002 000e 	ori.b #2,14(a0)
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
     764:	226a 003c      	movea.l 60(a2),a1
     768:	4aa9 0014      	tst.l 20(a1)
     76c:	670a           	beq.s 778 778 _init_device+0x598
     76e:	2c42           	movea.l d2,a6
     770:	4eae fe3e      	jsr -450(a6)
     774:	226a 003c      	movea.l 60(a2),a1
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
     778:	2009           	move.l a1,d0
     77a:	6700 fdfe      	beq.w 57a 57a _init_device+0x39a
     77e:	2f09           	move.l a1,-(sp)
     780:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
     786:	588f           	addq.l #4,sp
     788:	6000 fdf0      	bra.w 57a 57a _init_device+0x39a
/* device dependent expunge function
!!! CAUTION: This function runs in a forbidden state !!!
This call is guaranteed to be single-threaded; only one task
will execute your Expunge at a time. */
static BPTR __attribute__((used, saveds)) expunge(struct DeviceBase *dev asm("a6"))
{
     78c:	48e7 2032      	movem.l d2/a2-a3/a6,-(sp)
     790:	244e           	movea.l a6,a2
    Trace((CONST_STRPTR) "running expunge()\n");

    if (dev->lib.lib_OpenCnt != 0)
     792:	4a6e 0020      	tst.w 32(a6)
     796:	6600 00d8      	bne.w 870 870 _init_device+0x690
    {
        dev->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    if (dev->Task != NULL && dev->TaskActive == true) {
     79a:	4aae 002e      	tst.l 46(a6)
     79e:	6708           	beq.s 7a8 7a8 _init_device+0x5c8
     7a0:	102e 0036      	move.b 54(a6),d0
     7a4:	6600 00d8      	bne.w 87e 87e _init_device+0x69e
     7a8:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     7ac:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     7b0:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     7b4:	2009           	move.l a1,d0
     7b6:	6706           	beq.s 7be 7be _init_device+0x5de
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     7b8:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     7be:	2268 0078      	movea.l 120(a0),a1
     7c2:	2009           	move.l a1,d0
     7c4:	6706           	beq.s 7cc 7cc _init_device+0x5ec
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     7c6:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     7cc:	2268 00ca      	movea.l 202(a0),a1
     7d0:	2009           	move.l a1,d0
     7d2:	6706           	beq.s 7da 7da _init_device+0x5fa
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     7d4:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     7da:	2068 011c      	movea.l 284(a0),a0
     7de:	2008           	move.l a0,d0
     7e0:	6706           	beq.s 7e8 7e8 _init_device+0x608
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     7e2:	0028 0002 000e 	ori.b #2,14(a0)
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
     7e8:	226a 003c      	movea.l 60(a2),a1
     7ec:	4aa9 0014      	tst.l 20(a1)
     7f0:	670a           	beq.s 7fc 7fc _init_device+0x61c
     7f2:	2c42           	movea.l d2,a6
     7f4:	4eae fe3e      	jsr -450(a6)
     7f8:	226a 003c      	movea.l 60(a2),a1
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
     7fc:	2009           	move.l a1,d0
     7fe:	670a           	beq.s 80a 80a _init_device+0x62a
     800:	2f09           	move.l a1,-(sp)
     802:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
     808:	588f           	addq.l #4,sp
    if (dev->TimerMP) DeletePort(dev->TimerMP);
     80a:	202a 0038      	move.l 56(a2),d0
     80e:	670a           	beq.s 81a 81a _init_device+0x63a
     810:	2f00           	move.l d0,-(sp)
     812:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
     818:	588f           	addq.l #4,sp
    if (dev->ExpansionBase) CloseLibrary((struct Library *)dev->ExpansionBase);
     81a:	226a 0026      	movea.l 38(a2),a1
     81e:	2009           	move.l a1,d0
     820:	6706           	beq.s 828 828 _init_device+0x648
     822:	2c42           	movea.l d2,a6
     824:	4eae fe62      	jsr -414(a6)
    if (dev->units) FreeMem(dev->units,sizeof(struct IDEUnit) * MAX_UNITS);
     828:	226a 0048      	movea.l 72(a2),a1
     82c:	2009           	move.l a1,d0
     82e:	670c           	beq.s 83c 83c _init_device+0x65c
     830:	203c 0000 0148 	move.l #328,d0
     836:	2c42           	movea.l d2,a6
     838:	4eae ff2e      	jsr -210(a6)
        if (ioreq) DeleteStdIO(ioreq);
        if (mp) DeletePort(mp);
    }

    Cleanup(dev);
    BPTR seg_list = dev->saved_seg_list;
     83c:	242a 0040      	move.l 64(a2),d2
    Remove(&dev->lib.lib_Node);
     840:	224a           	movea.l a2,a1
     842:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     848:	4eae ff04      	jsr -252(a6)
    FreeMem((char *)dev - dev->lib.lib_NegSize, dev->lib.lib_NegSize + dev->lib.lib_PosSize);
     84c:	7000           	moveq #0,d0
     84e:	302a 0010      	move.w 16(a2),d0
     852:	7200           	moveq #0,d1
     854:	322a 0012      	move.w 18(a2),d1
     858:	224a           	movea.l a2,a1
     85a:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     860:	93c0           	suba.l d0,a1
     862:	d081           	add.l d1,d0
     864:	4eae ff2e      	jsr -210(a6)
    return seg_list;
}
     868:	2002           	move.l d2,d0
     86a:	4cdf 4c04      	movem.l (sp)+,d2/a2-a3/a6
     86e:	4e75           	rts
        dev->lib.lib_Flags |= LIBF_DELEXP;
     870:	002e 0008 000e 	ori.b #8,14(a6)
}
     876:	7000           	moveq #0,d0
     878:	4cdf 4c04      	movem.l (sp)+,d2/a2-a3/a6
     87c:	4e75           	rts
        if ((mp = CreatePort(NULL,0)) == NULL)
     87e:	42a7           	clr.l -(sp)
     880:	42a7           	clr.l -(sp)
     882:	4eb9 0000 3328 	jsr 3328 3328 _CreatePort
     888:	2400           	move.l d0,d2
     88a:	508f           	addq.l #8,sp
     88c:	67da           	beq.s 868 868 _init_device+0x688
        if ((ioreq = CreateStdIO(mp)) == NULL) {
     88e:	2f00           	move.l d0,-(sp)
     890:	4eb9 0000 3426 	jsr 3426 3426 _CreateStdIO
     896:	2640           	movea.l d0,a3
     898:	588f           	addq.l #4,sp
     89a:	4a80           	tst.l d0
     89c:	674a           	beq.s 8e8 8e8 _init_device+0x708
        ioreq->io_Command = CMD_DIE; // Tell ide_task to shut down
     89e:	377c 000a 001c 	move.w #10,28(a3)
        PutMsg(dev->TaskMP,(struct Message *)ioreq);
     8a4:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     8aa:	2240           	movea.l d0,a1
     8ac:	206a 0032      	movea.l 50(a2),a0
     8b0:	4eae fe92      	jsr -366(a6)
        WaitPort(mp);                // Wait for ide_task to signal that it is shutting down
     8b4:	2042           	movea.l d2,a0
     8b6:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     8bc:	4eae fe80      	jsr -384(a6)
        if (ioreq) DeleteStdIO(ioreq);
     8c0:	2f0b           	move.l a3,-(sp)
     8c2:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
        if (mp) DeletePort(mp);
     8c8:	2f02           	move.l d2,-(sp)
     8ca:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
     8d0:	508f           	addq.l #8,sp
     8d2:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     8d6:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     8da:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     8de:	2009           	move.l a1,d0
     8e0:	6600 fed6      	bne.w 7b8 7b8 _init_device+0x5d8
     8e4:	6000 fed8      	bra.w 7be 7be _init_device+0x5de
            DeletePort(mp);
     8e8:	2f02           	move.l d2,-(sp)
     8ea:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
            return 0;
     8f0:	220b           	move.l a3,d1
}
     8f2:	2001           	move.l d1,d0
            DeletePort(mp);
     8f4:	588f           	addq.l #4,sp
}
     8f6:	4cdf 4c04      	movem.l (sp)+,d2/a2-a3/a6
     8fa:	4e75           	rts
This call is guaranteed to be single-threaded; only one task
will execute your Open at a time. */
static void __attribute__((used, saveds)) open(struct DeviceBase *dev asm("a6"), struct IORequest *ioreq asm("a1"), ULONG unitnum asm("d0"), ULONG flags asm("d1"))
{
    Trace((CONST_STRPTR) "running open() for unitnum %ld\n",unitnum);
    ioreq->io_Error = IOERR_OPENFAIL;
     8fc:	50e9 001f      	st 31(a1)

    if (dev->Task == NULL || dev->TaskActive == false) {
     900:	4aae 002e      	tst.l 46(a6)
     904:	673c           	beq.s 942 942 _init_device+0x762
     906:	122e 0036      	move.b 54(a6),d1
     90a:	6736           	beq.s 942 942 _init_device+0x762
        ioreq->io_Error = IOERR_OPENFAIL;
        return;
    }


    if (unitnum >= MAX_UNITS || dev->units[unitnum].present == false) {
     90c:	7203           	moveq #3,d1
     90e:	b280           	cmp.l d0,d1
     910:	6532           	bcs.s 944 944 _init_device+0x764
     912:	2200           	move.l d0,d1
     914:	e589           	lsl.l #2,d1
     916:	d280           	add.l d0,d1
     918:	e789           	lsl.l #3,d1
     91a:	d081           	add.l d1,d0
     91c:	2040           	movea.l d0,a0
     91e:	d1c0           	adda.l d0,a0
     920:	d1ee 0048      	adda.l 72(a6),a0
     924:	4a68 0040      	tst.w 64(a0)
     928:	671a           	beq.s 944 944 _init_device+0x764
        ioreq->io_Error = TDERR_BadUnitNum;
        return;
    }

    ioreq->io_Unit = (struct Unit *)&dev->units[unitnum];
     92a:	2348 0018      	move.l a0,24(a1)

    if (!dev->is_open)
     92e:	4a6e 0044      	tst.w 68(a6)
     932:	6606           	bne.s 93a 93a _init_device+0x75a
    {
        dev->is_open = TRUE;
     934:	3d7c 0001 0044 	move.w #1,68(a6)
    }

    dev->lib.lib_OpenCnt++;
     93a:	526e 0020      	addq.w #1,32(a6)
    ioreq->io_Error = 0; //Success
     93e:	4229 001f      	clr.b 31(a1)
}
     942:	4e75           	rts
        ioreq->io_Error = TDERR_BadUnitNum;
     944:	137c 0020 001f 	move.b #32,31(a1)
}
     94a:	4e75           	rts
/* device dependent close function
!!! CAUTION: This function runs in a forbidden state !!!
This call is guaranteed to be single-threaded; only one task
will execute your Close at a time. */
static BPTR __attribute__((used, saveds)) close(struct DeviceBase *dev asm("a6"), struct IORequest *ioreq asm("a1"))
{
     94c:	48e7 2032      	movem.l d2/a2-a3/a6,-(sp)
     950:	244e           	movea.l a6,a2
    Trace((CONST_STRPTR) "running close()\n");
    dev->lib.lib_OpenCnt--;
     952:	302e 0020      	move.w 32(a6),d0
     956:	5340           	subq.w #1,d0
     958:	3d40 0020      	move.w d0,32(a6)

    if (dev->lib.lib_OpenCnt == 0 && (dev->lib.lib_Flags & LIBF_DELEXP))
     95c:	6608           	bne.s 966 966 _init_device+0x786
     95e:	082e 0003 000e 	btst #3,14(a6)
     964:	660a           	bne.s 970 970 _init_device+0x790
        return expunge(dev);

    return 0;
     966:	91c8           	suba.l a0,a0
}
     968:	2008           	move.l a0,d0
     96a:	4cdf 4c04      	movem.l (sp)+,d2/a2-a3/a6
     96e:	4e75           	rts
    if (dev->Task != NULL && dev->TaskActive == true) {
     970:	4aae 002e      	tst.l 46(a6)
     974:	6708           	beq.s 97e 97e _init_device+0x79e
     976:	102e 0036      	move.b 54(a6),d0
     97a:	6600 00ca      	bne.w a46 a46 _init_device+0x866
     97e:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     982:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     986:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     98a:	2009           	move.l a1,d0
     98c:	6706           	beq.s 994 994 _init_device+0x7b4
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     98e:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     994:	2268 0078      	movea.l 120(a0),a1
     998:	2009           	move.l a1,d0
     99a:	6706           	beq.s 9a2 9a2 _init_device+0x7c2
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     99c:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     9a2:	2268 00ca      	movea.l 202(a0),a1
     9a6:	2009           	move.l a1,d0
     9a8:	6706           	beq.s 9b0 9b0 _init_device+0x7d0
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     9aa:	0029 0002 000e 	ori.b #2,14(a1)
        if (dev->units[i].cd != NULL) {
     9b0:	2068 011c      	movea.l 284(a0),a0
     9b4:	2008           	move.l a0,d0
     9b6:	6706           	beq.s 9be 9be _init_device+0x7de
            dev->units[i].cd->cd_Flags |= CDF_CONFIGME;
     9b8:	0028 0002 000e 	ori.b #2,14(a0)
    if (dev->TimeReq->tr_node.io_Device) CloseDevice((struct IORequest *)dev->TimeReq);
     9be:	226a 003c      	movea.l 60(a2),a1
     9c2:	4aa9 0014      	tst.l 20(a1)
     9c6:	670a           	beq.s 9d2 9d2 _init_device+0x7f2
     9c8:	2c42           	movea.l d2,a6
     9ca:	4eae fe3e      	jsr -450(a6)
     9ce:	226a 003c      	movea.l 60(a2),a1
    if (dev->TimeReq) DeleteExtIO((struct IORequest *)dev->TimeReq);
     9d2:	2009           	move.l a1,d0
     9d4:	670a           	beq.s 9e0 9e0 _init_device+0x800
     9d6:	2f09           	move.l a1,-(sp)
     9d8:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
     9de:	588f           	addq.l #4,sp
    if (dev->TimerMP) DeletePort(dev->TimerMP);
     9e0:	202a 0038      	move.l 56(a2),d0
     9e4:	670a           	beq.s 9f0 9f0 _init_device+0x810
     9e6:	2f00           	move.l d0,-(sp)
     9e8:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
     9ee:	588f           	addq.l #4,sp
    if (dev->ExpansionBase) CloseLibrary((struct Library *)dev->ExpansionBase);
     9f0:	226a 0026      	movea.l 38(a2),a1
     9f4:	2009           	move.l a1,d0
     9f6:	6706           	beq.s 9fe 9fe _init_device+0x81e
     9f8:	2c42           	movea.l d2,a6
     9fa:	4eae fe62      	jsr -414(a6)
    if (dev->units) FreeMem(dev->units,sizeof(struct IDEUnit) * MAX_UNITS);
     9fe:	226a 0048      	movea.l 72(a2),a1
     a02:	2009           	move.l a1,d0
     a04:	670c           	beq.s a12 a12 _init_device+0x832
     a06:	203c 0000 0148 	move.l #328,d0
     a0c:	2c42           	movea.l d2,a6
     a0e:	4eae ff2e      	jsr -210(a6)
    BPTR seg_list = dev->saved_seg_list;
     a12:	266a 0040      	movea.l 64(a2),a3
    Remove(&dev->lib.lib_Node);
     a16:	224a           	movea.l a2,a1
     a18:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     a1e:	4eae ff04      	jsr -252(a6)
    FreeMem((char *)dev - dev->lib.lib_NegSize, dev->lib.lib_NegSize + dev->lib.lib_PosSize);
     a22:	7000           	moveq #0,d0
     a24:	302a 0010      	move.w 16(a2),d0
     a28:	7200           	moveq #0,d1
     a2a:	322a 0012      	move.w 18(a2),d1
     a2e:	224a           	movea.l a2,a1
     a30:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     a36:	93c0           	suba.l d0,a1
     a38:	d081           	add.l d1,d0
     a3a:	4eae ff2e      	jsr -210(a6)
}
     a3e:	200b           	move.l a3,d0
     a40:	4cdf 4c04      	movem.l (sp)+,d2/a2-a3/a6
     a44:	4e75           	rts
        if ((mp = CreatePort(NULL,0)) == NULL)
     a46:	42a7           	clr.l -(sp)
     a48:	42a7           	clr.l -(sp)
     a4a:	4eb9 0000 3328 	jsr 3328 3328 _CreatePort
     a50:	2400           	move.l d0,d2
     a52:	508f           	addq.l #8,sp
     a54:	6700 ff10      	beq.w 966 966 _init_device+0x786
        if ((ioreq = CreateStdIO(mp)) == NULL) {
     a58:	2f00           	move.l d0,-(sp)
     a5a:	4eb9 0000 3426 	jsr 3426 3426 _CreateStdIO
     a60:	2640           	movea.l d0,a3
     a62:	588f           	addq.l #4,sp
     a64:	4a80           	tst.l d0
     a66:	674a           	beq.s ab2 ab2 _init_device+0x8d2
        ioreq->io_Command = CMD_DIE; // Tell ide_task to shut down
     a68:	377c 000a 001c 	move.w #10,28(a3)
        PutMsg(dev->TaskMP,(struct Message *)ioreq);
     a6e:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     a74:	2240           	movea.l d0,a1
     a76:	206a 0032      	movea.l 50(a2),a0
     a7a:	4eae fe92      	jsr -366(a6)
        WaitPort(mp);                // Wait for ide_task to signal that it is shutting down
     a7e:	2042           	movea.l d2,a0
     a80:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     a86:	4eae fe80      	jsr -384(a6)
        if (ioreq) DeleteStdIO(ioreq);
     a8a:	2f0b           	move.l a3,-(sp)
     a8c:	4eb9 0000 3436 	jsr 3436 3436 _DeleteExtIO
        if (mp) DeletePort(mp);
     a92:	2f02           	move.l d2,-(sp)
     a94:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
     a9a:	508f           	addq.l #8,sp
     a9c:	206a 0048      	movea.l 72(a2),a0
        if (dev->units[i].cd != NULL) {
     aa0:	2268 0026      	movea.l 38(a0),a1
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
     aa4:	2438 0004      	move.l 4 4 __start+0x4,d2
        if (dev->units[i].cd != NULL) {
     aa8:	2009           	move.l a1,d0
     aaa:	6600 fee2      	bne.w 98e 98e _init_device+0x7ae
     aae:	6000 fee4      	bra.w 994 994 _init_device+0x7b4
            DeletePort(mp);
     ab2:	2f02           	move.l d2,-(sp)
     ab4:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
}
     aba:	200b           	move.l a3,d0
            DeletePort(mp);
     abc:	588f           	addq.l #4,sp
}
     abe:	4cdf 4c04      	movem.l (sp)+,d2/a2-a3/a6
     ac2:	4e75           	rts
 * begin_io
 *
 * Handle immediate requests and send any others to ide_task
*/
static void __attribute__((used, saveds)) begin_io(struct DeviceBase *dev asm("a6"), struct IOStdReq *ioreq asm("a1"))
{
     ac4:	598f           	subq.l #4,sp
     ac6:	48e7 3c32      	movem.l d2-d5/a2-a3/a6,-(sp)
     aca:	204e           	movea.l a6,a0
    Trace((CONST_STRPTR) "running begin_io()\n");
    ioreq->io_Error = TDERR_NotSpecified;
     acc:	137c 0014 001f 	move.b #20,31(a1)

    if (dev->Task == NULL || dev->TaskActive == false) {
     ad2:	4aae 002e      	tst.l 46(a6)
     ad6:	6768           	beq.s b40 b40 _init_device+0x960
     ad8:	102e 0036      	move.b 54(a6),d0
     adc:	6762           	beq.s b40 b40 _init_device+0x960
        ioreq->io_Error = IOERR_OPENFAIL;
    }

    if (ioreq == NULL || ioreq->io_Unit == 0) return;
     ade:	2009           	move.l a1,d0
     ae0:	6756           	beq.s b38 b38 _init_device+0x958
     ae2:	2469 0018      	movea.l 24(a1),a2
     ae6:	200a           	move.l a2,d0
     ae8:	674e           	beq.s b38 b38 _init_device+0x958
    Trace("Command %04x%04x\n",ioreq->io_Command);
    switch (ioreq->io_Command) {
     aea:	3029 001c      	move.w 28(a1),d0
     aee:	0c40 0012      	cmpi.w #18,d0
     af2:	6700 015e      	beq.w c52 c52 _init_device+0xa72
     af6:	624e           	bhi.s b46 b46 _init_device+0x966
     af8:	0c40 0009      	cmpi.w #9,d0
     afc:	6720           	beq.s b1e b1e _init_device+0x93e
     afe:	6300 0126      	bls.w c26 c26 _init_device+0xa46
     b02:	0c40 000d      	cmpi.w #13,d0
     b06:	6700 00f2      	beq.w bfa bfa _init_device+0xa1a
     b0a:	6300 01aa      	bls.w cb6 cb6 _init_device+0xad6
     b0e:	0c40 000e      	cmpi.w #14,d0
     b12:	6700 0150      	beq.w c64 c64 _init_device+0xa84
     b16:	0c40 000f      	cmpi.w #15,d0
     b1a:	6600 0100      	bne.w c1c c1c _init_device+0xa3c
            ioreq->io_Actual = (((struct IDEUnit *)ioreq->io_Unit)->present) ? 0 : 1;
            ioreq->io_Error  = 0;
            break;

        case TD_PROTSTATUS:
            ioreq->io_Actual = 0; // Not protected
     b1e:	42a9 0020      	clr.l 32(a1)
            ioreq->io_Error  = 0;
     b22:	4229 001f      	clr.b 31(a1)

        default:
            Warn("Unknown command %d\n", ioreq->io_Command);
            ioreq->io_Error = IOERR_NOCMD;
    }
    if (ioreq && !(ioreq->io_Flags & IOF_QUICK)) {
     b26:	0829 0000 001e 	btst #0,30(a1)
     b2c:	660a           	bne.s b38 b38 _init_device+0x958
        ReplyMsg(&ioreq->io_Message);
     b2e:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     b34:	4eae fe86      	jsr -378(a6)
    }
}
     b38:	4cdf 4c3c      	movem.l (sp)+,d2-d5/a2-a3/a6
     b3c:	588f           	addq.l #4,sp
     b3e:	4e75           	rts
        ioreq->io_Error = IOERR_OPENFAIL;
     b40:	50e9 001f      	st 31(a1)
     b44:	6098           	bra.s ade ade _init_device+0x8fe
    switch (ioreq->io_Command) {
     b46:	0c40 001c      	cmpi.w #28,d0
     b4a:	6342           	bls.s b8e b8e _init_device+0x9ae
     b4c:	0c40 c001      	cmpi.w #-16383,d0
     b50:	6200 0146      	bhi.w c98 c98 _init_device+0xab8
     b54:	0c40 c000      	cmpi.w #-16384,d0
     b58:	6400 00dc      	bcc.w c36 c36 _init_device+0xa56
     b5c:	0c40 4000      	cmpi.w #16384,d0
     b60:	6600 00ba      	bne.w c1c c1c _init_device+0xa3c
            if (ioreq->io_Length >= sizeof(struct NSDeviceQueryResult))
     b64:	700f           	moveq #15,d0
     b66:	b0a9 0024      	cmp.l 36(a1),d0
     b6a:	6400 0122      	bcc.w c8e c8e _init_device+0xaae
                struct NSDeviceQueryResult *result = ioreq->io_Data;
     b6e:	2069 0028      	movea.l 40(a1),a0
     b72:	4298           	clr.l (a0)+
                result->SizeAvailable     = sizeof(struct NSDeviceQueryResult);
     b74:	7010           	moveq #16,d0
     b76:	20c0           	move.l d0,(a0)+
     b78:	20fc 0005 0000 	move.l #327680,(a0)+
     b7e:	20bc 0000 0014 	move.l #20,(a0)
                ioreq->io_Actual = sizeof(struct NSDeviceQueryResult);
     b84:	2340 0020      	move.l d0,32(a1)
                ioreq->io_Error = 0;
     b88:	4229 001f      	clr.b 31(a1)
     b8c:	6098           	bra.s b26 b26 _init_device+0x946
    switch (ioreq->io_Command) {
     b8e:	0c40 001b      	cmpi.w #27,d0
     b92:	6400 00a2      	bcc.w c36 c36 _init_device+0xa56
     b96:	0c40 0016      	cmpi.w #22,d0
     b9a:	6670           	bne.s c0c c0c _init_device+0xa2c
    struct DriveGeometry *geometry = (struct DriveGeometry *)ioreq->io_Data;
     b9c:	2669 0028      	movea.l 40(a1),a3
    geometry->dg_SectorSize   = unit->blockSize;
     ba0:	7000           	moveq #0,d0
     ba2:	302a 004a      	move.w 74(a2),d0
     ba6:	26c0           	move.l d0,(a3)+
    geometry->dg_TotalSectors = (unit->cylinders * unit->heads * unit->sectorsPerTrack);
     ba8:	3a2a 0044      	move.w 68(a2),d5
     bac:	362a 0046      	move.w 70(a2),d3
     bb0:	382a 0048      	move.w 72(a2),d4
     bb4:	7400           	moveq #0,d2
     bb6:	3404           	move.w d4,d2
     bb8:	2f02           	move.l d2,-(sp)
     bba:	3005           	move.w d5,d0
     bbc:	c0c3           	mulu.w d3,d0
     bbe:	2f00           	move.l d0,-(sp)
     bc0:	2f49 0024      	move.l a1,36(sp)
     bc4:	4eb9 0000 3514 	jsr 3514 3514 ___mulsi3
     bca:	508f           	addq.l #8,sp
     bcc:	26c0           	move.l d0,(a3)+
    geometry->dg_Cylinders    = unit->cylinders;
     bce:	7000           	moveq #0,d0
     bd0:	3005           	move.w d5,d0
     bd2:	26c0           	move.l d0,(a3)+
    geometry->dg_CylSectors   = (unit->sectorsPerTrack * unit->heads);
     bd4:	c8c3           	mulu.w d3,d4
     bd6:	26c4           	move.l d4,(a3)+
    geometry->dg_Heads        = unit->heads;
     bd8:	3003           	move.w d3,d0
     bda:	26c0           	move.l d0,(a3)+
     bdc:	26c2           	move.l d2,(a3)+
    geometry->dg_BufMemType   = MEMF_PUBLIC;
     bde:	7001           	moveq #1,d0
     be0:	26c0           	move.l d0,(a3)+
     be2:	16ea 0038      	move.b 56(a2),(a3)+
     be6:	4213           	clr.b (a3)
    ioreq->io_Error = 0;
     be8:	226f 001c      	movea.l 28(sp),a1
     bec:	4229 001f      	clr.b 31(a1)
    ioreq->io_Actual = sizeof(struct DriveGeometry);
     bf0:	7020           	moveq #32,d0
     bf2:	2340 0020      	move.l d0,32(a1)
     bf6:	6000 ff2e      	bra.w b26 b26 _init_device+0x946
            ioreq->io_Actual = ((struct IDEUnit *)ioreq->io_Unit)->change_count;
     bfa:	7000           	moveq #0,d0
     bfc:	302a 0042      	move.w 66(a2),d0
     c00:	2340 0020      	move.l d0,32(a1)
            ioreq->io_Error  = 0;
     c04:	4229 001f      	clr.b 31(a1)
            break;
     c08:	6000 ff1c      	bra.w b26 b26 _init_device+0x946
    switch (ioreq->io_Command) {
     c0c:	0c40 0016      	cmpi.w #22,d0
     c10:	650a           	bcs.s c1c c1c _init_device+0xa3c
     c12:	0640 ffe8      	addi.w #-24,d0
     c16:	0c40 0001      	cmpi.w #1,d0
     c1a:	631a           	bls.s c36 c36 _init_device+0xa56
            ioreq->io_Error = IOERR_NOCMD;
     c1c:	137c 00fd 001f 	move.b #-3,31(a1)
     c22:	6000 ff02      	bra.w b26 b26 _init_device+0x946
    switch (ioreq->io_Command) {
     c26:	0c40 0002      	cmpi.w #2,d0
     c2a:	65f0           	bcs.s c1c c1c _init_device+0xa3c
     c2c:	0c40 0003      	cmpi.w #3,d0
     c30:	624a           	bhi.s c7c c7c _init_device+0xa9c
            ioreq->io_Actual = 0; // Clear high offset for 32-bit commands
     c32:	42a9 0020      	clr.l 32(a1)
            ioreq->io_Flags &= ~IOF_QUICK;
     c36:	0229 00fe 001e 	andi.b #-2,30(a1)
            PutMsg(dev->TaskMP,&ioreq->io_Message);
     c3c:	2068 0032      	movea.l 50(a0),a0
     c40:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     c46:	4eae fe92      	jsr -366(a6)
}
     c4a:	4cdf 4c3c      	movem.l (sp)+,d2-d5/a2-a3/a6
     c4e:	588f           	addq.l #4,sp
     c50:	4e75           	rts
            ioreq->io_Actual = ((struct IDEUnit *)ioreq->io_Unit)->device_type;
     c52:	7000           	moveq #0,d0
     c54:	102a 0038      	move.b 56(a2),d0
     c58:	2340 0020      	move.l d0,32(a1)
            ioreq->io_Error  = 0;
     c5c:	4229 001f      	clr.b 31(a1)
            break;
     c60:	6000 fec4      	bra.w b26 b26 _init_device+0x946
            ioreq->io_Actual = (((struct IDEUnit *)ioreq->io_Unit)->present) ? 0 : 1;
     c64:	4a6a 0040      	tst.w 64(a2)
     c68:	57c0           	seq d0
     c6a:	4880           	ext.w d0
     c6c:	48c0           	ext.l d0
     c6e:	4480           	neg.l d0
     c70:	2340 0020      	move.l d0,32(a1)
            ioreq->io_Error  = 0;
     c74:	4229 001f      	clr.b 31(a1)
            break;
     c78:	6000 feac      	bra.w b26 b26 _init_device+0x946
    switch (ioreq->io_Command) {
     c7c:	0c40 0005      	cmpi.w #5,d0
     c80:	629a           	bhi.s c1c c1c _init_device+0xa3c
            ioreq->io_Actual = 0; // Not protected
     c82:	42a9 0020      	clr.l 32(a1)
            ioreq->io_Error  = 0;
     c86:	4229 001f      	clr.b 31(a1)
     c8a:	6000 fe9a      	bra.w b26 b26 _init_device+0x946
                ioreq->io_Error = IOERR_BADLENGTH;
     c8e:	137c 00fc 001f 	move.b #-4,31(a1)
     c94:	6000 fe90      	bra.w b26 b26 _init_device+0x946
    switch (ioreq->io_Command) {
     c98:	0c40 c003      	cmpi.w #-16381,d0
     c9c:	6600 ff7e      	bne.w c1c c1c _init_device+0xa3c
            ioreq->io_Flags &= ~IOF_QUICK;
     ca0:	0229 00fe 001e 	andi.b #-2,30(a1)
            PutMsg(dev->TaskMP,&ioreq->io_Message);
     ca6:	2068 0032      	movea.l 50(a0),a0
     caa:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     cb0:	4eae fe92      	jsr -366(a6)
     cb4:	6094           	bra.s c4a c4a _init_device+0xa6a
    switch (ioreq->io_Command) {
     cb6:	0c40 000b      	cmpi.w #11,d0
     cba:	6600 ff60      	bne.w c1c c1c _init_device+0xa3c
            ioreq->io_Flags &= ~IOF_QUICK;
     cbe:	0229 00fe 001e 	andi.b #-2,30(a1)
            PutMsg(dev->TaskMP,&ioreq->io_Message);
     cc4:	2068 0032      	movea.l 50(a0),a0
     cc8:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     cce:	4eae fe92      	jsr -366(a6)
     cd2:	6000 ff76      	bra.w c4a c4a _init_device+0xa6a
*/
static ULONG __attribute__((used, saveds)) abort_io(struct Library *dev asm("a6"), struct IOStdReq *ioreq asm("a1"))
{
    Trace((CONST_STRPTR) "running abort_io()\n");
    return IOERR_NOCMD;
}
     cd6:	70fd           	moveq #-3,d0
     cd8:	4e75           	rts
     cda:	0000 08fc      	.long _init_device+0x71c
     cde:	0000 094c      	.long _init_device+0x76c
     ce2:	0000 078c      	.long _init_device+0x5ac
     ce6:	0000 0000      	.long 0x00000000
     cea:	0000 0ac4      	.long _init_device+0x8e4
     cee:	0000 0cd6      	.long _init_device+0xaf6
     cf2:	ffff           	.short 0xffff
     cf4:	ffff           	.short 0xffff
 * init
 *
 * Create the device and add it to the system if init_device succeeds
*/
static struct Library __attribute__((used)) * init(BPTR seg_list asm("a0"))
{
     cf6:	2f0e           	move.l a6,-(sp)
     cf8:	2f0a           	move.l a2,-(sp)
    Info("Init driver.\n");
    SysBase = *(struct ExecBase **)4UL;
     cfa:	2c78 0004      	movea.l 4 4 __start+0x4,a6
{
     cfe:	2208           	move.l a0,d1
    SysBase = *(struct ExecBase **)4UL;
     d00:	23ce 0000 0004 	move.l a6,4 4 _SysBase
    struct DeviceBase *mydev = (struct DeviceBase *)MakeLibrary((ULONG *)&device_vectors,  // Vectors
     d06:	41fa ffd2      	lea cda cda _init_device+0xafa(pc),a0
     d0a:	93c9           	suba.l a1,a1
     d0c:	704c           	moveq #76,d0
     d0e:	45fa f4d0      	lea 1e0 1e0 _init_device(pc),a2
     d12:	4eae ffac      	jsr -84(a6)
     d16:	2440           	movea.l d0,a2
                                                        NULL,                      // InitStruct data
                                                        (APTR)init_device,         // Init function
                                                        sizeof(struct DeviceBase), // Library data size
                                                        seg_list);                 // Segment list

    if (mydev != NULL) {
     d18:	4a80           	tst.l d0
     d1a:	670c           	beq.s d28 d28 _init_device+0xb48
    Info("Add Device.\n");
        AddDevice((struct Device *)mydev);
     d1c:	2240           	movea.l d0,a1
     d1e:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     d24:	4eae fe50      	jsr -432(a6)
    }
    mount_drives(mydev->units[0].cd,mydev->lib.lib_Node.ln_Name);
     d28:	2f2a 000a      	move.l 10(a2),-(sp)
     d2c:	206a 0048      	movea.l 72(a2),a0
     d30:	2f28 0026      	move.l 38(a0),-(sp)
     d34:	4eb9 0000 32d4 	jsr 32d4 32d4 _mount_drives
    return (struct Library *)mydev;
}
     d3a:	200a           	move.l a2,d0
    return (struct Library *)mydev;
     d3c:	508f           	addq.l #8,sp
}
     d3e:	245f           	movea.l (sp)+,a2
     d40:	2c5f           	movea.l (sp)+,a6
     d42:	4e75           	rts

00000d44 00000d44 _MyGetSysTime:
#include "device.h"
#include "ata.h"

#define WAIT_TIMEOUT_MS 500

void MyGetSysTime(struct timerequest *tr) {
     d44:	2f0e           	move.l a6,-(sp)
     d46:	226f 0008      	movea.l 8(sp),a1
   tr->tr_node.io_Command = TR_GETSYSTIME;
     d4a:	337c 000a 001c 	move.w #10,28(a1)
   DoIO((struct IORequest *)tr);
     d50:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     d56:	4eae fe38      	jsr -456(a6)
}
     d5a:	2c5f           	movea.l (sp)+,a6
     d5c:	4e75           	rts

00000d5e 00000d5e _ata_identify:
 * Send an IDENTIFY command to the device and place the results in the buffer
 * 
 * returns fals on error
*/
bool ata_identify(struct IDEUnit *unit, UWORD *buffer)
{
     d5e:	4fef fff4      	lea -12(sp),sp
     d62:	48e7 303e      	movem.l d2-d3/a2-a6,-(sp)
     d66:	266f 002c      	movea.l 44(sp),a3
     d6a:	242f 0030      	move.l 48(sp),d2
    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive
     d6e:	246b 0032      	movea.l 50(a3),a2
     d72:	4a6b 003e      	tst.w 62(a3)
     d76:	6700 0102      	beq.w e7a e7a _ata_identify+0x11c
     d7a:	70e0           	moveq #-32,d0
     d7c:	1540 0c00      	move.b d0,3072(a2)
    *unit->drive->sectorCount = 0;
     d80:	157c 0000 0400 	move.b #0,1024(a2)
    *unit->drive->lbaLow  = 0;
     d86:	157c 0000 0600 	move.b #0,1536(a2)
    *unit->drive->lbaMid  = 0;
     d8c:	157c 0000 0800 	move.b #0,2048(a2)
    *unit->drive->lbaHigh = 0;
     d92:	157c 0000 0a00 	move.b #0,2560(a2)
    *unit->drive->error_features = 0;
     d98:	157c 0000 0200 	move.b #0,512(a2)
    *unit->drive->status_command = ATA_CMD_IDENTIFY;
     d9e:	157c 00ec 0e00 	move.b #-20,3584(a2)
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
     da4:	286b 002e      	movea.l 46(a3),a4
     da8:	262c 0014      	move.l 20(a4),d3
    if (unit->present == false) {
     dac:	4a6b 0040      	tst.w 64(a3)
     db0:	6632           	bne.s de4 de4 _ata_identify+0x86
        then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
     db2:	2f7c 0007 a120 	move.l #500000,36(sp)
     db8:	0024 
        then.tv_secs  = 0;
     dba:	42af 0020      	clr.l 32(sp)
   tr->tr_node.io_Command = TR_GETSYSTIME;
     dbe:	397c 000a 001c 	move.w #10,28(a4)
   DoIO((struct IORequest *)tr);
     dc4:	224c           	movea.l a4,a1
     dc6:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     dcc:	4eae fe38      	jsr -456(a6)
        AddTime(&then,&tr->tr_time);
     dd0:	2c43           	movea.l d3,a6
     dd2:	43ec 0020      	lea 32(a4),a1
     dd6:	41ef 0020      	lea 32(sp),a0
     dda:	4eae ffd6      	jsr -42(a6)
     dde:	246b 0032      	movea.l 50(a3),a2
        tr = unit->TimeReq;
     de2:	2a4c           	movea.l a4,a5
            if (CmpTime(&then,&tr->tr_time) == 1) {
     de4:	49ed 0020      	lea 32(a5),a4
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_drq | ata_flag_busy | ata_flag_error)) == ata_flag_drq) return true;
     de8:	1f6a 0e00 001f 	move.b 3584(a2),31(sp)
     dee:	102f 001f      	move.b 31(sp),d0
     df2:	7276           	moveq #118,d1
     df4:	4601           	not.b d1
     df6:	c081           	and.l d1,d0
     df8:	7208           	moveq #8,d1
     dfa:	b280           	cmp.l d0,d1
     dfc:	6744           	beq.s e42 e42 _ata_identify+0xe4
        if (unit->present == false) {
     dfe:	4a6b 0040      	tst.w 64(a3)
     e02:	66e4           	bne.s de8 de8 _ata_identify+0x8a
   tr->tr_node.io_Command = TR_GETSYSTIME;
     e04:	3b7c 000a 001c 	move.w #10,28(a5)
   DoIO((struct IORequest *)tr);
     e0a:	224d           	movea.l a5,a1
     e0c:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     e12:	4eae fe38      	jsr -456(a6)
            if (CmpTime(&then,&tr->tr_time) == 1) {
     e16:	2c43           	movea.l d3,a6
     e18:	224c           	movea.l a4,a1
     e1a:	41ef 0020      	lea 32(sp),a0
     e1e:	4eae ffca      	jsr -54(a6)
     e22:	7201           	moveq #1,d1
     e24:	b280           	cmp.l d0,d1
     e26:	6758           	beq.s e80 e80 _ata_identify+0x122
     e28:	246b 0032      	movea.l 50(a3),a2
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_drq | ata_flag_busy | ata_flag_error)) == ata_flag_drq) return true;
     e2c:	1f6a 0e00 001f 	move.b 3584(a2),31(sp)
     e32:	102f 001f      	move.b 31(sp),d0
     e36:	7276           	moveq #118,d1
     e38:	4601           	not.b d1
     e3a:	c081           	and.l d1,d0
     e3c:	7208           	moveq #8,d1
     e3e:	b280           	cmp.l d0,d1
     e40:	66bc           	bne.s dfe dfe _ata_identify+0xa0
        unit->last_error[4] = *unit->drive->status_command;
        }
        return false;
    }

    if (buffer) {
     e42:	4a82           	tst.l d2
     e44:	6728           	beq.s e6e e6e _ata_identify+0x110
     e46:	2242           	movea.l d2,a1
     e48:	307c 0100      	movea.w #256,a0
     e4c:	7200           	moveq #0,d1
     e4e:	4601           	not.b d1
     e50:	203c 0000 0100 	move.l #256,d0
     e56:	9088           	sub.l a0,d0
        UWORD read_data;
        for (int i=0; i<256; i++) {
            read_data = unit->drive->data[i];
     e58:	d080           	add.l d0,d0
     e5a:	3032 0800      	move.w (0,a2,d0.l),d0
            buffer[i] = ((read_data >> 8) | (read_data << 8));
     e5e:	e058           	ror.w #8,d0
     e60:	32c0           	move.w d0,(a1)+
     e62:	5388           	subq.l #1,a0
        for (int i=0; i<256; i++) {
     e64:	51c9 ffea      	dbf d1,e50 e50 _ata_identify+0xf2
     e68:	4241           	clr.w d1
     e6a:	5381           	subq.l #1,d1
     e6c:	64e2           	bcc.s e50 e50 _ata_identify+0xf2
        return false;
     e6e:	7001           	moveq #1,d0
        }
    }

    return true;
}
     e70:	4cdf 7c0c      	movem.l (sp)+,d2-d3/a2-a6
     e74:	4fef 000c      	lea 12(sp),sp
     e78:	4e75           	rts
    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive
     e7a:	70f0           	moveq #-16,d0
     e7c:	6000 fefe      	bra.w d7c d7c _ata_identify+0x1e
        if (*unit->drive->status_command & ata_flag_error) {
     e80:	206b 0032      	movea.l 50(a3),a0
     e84:	1028 0e00      	move.b 3584(a0),d0
     e88:	0200 0001      	andi.b #1,d0
     e8c:	67e2           	beq.s e70 e70 _ata_identify+0x112
        unit->last_error[0] = *unit->drive->error_features;
     e8e:	47eb 0039      	lea 57(a3),a3
     e92:	16e8 0200      	move.b 512(a0),(a3)+
        unit->last_error[1] = *unit->drive->lbaHigh;
     e96:	16e8 0a00      	move.b 2560(a0),(a3)+
        unit->last_error[2] = *unit->drive->lbaMid;
     e9a:	16e8 0800      	move.b 2048(a0),(a3)+
        unit->last_error[3] = *unit->drive->lbaLow;
     e9e:	16e8 0600      	move.b 1536(a0),(a3)+
        unit->last_error[4] = *unit->drive->status_command;
     ea2:	16a8 0e00      	move.b 3584(a0),(a3)
        return false;
     ea6:	4200           	clr.b d0
}
     ea8:	4cdf 7c0c      	movem.l (sp)+,d2-d3/a2-a6
     eac:	4fef 000c      	lea 12(sp),sp
     eb0:	4e75           	rts

00000eb2 00000eb2 _ata_init_unit:
 * 
 * Initialize a unit, check if it is there and responding
 * 
 * returns false on error
*/
bool ata_init_unit(struct IDEUnit *unit) {
     eb2:	518f           	subq.l #8,sp
     eb4:	48e7 3c3a      	movem.l d2-d5/a2-a4/a6,-(sp)
     eb8:	246f 002c      	movea.l 44(sp),a2
    struct ExecBase *SysBase = unit->SysBase;
     ebc:	262a 002a      	move.l 42(a2),d3

    unit->cylinders       = 0;
     ec0:	426a 0044      	clr.w 68(a2)
    unit->heads           = 0;
     ec4:	426a 0046      	clr.w 70(a2)
    unit->sectorsPerTrack = 0;
     ec8:	426a 0048      	clr.w 72(a2)
    unit->blockSize       = 0;
     ecc:	426a 004a      	clr.w 74(a2)
    unit->present         = false;
     ed0:	426a 0040      	clr.w 64(a2)
    ULONG offset;
    UWORD *buf;

    bool dev_found = false;

    offset = (unit->channel == 0) ? CHANNEL_0 : CHANNEL_1;
     ed4:	4a2a 0037      	tst.b 55(a2)
     ed8:	6600 009a      	bne.w f74 f74 _ata_init_unit+0xc2
     edc:	307c 1000      	movea.w #4096,a0
    unit->drive = (void *)unit->cd->cd_BoardAddr + offset; // Pointer to drive base
     ee0:	226a 0026      	movea.l 38(a2),a1
     ee4:	d1e9 0020      	adda.l 32(a1),a0
     ee8:	2548 0032      	move.l a0,50(a2)

    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive
     eec:	4a6a 003e      	tst.w 62(a2)
     ef0:	677e           	beq.s f70 f70 _ata_init_unit+0xbe
     ef2:	70e0           	moveq #-32,d0
     ef4:	1140 0c00      	move.b d0,3072(a0)

    for (int i=0; i<(8*NEXT_REG); i+=NEXT_REG) {
        // Check if the bus is floating (D7/6 pulled-up with resistors)
        if ((*((volatile UBYTE *)unit->drive->data + i) & 0xC0) != 0xC0) {
     ef8:	1010           	move.b (a0),d0
     efa:	0200 00c0      	andi.b #-64,d0
     efe:	0c00 00c0      	cmpi.b #-64,d0
     f02:	6678           	bne.s f7c f7c _ata_init_unit+0xca
     f04:	1028 0200      	move.b 512(a0),d0
     f08:	0200 00c0      	andi.b #-64,d0
     f0c:	0c00 00c0      	cmpi.b #-64,d0
     f10:	666a           	bne.s f7c f7c _ata_init_unit+0xca
     f12:	1028 0400      	move.b 1024(a0),d0
     f16:	0200 00c0      	andi.b #-64,d0
     f1a:	0c00 00c0      	cmpi.b #-64,d0
     f1e:	665c           	bne.s f7c f7c _ata_init_unit+0xca
     f20:	1028 0600      	move.b 1536(a0),d0
     f24:	0200 00c0      	andi.b #-64,d0
     f28:	0c00 00c0      	cmpi.b #-64,d0
     f2c:	664e           	bne.s f7c f7c _ata_init_unit+0xca
     f2e:	1028 0800      	move.b 2048(a0),d0
     f32:	0200 00c0      	andi.b #-64,d0
     f36:	0c00 00c0      	cmpi.b #-64,d0
     f3a:	6640           	bne.s f7c f7c _ata_init_unit+0xca
     f3c:	1028 0a00      	move.b 2560(a0),d0
     f40:	0200 00c0      	andi.b #-64,d0
     f44:	0c00 00c0      	cmpi.b #-64,d0
     f48:	6632           	bne.s f7c f7c _ata_init_unit+0xca
     f4a:	1028 0c00      	move.b 3072(a0),d0
     f4e:	0200 00c0      	andi.b #-64,d0
     f52:	0c00 00c0      	cmpi.b #-64,d0
     f56:	6624           	bne.s f7c f7c _ata_init_unit+0xca
     f58:	1028 0e00      	move.b 3584(a0),d0
     f5c:	0200 00c0      	andi.b #-64,d0
     f60:	0c00 00c0      	cmpi.b #-64,d0
     f64:	6616           	bne.s f7c f7c _ata_init_unit+0xca

    unit->present = true;

    if (buf) FreeMem(buf,512);
    return true;
}
     f66:	4200           	clr.b d0
     f68:	4cdf 5c3c      	movem.l (sp)+,d2-d5/a2-a4/a6
     f6c:	508f           	addq.l #8,sp
     f6e:	4e75           	rts
    *unit->drive->devHead = (unit->primary) ? 0xE0 : 0xF0; // Select drive
     f70:	70f0           	moveq #-16,d0
     f72:	6080           	bra.s ef4 ef4 _ata_init_unit+0x42
    offset = (unit->channel == 0) ? CHANNEL_0 : CHANNEL_1;
     f74:	307c 2000      	movea.w #8192,a0
     f78:	6000 ff66      	bra.w ee0 ee0 _ata_init_unit+0x2e
     f7c:	266a 002e      	movea.l 46(a2),a3
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
     f80:	242b 0014      	move.l 20(a3),d2
    then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
     f84:	2f7c 0007 a120 	move.l #500000,36(sp)
     f8a:	0024 
    then.tv_secs  = 0;
     f8c:	42af 0020      	clr.l 32(sp)
   tr->tr_node.io_Command = TR_GETSYSTIME;
     f90:	377c 000a 001c 	move.w #10,28(a3)
   DoIO((struct IORequest *)tr);
     f96:	224b           	movea.l a3,a1
     f98:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     f9e:	4eae fe38      	jsr -456(a6)
    AddTime(&then,&tr->tr_time);
     fa2:	49eb 0020      	lea 32(a3),a4
     fa6:	2c42           	movea.l d2,a6
     fa8:	224c           	movea.l a4,a1
     faa:	41ef 0020      	lea 32(sp),a0
     fae:	4eae ffd6      	jsr -42(a6)
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_busy) == 0) return true;
     fb2:	206a 0032      	movea.l 50(a2),a0
     fb6:	1028 0e00      	move.b 3584(a0),d0
     fba:	6c2e           	bge.s fea fea _ata_init_unit+0x138
   tr->tr_node.io_Command = TR_GETSYSTIME;
     fbc:	377c 000a 001c 	move.w #10,28(a3)
   DoIO((struct IORequest *)tr);
     fc2:	224b           	movea.l a3,a1
     fc4:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
     fca:	4eae fe38      	jsr -456(a6)
        if (CmpTime(&then,&tr->tr_time) == 1) {
     fce:	2c42           	movea.l d2,a6
     fd0:	224c           	movea.l a4,a1
     fd2:	41ef 0020      	lea 32(sp),a0
     fd6:	4eae ffca      	jsr -54(a6)
     fda:	7201           	moveq #1,d1
     fdc:	b280           	cmp.l d0,d1
     fde:	6786           	beq.s f66 f66 _ata_init_unit+0xb4
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_busy) == 0) return true;
     fe0:	206a 0032      	movea.l 50(a2),a0
     fe4:	1028 0e00      	move.b 3584(a0),d0
     fe8:	6dd2           	blt.s fbc fbc _ata_init_unit+0x10a
    if ((buf = AllocMem(512,MEMF_ANY|MEMF_CLEAR)) == NULL) // Allocate buffer for IDENTIFY result
     fea:	2c43           	movea.l d3,a6
     fec:	203c 0000 0200 	move.l #512,d0
     ff2:	7201           	moveq #1,d1
     ff4:	4841           	swap d1
     ff6:	4eae ff3a      	jsr -198(a6)
     ffa:	2640           	movea.l d0,a3
     ffc:	4a80           	tst.l d0
     ffe:	6700 ff66      	beq.w f66 f66 _ata_init_unit+0xb4
    if (ata_identify(unit,buf) == false) {
    1002:	2f0b           	move.l a3,-(sp)
    1004:	2f0a           	move.l a2,-(sp)
    1006:	4eba fd56      	jsr d5e d5e _ata_identify(pc)
    100a:	1400           	move.b d0,d2
    100c:	508f           	addq.l #8,sp
    100e:	674e           	beq.s 105e 105e _ata_init_unit+0x1ac
    unit->cylinders       = *((UWORD *)buf + ata_identify_cylinders);
    1010:	41ea 0044      	lea 68(a2),a0
    1014:	30eb 0002      	move.w 2(a3),(a0)+
    unit->heads           = *((UWORD *)buf + ata_identify_heads);
    1018:	30eb 0006      	move.w 6(a3),(a0)+
    unit->sectorsPerTrack = *((UWORD *)buf + ata_identify_sectors);
    101c:	30eb 000c      	move.w 12(a3),(a0)+
    unit->blockSize       = *((UWORD *)buf + ata_identify_sectorsize);
    1020:	382b 000a      	move.w 10(a3),d4
    1024:	3084           	move.w d4,(a0)
    unit->logicalSectors  = *((ULONG *)buf + (ata_identify_logical_sectors/2));
    1026:	256b 0078 004e 	move.l 120(a3),78(a2)
    unit->blockShift      = 0;
    102c:	426a 004c      	clr.w 76(a2)
    if (unit->blockSize == 0) {
    1030:	4a44           	tst.w d4
    1032:	6740           	beq.s 1074 1074 _ata_init_unit+0x1c2
    while ((unit->blockSize >> unit->blockShift) > 1) {
    1034:	7a00           	moveq #0,d5
    1036:	3a04           	move.w d4,d5
    1038:	4240           	clr.w d0
    103a:	7201           	moveq #1,d1
    103c:	b285           	cmp.l d5,d1
    103e:	6716           	beq.s 1056 1056 _ata_init_unit+0x1a4
        unit->blockShift++;
    1040:	5240           	addq.w #1,d0
    while ((unit->blockSize >> unit->blockShift) > 1) {
    1042:	7200           	moveq #0,d1
    1044:	3200           	move.w d0,d1
    1046:	2805           	move.l d5,d4
    1048:	e2a4           	asr.l d1,d4
    104a:	2204           	move.l d4,d1
    104c:	7801           	moveq #1,d4
    104e:	b881           	cmp.l d1,d4
    1050:	6dee           	blt.s 1040 1040 _ata_init_unit+0x18e
    1052:	3540 004c      	move.w d0,76(a2)
    unit->present = true;
    1056:	357c 0001 0040 	move.w #1,64(a2)
    if (buf) FreeMem(buf,512);
    105c:	2c43           	movea.l d3,a6
    105e:	203c 0000 0200 	move.l #512,d0
    1064:	224b           	movea.l a3,a1
    1066:	4eae ff2e      	jsr -210(a6)
}
    106a:	1002           	move.b d2,d0
    106c:	4cdf 5c3c      	movem.l (sp)+,d2-d5/a2-a4/a6
    1070:	508f           	addq.l #8,sp
    1072:	4e75           	rts
        if (buf) FreeMem(buf,512);
    1074:	203c 0000 0200 	move.l #512,d0
    107a:	224b           	movea.l a3,a1
    107c:	4eae ff2e      	jsr -210(a6)
        return false;
    1080:	1204           	move.b d4,d1
}
    1082:	1001           	move.b d1,d0
    1084:	4cdf 5c3c      	movem.l (sp)+,d2-d5/a2-a4/a6
    1088:	508f           	addq.l #8,sp
    108a:	4e75           	rts

0000108c 0000108c _ata_transfer:
 * @param count Number of blocks to transfer
 * @param actual Pointer to the io requests io_Actual 
 * @param unit Pointer to the unit structure
 * @param direction READ/WRITE
*/
BYTE ata_transfer(void *buffer, ULONG lba, ULONG count, ULONG *actual, struct IDEUnit *unit, enum xfer_dir direction) {
    108c:	4fef ffd4      	lea -44(sp),sp
    1090:	48e7 3f3e      	movem.l d2-d7/a2-a6,-(sp)
    1094:	41ef 005c      	lea 92(sp),a0
    1098:	2f58 005c      	move.l (a0)+,92(sp)
    109c:	2f58 0060      	move.l (a0)+,96(sp)
    10a0:	2f58 0064      	move.l (a0)+,100(sp)
    10a4:	2f58 0068      	move.l (a0)+,104(sp)
    10a8:	2a58           	movea.l (a0)+,a5
    10aa:	2f50 0070      	move.l (a0),112(sp)
} else {
    Trace("ata_write");
}
    ULONG subcount = 0;
    ULONG offset = 0;
    *actual = 0;
    10ae:	206f 0068      	movea.l 104(sp),a0
    10b2:	4290           	clr.l (a0)

    if (count == 0) return TDERR_TooFewSecs;
    10b4:	4aaf 0064      	tst.l 100(sp)
    10b8:	660c           	bne.s 10c6 10c6 _ata_transfer+0x3a
    10ba:	701a           	moveq #26,d0
        lba += subcount;

    }

    return 0;
}
    10bc:	4cdf 7cfc      	movem.l (sp)+,d2-d7/a2-a6
    10c0:	4fef 002c      	lea 44(sp),sp
    10c4:	4e75           	rts
    BYTE drvSel = (unit->primary) ? 0xE0 : 0xF0;
    10c6:	4a6d 003e      	tst.w 62(a5)
    10ca:	6700 008e      	beq.w 115a 115a _ata_transfer+0xce
    10ce:	72e0           	moveq #-32,d1
    *unit->drive->devHead        = (UBYTE)(drvSel | ((lba >> 24) & 0x0F));
    10d0:	202f 0060      	move.l 96(sp),d0
    10d4:	4240           	clr.w d0
    10d6:	4840           	swap d0
    10d8:	e048           	lsr.w #8,d0
    10da:	0200 000f      	andi.b #15,d0
    10de:	8001           	or.b d1,d0
    10e0:	206d 0032      	movea.l 50(a5),a0
    10e4:	1140 0c00      	move.b d0,3072(a0)
    10e8:	266d 002e      	movea.l 46(a5),a3
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    10ec:	242b 0014      	move.l 20(a3),d2
    then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
    10f0:	2f7c 0007 a120 	move.l #500000,84(sp)
    10f6:	0054 
    then.tv_secs  = 0;
    10f8:	42af 0050      	clr.l 80(sp)
   tr->tr_node.io_Command = TR_GETSYSTIME;
    10fc:	377c 000a 001c 	move.w #10,28(a3)
   DoIO((struct IORequest *)tr);
    1102:	224b           	movea.l a3,a1
    1104:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    110a:	4eae fe38      	jsr -456(a6)
    AddTime(&then,&tr->tr_time);
    110e:	45eb 0020      	lea 32(a3),a2
    1112:	2c42           	movea.l d2,a6
    1114:	224a           	movea.l a2,a1
    1116:	41ef 0050      	lea 80(sp),a0
    111a:	4eae ffd6      	jsr -42(a6)
        if ((*(volatile BYTE *)unit->drive->status_command & ata_flag_busy) == 0) return true;
    111e:	206d 0032      	movea.l 50(a5),a0
    1122:	1028 0e00      	move.b 3584(a0),d0
    1126:	6c00 0086      	bge.w 11ae 11ae _ata_transfer+0x122
   tr->tr_node.io_Command = TR_GETSYSTIME;
    112a:	377c 000a 001c 	move.w #10,28(a3)
   DoIO((struct IORequest *)tr);
    1130:	224b           	movea.l a3,a1
    1132:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    1138:	4eae fe38      	jsr -456(a6)
        if (CmpTime(&then,&tr->tr_time) == 1) {
    113c:	2c42           	movea.l d2,a6
    113e:	224a           	movea.l a2,a1
    1140:	41ef 0050      	lea 80(sp),a0
    1144:	4eae ffca      	jsr -54(a6)
    1148:	7201           	moveq #1,d1
    114a:	b280           	cmp.l d0,d1
    114c:	66d0           	bne.s 111e 111e _ata_transfer+0x92
                return HFERR_BadStatus;
    114e:	702d           	moveq #45,d0
}
    1150:	4cdf 7cfc      	movem.l (sp)+,d2-d7/a2-a6
    1154:	4fef 002c      	lea 44(sp),sp
    1158:	4e75           	rts
    115a:	72f0           	moveq #-16,d1
    *unit->drive->devHead        = (UBYTE)(drvSel | ((lba >> 24) & 0x0F));
    115c:	202f 0060      	move.l 96(sp),d0
    1160:	4240           	clr.w d0
    1162:	4840           	swap d0
    1164:	e048           	lsr.w #8,d0
    1166:	0200 000f      	andi.b #15,d0
    116a:	8001           	or.b d1,d0
    116c:	206d 0032      	movea.l 50(a5),a0
    1170:	1140 0c00      	move.b d0,3072(a0)
    1174:	266d 002e      	movea.l 46(a5),a3
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    1178:	242b 0014      	move.l 20(a3),d2
    then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
    117c:	2f7c 0007 a120 	move.l #500000,84(sp)
    1182:	0054 
    then.tv_secs  = 0;
    1184:	42af 0050      	clr.l 80(sp)
   tr->tr_node.io_Command = TR_GETSYSTIME;
    1188:	377c 000a 001c 	move.w #10,28(a3)
   DoIO((struct IORequest *)tr);
    118e:	224b           	movea.l a3,a1
    1190:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    1196:	4eae fe38      	jsr -456(a6)
    AddTime(&then,&tr->tr_time);
    119a:	45eb 0020      	lea 32(a3),a2
    119e:	2c42           	movea.l d2,a6
    11a0:	224a           	movea.l a2,a1
    11a2:	41ef 0050      	lea 80(sp),a0
    11a6:	4eae ffd6      	jsr -42(a6)
    11aa:	6000 ff72      	bra.w 111e 111e _ata_transfer+0x92
    11ae:	4aaf 0070      	tst.l 112(sp)
    11b2:	6600 02b4      	bne.w 1468 1468 _ata_transfer+0x3dc
    11b6:	1f7c 0020 004f 	move.b #32,79(sp)
    11bc:	93c9           	suba.l a1,a1
    11be:	2f6f 0064 004a 	move.l 100(sp),74(sp)
    11c4:	0caf 0000 00ff 	cmpi.l #255,100(sp)
    11ca:	0064 
    11cc:	6308           	bls.s 11d6 11d6 _ata_transfer+0x14a
    11ce:	2f7c 0000 00ff 	move.l #255,74(sp)
    11d4:	004a 
        *unit->drive->sectorCount    = subcount; // Count value of 0 indicates to transfer 256 sectors
    11d6:	116f 004d 0400 	move.b 77(sp),1024(a0)
        *unit->drive->lbaLow         = (UBYTE)(lba);
    11dc:	116f 0063 0600 	move.b 99(sp),1536(a0)
        *unit->drive->lbaMid         = (UBYTE)(lba >> 8);
    11e2:	202f 0060      	move.l 96(sp),d0
    11e6:	e088           	lsr.l #8,d0
    11e8:	1140 0800      	move.b d0,2048(a0)
        *unit->drive->lbaHigh        = (UBYTE)(lba >> 16);
    11ec:	202f 0060      	move.l 96(sp),d0
    11f0:	4240           	clr.w d0
    11f2:	4840           	swap d0
    11f4:	1140 0a00      	move.b d0,2560(a0)
        *unit->drive->error_features = 0;
    11f8:	117c 0000 0200 	move.b #0,512(a0)
        *unit->drive->status_command = (direction == READ) ? ATA_CMD_READ : ATA_CMD_WRITE;
    11fe:	116f 004f 0e00 	move.b 79(sp),3584(a0)
    1204:	45e9 0200      	lea 512(a1),a2
    1208:	2f4a 0046      	move.l a2,70(sp)
    120c:	d3ef 005c      	adda.l 92(sp),a1
    1210:	2f49 003e      	move.l a1,62(sp)
    1214:	42af 0042      	clr.l 66(sp)
    1218:	2f48 0032      	move.l a0,50(sp)
    121c:	2f4d 002e      	move.l a5,46(sp)
    struct Device *TimerBase = unit->TimeReq->tr_node.io_Device;
    1220:	246d 002e      	movea.l 46(a5),a2
    1224:	242a 0014      	move.l 20(a2),d2
    if (unit->present == false) {
    1228:	4a6d 0040      	tst.w 64(a5)
    122c:	663a           	bne.s 1268 1268 _ata_transfer+0x1dc
        then.tv_micro = (WAIT_TIMEOUT_MS * 1000);
    122e:	2f7c 0007 a120 	move.l #500000,84(sp)
    1234:	0054 
        then.tv_secs  = 0;
    1236:	42af 0050      	clr.l 80(sp)
   tr->tr_node.io_Command = TR_GETSYSTIME;
    123a:	357c 000a 001c 	move.w #10,28(a2)
   DoIO((struct IORequest *)tr);
    1240:	224a           	movea.l a2,a1
    1242:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    1248:	4eae fe38      	jsr -456(a6)
        AddTime(&then,&tr->tr_time);
    124c:	2c42           	movea.l d2,a6
    124e:	43ea 0020      	lea 32(a2),a1
    1252:	41ef 0050      	lea 80(sp),a0
    1256:	4eae ffd6      	jsr -42(a6)
    125a:	2f6d 0032 0032 	move.l 50(a5),50(sp)
        tr = unit->TimeReq;
    1260:	2f4a 003a      	move.l a2,58(sp)
    1264:	2a6f 002e      	movea.l 46(sp),a5
            if (CmpTime(&then,&tr->tr_time) == 1) {
    1268:	347c 0020      	movea.w #32,a2
    126c:	d5ef 003a      	adda.l 58(sp),a2
    1270:	206f 0032      	movea.l 50(sp),a0
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_drq | ata_flag_busy | ata_flag_error)) == ata_flag_drq) return true;
    1274:	1f68 0e00 0039 	move.b 3584(a0),57(sp)
    127a:	102f 0039      	move.b 57(sp),d0
    127e:	7276           	moveq #118,d1
    1280:	4601           	not.b d1
    1282:	c081           	and.l d1,d0
    1284:	7208           	moveq #8,d1
    1286:	b280           	cmp.l d0,d1
    1288:	674a           	beq.s 12d4 12d4 _ata_transfer+0x248
        if (unit->present == false) {
    128a:	4a6d 0040      	tst.w 64(a5)
    128e:	66e4           	bne.s 1274 1274 _ata_transfer+0x1e8
   tr->tr_node.io_Command = TR_GETSYSTIME;
    1290:	206f 003a      	movea.l 58(sp),a0
    1294:	317c 000a 001c 	move.w #10,28(a0)
   DoIO((struct IORequest *)tr);
    129a:	2248           	movea.l a0,a1
    129c:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    12a2:	4eae fe38      	jsr -456(a6)
            if (CmpTime(&then,&tr->tr_time) == 1) {
    12a6:	2c42           	movea.l d2,a6
    12a8:	224a           	movea.l a2,a1
    12aa:	41ef 0050      	lea 80(sp),a0
    12ae:	4eae ffca      	jsr -54(a6)
    12b2:	7201           	moveq #1,d1
    12b4:	b280           	cmp.l d0,d1
    12b6:	6700 fe96      	beq.w 114e 114e _ata_transfer+0xc2
    12ba:	206d 0032      	movea.l 50(a5),a0
        if ((*(volatile BYTE *)unit->drive->status_command & (ata_flag_drq | ata_flag_busy | ata_flag_error)) == ata_flag_drq) return true;
    12be:	1f68 0e00 0039 	move.b 3584(a0),57(sp)
    12c4:	102f 0039      	move.b 57(sp),d0
    12c8:	7276           	moveq #118,d1
    12ca:	4601           	not.b d1
    12cc:	c081           	and.l d1,d0
    12ce:	7208           	moveq #8,d1
    12d0:	b280           	cmp.l d0,d1
    12d2:	66b6           	bne.s 128a 128a _ata_transfer+0x1fe
    12d4:	2f48 0032      	move.l a0,50(sp)
    12d8:	2f4d 002e      	move.l a5,46(sp)
            if (direction == READ) {
    12dc:	4aaf 0070      	tst.l 112(sp)
    12e0:	6600 00bc      	bne.w 139e 139e _ata_transfer+0x312
                read_fast((void *)(unit->drive->error_features - 48),(buffer + offset));
    12e4:	41e8 01d0      	lea 464(a0),a0
    12e8:	2f48 0036      	move.l a0,54(sp)
 * 
 * @param source Pointer to drive data port
 * @param destination Pointer to source buffer
*/
void read_fast (void *source, void *destinaton) {
    asm volatile ("moveq  #48,d7\n\t"
    12ec:	2a6f 003e      	movea.l 62(sp),a5
    12f0:	7e30           	moveq #48,d7
    12f2:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    12f6:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    12fa:	dbc7           	adda.l d7,a5
    12fc:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    1300:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1304:	dbc7           	adda.l d7,a5
    1306:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    130a:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    130e:	dbc7           	adda.l d7,a5
    1310:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    1314:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1318:	dbc7           	adda.l d7,a5
    131a:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    131e:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1322:	dbc7           	adda.l d7,a5
    1324:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    1328:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    132c:	dbc7           	adda.l d7,a5
    132e:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    1332:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1336:	dbc7           	adda.l d7,a5
    1338:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    133c:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1340:	dbc7           	adda.l d7,a5
    1342:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    1346:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    134a:	dbc7           	adda.l d7,a5
    134c:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    1350:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1354:	dbc7           	adda.l d7,a5
    1356:	4ce8 027f 0010 	movem.l 16(a0),d0-d6/a1
    135c:	48d5 027f      	movem.l d0-d6/a1,(a5)
            *actual += unit->blockSize;
    1360:	206f 002e      	movea.l 46(sp),a0
    1364:	7000           	moveq #0,d0
    1366:	246f 0068      	movea.l 104(sp),a2
    136a:	3028 004a      	move.w 74(a0),d0
    136e:	d192           	add.l d0,(a2)
        for (int block = 0; block < subcount; block++) {
    1370:	52af 0042      	addq.l #1,66(sp)
    1374:	203c 0000 0200 	move.l #512,d0
    137a:	d0af 0046      	add.l 70(sp),d0
    137e:	06af 0000 0200 	addi.l #512,62(sp)
    1384:	003e 
    1386:	246f 0042      	movea.l 66(sp),a2
    138a:	b5ef 004a      	cmpa.l 74(sp),a2
    138e:	6700 00a4      	beq.w 1434 1434 _ata_transfer+0x3a8
    1392:	2f40 0046      	move.l d0,70(sp)
    1396:	2a6f 002e      	movea.l 46(sp),a5
    139a:	6000 fe84      	bra.w 1220 1220 _ata_transfer+0x194
                write_fast((buffer + offset),(void *)(unit->drive->error_features - 48));
    139e:	41e8 01d0      	lea 464(a0),a0
    13a2:	2f48 0036      	move.l a0,54(sp)
 * 
 * @param source Pointer to source buffer
 * @param destination Pointer to drive data port
*/
void write_fast (void *source, void *destinaton) {
    asm volatile (
    13a6:	2a6f 003e      	movea.l 62(sp),a5
    13aa:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13ae:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13b2:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13b6:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13ba:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13be:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13c2:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13c6:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13ca:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13ce:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13d2:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13d6:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13da:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13de:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13e2:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13e6:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13ea:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13ee:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13f2:	4cdd 5e7f      	movem.l (a5)+,d0-d6/a1-a4/a6
    13f6:	48d0 5e7f      	movem.l d0-d6/a1-a4/a6,(a0)
    13fa:	4cdd 027f      	movem.l (a5)+,d0-d6/a1
    13fe:	48d0 027f      	movem.l d0-d6/a1,(a0)
            *actual += unit->blockSize;
    1402:	206f 002e      	movea.l 46(sp),a0
    1406:	7000           	moveq #0,d0
    1408:	246f 0068      	movea.l 104(sp),a2
    140c:	3028 004a      	move.w 74(a0),d0
    1410:	d192           	add.l d0,(a2)
        for (int block = 0; block < subcount; block++) {
    1412:	52af 0042      	addq.l #1,66(sp)
    1416:	203c 0000 0200 	move.l #512,d0
    141c:	d0af 0046      	add.l 70(sp),d0
    1420:	06af 0000 0200 	addi.l #512,62(sp)
    1426:	003e 
    1428:	222f 004a      	move.l 74(sp),d1
    142c:	b2af 0042      	cmp.l 66(sp),d1
    1430:	6600 ff60      	bne.w 1392 1392 _ata_transfer+0x306
    1434:	2a48           	movea.l a0,a5
    1436:	206f 0032      	movea.l 50(sp),a0
        if (*unit->drive->status_command & ata_flag_error) {
    143a:	0828 0000 0e00 	btst #0,3584(a0)
    1440:	6632           	bne.s 1474 1474 _ata_transfer+0x3e8
        count -= subcount;
    1442:	202f 004a      	move.l 74(sp),d0
    1446:	91af 0064      	sub.l d0,100(sp)
        lba += subcount;
    144a:	d1af 0060      	add.l d0,96(sp)
    144e:	226f 0046      	movea.l 70(sp),a1
    while (count > 0) {
    1452:	4aaf 0064      	tst.l 100(sp)
    1456:	6600 fd66      	bne.w 11be 11be _ata_transfer+0x132
    return 0;
    145a:	102f 0067      	move.b 103(sp),d0
}
    145e:	4cdf 7cfc      	movem.l (sp)+,d2-d7/a2-a6
    1462:	4fef 002c      	lea 44(sp),sp
    1466:	4e75           	rts
    1468:	1f7c 0030 004f 	move.b #48,79(sp)
    146e:	93c9           	suba.l a1,a1
    1470:	6000 fd4c      	bra.w 11be 11be _ata_transfer+0x132
            unit->last_error[0] = unit->drive->error_features[0];
    1474:	43ed 0039      	lea 57(a5),a1
    1478:	12e8 0200      	move.b 512(a0),(a1)+
            unit->last_error[1] = unit->drive->lbaHigh[0];
    147c:	12e8 0a00      	move.b 2560(a0),(a1)+
            unit->last_error[2] = unit->drive->lbaMid[0];
    1480:	12e8 0800      	move.b 2048(a0),(a1)+
            unit->last_error[3] = unit->drive->lbaLow[0];
    1484:	12e8 0600      	move.b 1536(a0),(a1)+
            unit->last_error[4] = unit->drive->status_command[0];
    1488:	12a8 0e00      	move.b 3584(a0),(a1)
            return TDERR_NotSpecified;
    148c:	7014           	moveq #20,d0
}
    148e:	4cdf 7cfc      	movem.l (sp)+,d2-d7/a2-a6
    1492:	4fef 002c      	lea 44(sp),sp
    1496:	4e75           	rts

00001498 00001498 _read_fast:
void read_fast (void *source, void *destinaton) {
    1498:	48e7 3f3e      	movem.l d2-d7/a2-a6,-(sp)
    asm volatile ("moveq  #48,d7\n\t"
    149c:	2a6f 0034      	movea.l 52(sp),a5
    14a0:	206f 0030      	movea.l 48(sp),a0
    14a4:	7e30           	moveq #48,d7
    14a6:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14aa:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14ae:	dbc7           	adda.l d7,a5
    14b0:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14b4:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14b8:	dbc7           	adda.l d7,a5
    14ba:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14be:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14c2:	dbc7           	adda.l d7,a5
    14c4:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14c8:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14cc:	dbc7           	adda.l d7,a5
    14ce:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14d2:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14d6:	dbc7           	adda.l d7,a5
    14d8:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14dc:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14e0:	dbc7           	adda.l d7,a5
    14e2:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14e6:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14ea:	dbc7           	adda.l d7,a5
    14ec:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14f0:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14f4:	dbc7           	adda.l d7,a5
    14f6:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    14fa:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    14fe:	dbc7           	adda.l d7,a5
    1500:	4cd0 5e7f      	movem.l (a0),d0-d6/a1-a4/a6
    1504:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1508:	dbc7           	adda.l d7,a5
    150a:	4ce8 027f 0010 	movem.l 16(a0),d0-d6/a1
    1510:	48d5 027f      	movem.l d0-d6/a1,(a5)
}
    1514:	4cdf 7cfc      	movem.l (sp)+,d2-d7/a2-a6
    1518:	4e75           	rts

0000151a 0000151a _write_fast:
void write_fast (void *source, void *destinaton) {
    151a:	48e7 3f3e      	movem.l d2-d7/a2-a6,-(sp)
    asm volatile (
    151e:	2a6f 0034      	movea.l 52(sp),a5
    1522:	206f 0030      	movea.l 48(sp),a0
    1526:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    152a:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    152e:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    1532:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1536:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    153a:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    153e:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    1542:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1546:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    154a:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    154e:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    1552:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1556:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    155a:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    155e:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    1562:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1566:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    156a:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    156e:	4cd8 5e7f      	movem.l (a0)+,d0-d6/a1-a4/a6
    1572:	48d5 5e7f      	movem.l d0-d6/a1-a4/a6,(a5)
    1576:	4cd8 027f      	movem.l (a0)+,d0-d6/a1
    157a:	48d5 027f      	movem.l d0-d6/a1,(a5)
    "movem.l d0-d6/a1,(%1)\n\t"
    :
    :"a" (source),"a" (destinaton)
    :"a1","a2","a3","a4","a6","d0","d1","d2","d3","d4","d5","d6","d7"
    );
}
    157e:	4cdf 7cfc      	movem.l (sp)+,d2-d7/a2-a6
    1582:	4e75           	rts

00001584 00001584 _ide_task:
 * ide_task
 *
 * This is a task to complete IO Requests for all units
 * Requests are sent here from begin_io via the dev->TaskMP Message port
*/
void __attribute__((noreturn)) ide_task () {
    1584:	4fef ffec      	lea -20(sp),sp
    1588:	48e7 3f3e      	movem.l d2-d7/a2-a6,-(sp)
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    158c:	2438 0004      	move.l 4 4 __start+0x4,d2
    struct Task volatile *task = FindTask(NULL);
    1590:	93c9           	suba.l a1,a1
    1592:	2c42           	movea.l d2,a6
    1594:	4eae feda      	jsr -294(a6)
    1598:	2040           	movea.l d0,a0
    ULONG count;
    enum xfer_dir direction = WRITE;


    Info("Task: waiting for init\n");
    while (task->tc_UserData == NULL); // Wait for Task Data to be populated
    159a:	2028 0058      	move.l 88(a0),d0
    159e:	67fa           	beq.s 159a 159a _ide_task+0x16
    struct DeviceBase *dev = (struct DeviceBase *)task->tc_UserData;
    15a0:	2f68 0058 0034 	move.l 88(a0),52(sp)
    Trace("Task: CreatePort()\n");
    // Create the MessagePort used to send us requests
    if ((mp = CreatePort(NULL,0)) == NULL) {
    15a6:	42a7           	clr.l -(sp)
    15a8:	42a7           	clr.l -(sp)
    15aa:	4eb9 0000 3328 	jsr 3328 3328 _CreatePort
    15b0:	2a40           	movea.l d0,a5
    15b2:	508f           	addq.l #8,sp
    15b4:	4a80           	tst.l d0
    15b6:	6616           	bne.s 15ce 15ce _ide_task+0x4a
        dev->Task = NULL; // Failed to create MP, let the device know
    15b8:	206f 0034      	movea.l 52(sp),a0
    15bc:	2140 002e      	move.l d0,46(a0)
        RemTask(NULL);
    15c0:	2240           	movea.l d0,a1
    15c2:	2c42           	movea.l d2,a6
    15c4:	4eae fee0      	jsr -288(a6)
        Wait(0);
    15c8:	200d           	move.l a5,d0
    15ca:	4eae fec2      	jsr -318(a6)
    }

    dev->TaskMP = mp;
    15ce:	206f 0034      	movea.l 52(sp),a0
    15d2:	214d 0032      	move.l a5,50(a0)
    dev->TaskActive = true;
    15d6:	117c 0001 0036 	move.b #1,54(a0)
            UWORD *identity = AllocMem(512,MEMF_CLEAR|MEMF_ANY);
    15dc:	2a02           	move.l d2,d5

    while (1) {
        // Main loop, handle IO Requests as they comee in.
        Trace("WaitPort()\n");
        Wait(1 << mp->mp_SigBit); // Wait for an IORequest to show up
    15de:	7200           	moveq #0,d1
    15e0:	122d 000f      	move.b 15(a5),d1
    15e4:	7001           	moveq #1,d0
    15e6:	e3a8           	lsl.l d1,d0
    15e8:	2c45           	movea.l d5,a6
    15ea:	4eae fec2      	jsr -318(a6)

        while ((ioreq = (struct IOStdReq *)GetMsg(mp))) {
    15ee:	204d           	movea.l a5,a0
    15f0:	4eae fe8c      	jsr -372(a6)
    15f4:	2440           	movea.l d0,a2
    15f6:	4a80           	tst.l d0
    15f8:	67e4           	beq.s 15de 15de _ide_task+0x5a
            unit = (struct IDEUnit *)ioreq->io_Unit;
    15fa:	266a 0018      	movea.l 24(a2),a3
            direction = WRITE;

            switch (ioreq->io_Command) {
    15fe:	302a 001c      	move.w 28(a2),d0
    1602:	0c40 0019      	cmpi.w #25,d0
    1606:	6778           	beq.s 1680 1680 _ide_task+0xfc
    1608:	0c40 0019      	cmpi.w #25,d0
    160c:	6200 00f0      	bhi.w 16fe 16fe _ide_task+0x17a
    1610:	0c40 0003      	cmpi.w #3,d0
    1614:	676a           	beq.s 1680 1680 _ide_task+0xfc
    1616:	6300 049e      	bls.w 1ab6 1ab6 _ide_task+0x532
    161a:	0c40 000a      	cmpi.w #10,d0
    161e:	6600 055c      	bne.w 1b7c 1b7c _ide_task+0x5f8
                    break;

                /* CMD_DIE: Shut down this task and clean up */
                case CMD_DIE:
                    Info("CMD_DIE: Shutting down IDE Task\n");
                    DeletePort(mp);
    1622:	2f0d           	move.l a5,-(sp)
    1624:	4eb9 0000 33a2 	jsr 33a2 33a2 _DeletePort
                    dev->TaskMP = NULL;
    162a:	206f 0038      	movea.l 56(sp),a0
    162e:	42a8 0032      	clr.l 50(a0)
                    dev->Task = NULL;
    1632:	42a8 002e      	clr.l 46(a0)
                    dev->TaskActive = false;
    1636:	117c 0000 0036 	move.b #0,54(a0)
                    ReplyMsg(&ioreq->io_Message);
    163c:	224a           	movea.l a2,a1
    163e:	2c45           	movea.l d5,a6
    1640:	4eae fe86      	jsr -378(a6)
                    RemTask(NULL);
    1644:	93c9           	suba.l a1,a1
    1646:	4eae fee0      	jsr -288(a6)
                    Wait(0);
    164a:	7000           	moveq #0,d0
    164c:	4eae fec2      	jsr -318(a6)
    1650:	588f           	addq.l #4,sp

                default:
                    // Unknown commands.
                    ioreq->io_Error = IOERR_NOCMD;
    1652:	157c 00fd 001f 	move.b #-3,31(a2)
                    ioreq->io_Actual = 0;
    1658:	42aa 0020      	clr.l 32(a2)
                    break;
            }

            ReplyMsg(&ioreq->io_Message);
    165c:	224a           	movea.l a2,a1
    165e:	2c45           	movea.l d5,a6
    1660:	4eae fe86      	jsr -378(a6)
        while ((ioreq = (struct IOStdReq *)GetMsg(mp))) {
    1664:	204d           	movea.l a5,a0
    1666:	4eae fe8c      	jsr -372(a6)
    166a:	2440           	movea.l d0,a2
    166c:	4a80           	tst.l d0
    166e:	6700 ff6e      	beq.w 15de 15de _ide_task+0x5a
            unit = (struct IDEUnit *)ioreq->io_Unit;
    1672:	266a 0018      	movea.l 24(a2),a3
            switch (ioreq->io_Command) {
    1676:	302a 001c      	move.w 28(a2),d0
    167a:	0c40 0019      	cmpi.w #25,d0
    167e:	6688           	bne.s 1608 1608 _ide_task+0x84
            direction = WRITE;
    1680:	7601           	moveq #1,d3
                    lba = (((long long)ioreq->io_Actual << 32 | ioreq->io_Offset) >> blockShift);
    1682:	7000           	moveq #0,d0
    1684:	206a 0020      	movea.l 32(a2),a0
    1688:	302b 004c      	move.w 76(a3),d0
    168c:	2808           	move.l a0,d4
    168e:	307c ffe0      	movea.w #-32,a0
    1692:	d1c0           	adda.l d0,a0
    1694:	2c2a 002c      	move.l 44(a2),d6
    1698:	2208           	move.l a0,d1
    169a:	6d00 0440      	blt.w 1adc 1adc _ide_task+0x558
    169e:	2204           	move.l d4,d1
    16a0:	2408           	move.l a0,d2
    16a2:	e4a1           	asr.l d2,d1
    16a4:	2f41 0030      	move.l d1,48(sp)
    16a8:	d884           	add.l d4,d4
    16aa:	9984           	subx.l d4,d4
    16ac:	2f44 002c      	move.l d4,44(sp)
                    count = (ioreq->io_Length >> blockShift);
    16b0:	242a 0024      	move.l 36(a2),d2
    16b4:	e0aa           	lsr.l d0,d2
                    if ((lba + count) > ((unit->cylinders * unit->heads * unit->sectorsPerTrack) -1)) {
    16b6:	2802           	move.l d2,d4
    16b8:	d8af 0030      	add.l 48(sp),d4
    16bc:	7000           	moveq #0,d0
    16be:	302b 0048      	move.w 72(a3),d0
    16c2:	2f00           	move.l d0,-(sp)
    16c4:	302b 0044      	move.w 68(a3),d0
    16c8:	c0eb 0046      	mulu.w 70(a3),d0
    16cc:	2f00           	move.l d0,-(sp)
    16ce:	4eb9 0000 3514 	jsr 3514 3514 ___mulsi3
    16d4:	508f           	addq.l #8,sp
    16d6:	5380           	subq.l #1,d0
    16d8:	b084           	cmp.l d4,d0
    16da:	6400 0448      	bcc.w 1b24 1b24 _ide_task+0x5a0
                        ioreq->io_Error = IOERR_BADADDRESS;
    16de:	157c 00fb 001f 	move.b #-5,31(a2)
            ReplyMsg(&ioreq->io_Message);
    16e4:	224a           	movea.l a2,a1
    16e6:	2c45           	movea.l d5,a6
    16e8:	4eae fe86      	jsr -378(a6)
        while ((ioreq = (struct IOStdReq *)GetMsg(mp))) {
    16ec:	204d           	movea.l a5,a0
    16ee:	4eae fe8c      	jsr -372(a6)
    16f2:	2440           	movea.l d0,a2
    16f4:	4a80           	tst.l d0
    16f6:	6600 ff7a      	bne.w 1672 1672 _ide_task+0xee
    16fa:	6000 fee2      	bra.w 15de 15de _ide_task+0x5a
            switch (ioreq->io_Command) {
    16fe:	0c40 c000      	cmpi.w #-16384,d0
    1702:	6700 03ba      	beq.w 1abe 1abe _ide_task+0x53a
    1706:	6200 0456      	bhi.w 1b5e 1b5e _ide_task+0x5da
    170a:	0c40 001b      	cmpi.w #27,d0
    170e:	6700 ff70      	beq.w 1680 1680 _ide_task+0xfc
    1712:	0c40 001c      	cmpi.w #28,d0
    1716:	6600 ff3a      	bne.w 1652 1652 _ide_task+0xce
    struct SCSICmd *scsi_command = ioreq->io_Data;
    171a:	286a 0028      	movea.l 40(a2),a4
    UBYTE *data    = (APTR)scsi_command->scsi_Data;
    171e:	2414           	move.l (a4),d2
    UBYTE *command = (APTR)scsi_command->scsi_Command;
    1720:	206c 000c      	movea.l 12(a4),a0
    switch (scsi_command->scsi_Command[0]) {
    1724:	0c10 002a      	cmpi.b #42,(a0)
    1728:	6200 0376      	bhi.w 1aa0 1aa0 _ide_task+0x51c
    172c:	7000           	moveq #0,d0
    172e:	1010           	move.b (a0),d0
    1730:	d080           	add.l d0,d0
    1732:	303b 0806      	move.w (173a 173a _ide_task+0x1b6,pc,d0.l),d0
    1736:	4efb 0002      	jmp (173a 173a _ide_task+0x1b6,pc,d0.w)
    173a:	02de           	.short 0x02de
    173c:	0366           	bchg d1,-(a6)
    173e:	0366           	bchg d1,-(a6)
    1740:	0366           	bchg d1,-(a6)
    1742:	0366           	bchg d1,-(a6)
    1744:	0366           	bchg d1,-(a6)
    1746:	0366           	bchg d1,-(a6)
    1748:	0366           	bchg d1,-(a6)
    174a:	00ec           	.short 0x00ec
    174c:	0366           	bchg d1,-(a6)
    174e:	011a           	btst d0,(a2)+
    1750:	0366           	bchg d1,-(a6)
    1752:	0366           	bchg d1,-(a6)
    1754:	0366           	bchg d1,-(a6)
    1756:	0366           	bchg d1,-(a6)
    1758:	0366           	bchg d1,-(a6)
    175a:	0366           	bchg d1,-(a6)
    175c:	0366           	bchg d1,-(a6)
    175e:	022e 0366 0366 	andi.b #102,870(a6)
    1764:	0366           	bchg d1,-(a6)
    1766:	0366           	bchg d1,-(a6)
    1768:	0366           	bchg d1,-(a6)
    176a:	0366           	bchg d1,-(a6)
    176c:	0366           	bchg d1,-(a6)
    176e:	01b2 0366 0366 	bclr d0,([870,a2],870)
    1774:	0366 
    1776:	0366           	bchg d1,-(a6)
    1778:	0366           	bchg d1,-(a6)
    177a:	0366           	bchg d1,-(a6)
    177c:	0366           	bchg d1,-(a6)
    177e:	0366           	bchg d1,-(a6)
    1780:	0366           	bchg d1,-(a6)
    1782:	0366           	bchg d1,-(a6)
    1784:	014a 0366      	movep.l 870(a2),d0
    1788:	0366           	bchg d1,-(a6)
    178a:	0056 0366      	ori.w #870,(a6)
    178e:	02f4           	.short 0x02f4
            direction = READ;
    1790:	9dce           	suba.l a6,a6
            lba    = ((struct SCSI_CDB_10 *)command)->lba;
    1792:	7200           	moveq #0,d1
    1794:	1228 0002      	move.b 2(a0),d1
    1798:	e149           	lsl.w #8,d1
    179a:	4841           	swap d1
    179c:	4241           	clr.w d1
    179e:	7000           	moveq #0,d0
    17a0:	1028 0003      	move.b 3(a0),d0
    17a4:	4840           	swap d0
    17a6:	4240           	clr.w d0
    17a8:	8280           	or.l d0,d1
    17aa:	7000           	moveq #0,d0
    17ac:	1028 0004      	move.b 4(a0),d0
    17b0:	e188           	lsl.l #8,d0
    17b2:	8081           	or.l d1,d0
    17b4:	7800           	moveq #0,d4
    17b6:	1828 0005      	move.b 5(a0),d4
    17ba:	8880           	or.l d0,d4
            count  = ((struct SCSI_CDB_10 *)command)->length;
    17bc:	7600           	moveq #0,d3
    17be:	1628 0007      	move.b 7(a0),d3
    17c2:	e18b           	lsl.l #8,d3
    17c4:	7000           	moveq #0,d0
    17c6:	1028 0008      	move.b 8(a0),d0
    17ca:	8680           	or.l d0,d3
           if (data == NULL || (lba + count) > ((unit->heads * unit->cylinders * unit->sectorsPerTrack) - 1)) {
    17cc:	4a82           	tst.l d2
    17ce:	6726           	beq.s 17f6 17f6 _ide_task+0x272
    17d0:	7000           	moveq #0,d0
    17d2:	302b 0048      	move.w 72(a3),d0
    17d6:	2f00           	move.l d0,-(sp)
    17d8:	322b 0046      	move.w 70(a3),d1
    17dc:	c2eb 0044      	mulu.w 68(a3),d1
    17e0:	2f01           	move.l d1,-(sp)
    17e2:	4eb9 0000 3514 	jsr 3514 3514 ___mulsi3
    17e8:	508f           	addq.l #8,sp
    17ea:	5380           	subq.l #1,d0
    17ec:	2203           	move.l d3,d1
    17ee:	d284           	add.l d4,d1
    17f0:	b081           	cmp.l d1,d0
    17f2:	6400 027c      	bcc.w 1a70 1a70 _ide_task+0x4ec
    ioreq->io_Error = error;
    17f6:	157c 00fb 001f 	move.b #-5,31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    17fc:	396c 0010 0012 	move.w 16(a4),18(a4)
    if (error != 0) scsi_command->scsi_Status = SCSI_CHECK_CONDITION;
    1802:	197c 0002 0015 	move.b #2,21(a4)
    scsi_command->scsi_SenseActual = 0; // TODO: Add Sense data
    1808:	426c 001c      	clr.w 28(a4)
            ReplyMsg(&ioreq->io_Message);
    180c:	224a           	movea.l a2,a1
    180e:	2c45           	movea.l d5,a6
    1810:	4eae fe86      	jsr -378(a6)
        while ((ioreq = (struct IOStdReq *)GetMsg(mp))) {
    1814:	204d           	movea.l a5,a0
    1816:	4eae fe8c      	jsr -372(a6)
    181a:	2440           	movea.l d0,a2
    181c:	4a80           	tst.l d0
    181e:	6600 fe52      	bne.w 1672 1672 _ide_task+0xee
    1822:	6000 fdba      	bra.w 15de 15de _ide_task+0x5a
            direction = READ;
    1826:	9dce           	suba.l a6,a6
            lba   = (((((struct SCSI_CDB_6 *)command)->lba_high & 0x1F) << 16) |
    1828:	7800           	moveq #0,d4
    182a:	1828 0001      	move.b 1(a0),d4
    182e:	4844           	swap d4
    1830:	4244           	clr.w d4
    1832:	0284 001f 0000 	andi.l #2031616,d4
                       ((struct SCSI_CDB_6 *)command)->lba_mid << 8 |
    1838:	7000           	moveq #0,d0
    183a:	1028 0002      	move.b 2(a0),d0
    183e:	2200           	move.l d0,d1
    1840:	e189           	lsl.l #8,d1
    1842:	1028 0003      	move.b 3(a0),d0
    1846:	8081           	or.l d1,d0
    1848:	8880           	or.l d0,d4
            count = ((struct SCSI_CDB_6 *)command)->length;
    184a:	7600           	moveq #0,d3
    184c:	1628 0004      	move.b 4(a0),d3
    1850:	6000 ff7a      	bra.w 17cc 17cc _ide_task+0x248
    enum xfer_dir direction = WRITE;
    1854:	3c7c 0001      	movea.w #1,a6
            lba   = (((((struct SCSI_CDB_6 *)command)->lba_high & 0x1F) << 16) |
    1858:	7800           	moveq #0,d4
    185a:	1828 0001      	move.b 1(a0),d4
    185e:	4844           	swap d4
    1860:	4244           	clr.w d4
    1862:	0284 001f 0000 	andi.l #2031616,d4
                       ((struct SCSI_CDB_6 *)command)->lba_mid << 8 |
    1868:	7000           	moveq #0,d0
    186a:	1028 0002      	move.b 2(a0),d0
    186e:	2200           	move.l d0,d1
    1870:	e189           	lsl.l #8,d1
    1872:	1028 0003      	move.b 3(a0),d0
    1876:	8081           	or.l d1,d0
    1878:	8880           	or.l d0,d4
            count = ((struct SCSI_CDB_6 *)command)->length;
    187a:	7600           	moveq #0,d3
    187c:	1628 0004      	move.b 4(a0),d3
    1880:	6000 ff4a      	bra.w 17cc 17cc _ide_task+0x248
            if (data == NULL) {
    1884:	4a82           	tst.l d2
    1886:	6700 ff6e      	beq.w 17f6 17f6 _ide_task+0x272
            ((struct SCSI_CAPACITY_10 *)data)->lba = ((unit->sectorsPerTrack * unit->cylinders * unit->heads) - 1);
    188a:	7000           	moveq #0,d0
    188c:	302b 0046      	move.w 70(a3),d0
    1890:	2f00           	move.l d0,-(sp)
    1892:	302b 0048      	move.w 72(a3),d0
    1896:	c0eb 0044      	mulu.w 68(a3),d0
    189a:	2f00           	move.l d0,-(sp)
    189c:	4eb9 0000 3514 	jsr 3514 3514 ___mulsi3
    18a2:	5380           	subq.l #1,d0
    18a4:	2200           	move.l d0,d1
    18a6:	508f           	addq.l #8,sp
    18a8:	2042           	movea.l d2,a0
    18aa:	4241           	clr.w d1
    18ac:	4841           	swap d1
    18ae:	e049           	lsr.w #8,d1
    18b0:	10c1           	move.b d1,(a0)+
    18b2:	2200           	move.l d0,d1
    18b4:	4241           	clr.w d1
    18b6:	4841           	swap d1
    18b8:	10c1           	move.b d1,(a0)+
    18ba:	2200           	move.l d0,d1
    18bc:	e089           	lsr.l #8,d1
    18be:	10c1           	move.b d1,(a0)+
    18c0:	10c0           	move.b d0,(a0)+
            ((struct SCSI_CAPACITY_10 *)data)->block_size = unit->blockSize;
    18c2:	7000           	moveq #0,d0
    18c4:	302b 004a      	move.w 74(a3),d0
    18c8:	4218           	clr.b (a0)+
    18ca:	4218           	clr.b (a0)+
    18cc:	2200           	move.l d0,d1
    18ce:	e089           	lsr.l #8,d1
    18d0:	10c1           	move.b d1,(a0)+
    18d2:	1080           	move.b d0,(a0)
            scsi_command->scsi_Actual = 8;
    18d4:	7008           	moveq #8,d0
    18d6:	2940 0008      	move.l d0,8(a4)
    ioreq->io_Error = error;
    18da:	422a 001f      	clr.b 31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    18de:	396c 0010 0012 	move.w 16(a4),18(a4)
    scsi_command->scsi_SenseActual = 0; // TODO: Add Sense data
    18e4:	426c 001c      	clr.w 28(a4)
    18e8:	6000 ff22      	bra.w 180c 180c _ide_task+0x288
            if (data == NULL) {
    18ec:	4a82           	tst.l d2
    18ee:	6700 ff06      	beq.w 17f6 17f6 _ide_task+0x272
            if (subpage != 0) {
    18f2:	4a28 0003      	tst.b 3(a0)
    18f6:	6600 01a8      	bne.w 1aa0 1aa0 _ide_task+0x51c
            UBYTE page    = command[2] & 0x3F;
    18fa:	1628 0002      	move.b 2(a0),d3
            data[1] = unit->device_type; // Mode parameter: Media type
    18fe:	2042           	movea.l d2,a0
            UBYTE page    = command[2] & 0x3F;
    1900:	0203 003f      	andi.b #63,d3
            data[1] = unit->device_type; // Mode parameter: Media type
    1904:	116b 0038 0001 	move.b 56(a3),1(a0)
            data[2] = 0;                 // DPOFUA
    190a:	4228 0002      	clr.b 2(a0)
            data[3] = 0;                 // Block descriptor length
    190e:	4228 0003      	clr.b 3(a0)
            *data_length = 3;
    1912:	10bc 0003      	move.b #3,(a0)
            if (page == 0x3F || page == 0x03) {
    1916:	0c03 003f      	cmpi.b #63,d3
    191a:	6700 026e      	beq.w 1b8a 1b8a _ide_task+0x606
    191e:	0c03 0003      	cmpi.b #3,d3
    1922:	6700 0266      	beq.w 1b8a 1b8a _ide_task+0x606
    1926:	307c 0009      	movea.w #9,a0
    192a:	780a           	moveq #10,d4
    192c:	7208           	moveq #8,d1
    192e:	7006           	moveq #6,d0
    1930:	327c 0007      	movea.w #7,a1
    1934:	2f40 0038      	move.l d0,56(sp)
    1938:	7005           	moveq #5,d0
    193a:	2f40 003c      	move.l d0,60(sp)
    193e:	3c7c 0004      	movea.w #4,a6
    1942:	1001           	move.b d1,d0
            if (page == 0x3F || page == 0x04) {
    1944:	0c03 0004      	cmpi.b #4,d3
    1948:	6700 030e      	beq.w 1c58 1c58 _ide_task+0x6d4
    194c:	2208           	move.l a0,d1
            *data_length += (idx + 1);
    194e:	2042           	movea.l d2,a0
    1950:	1080           	move.b d0,(a0)
            scsi_command->scsi_Actual = *data_length + 1;
    1952:	2941 0008      	move.l d1,8(a4)
    ioreq->io_Error = error;
    1956:	422a 001f      	clr.b 31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    195a:	396c 0010 0012 	move.w 16(a4),18(a4)
    scsi_command->scsi_SenseActual = 0; // TODO: Add Sense data
    1960:	426c 001c      	clr.w 28(a4)
    1964:	6000 fea6      	bra.w 180c 180c _ide_task+0x288
            ((struct SCSI_Inquiry *)data)->peripheral_type = unit->device_type;
    1968:	2042           	movea.l d2,a0
    196a:	10eb 0038      	move.b 56(a3),(a0)+
    196e:	4218           	clr.b (a0)+
    1970:	10fc 0002      	move.b #2,(a0)+
    1974:	10fc 0002      	move.b #2,(a0)+
    1978:	10bc 0028      	move.b #40,(a0)
            UWORD *identity = AllocMem(512,MEMF_CLEAR|MEMF_ANY);
    197c:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    1982:	203c 0000 0200 	move.l #512,d0
    1988:	7201           	moveq #1,d1
    198a:	4841           	swap d1
    198c:	4eae ff3a      	jsr -198(a6)
    1990:	2600           	move.l d0,d3
            if (!identity) {
    1992:	6700 010c      	beq.w 1aa0 1aa0 _ide_task+0x51c
            if (!(ata_identify(unit,identity))) {
    1996:	2f00           	move.l d0,-(sp)
    1998:	2f0b           	move.l a3,-(sp)
    199a:	4eb9 0000 0d5e 	jsr d5e d5e _ata_identify
    19a0:	508f           	addq.l #8,sp
    19a2:	4a00           	tst.b d0
    19a4:	6700 00fa      	beq.w 1aa0 1aa0 _ide_task+0x51c
            CopyMem(&identity[ata_identify_model],&((struct SCSI_Inquiry *)data)->vendor,24);
    19a8:	307c 0036      	movea.w #54,a0
    19ac:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    19b2:	2242           	movea.l d2,a1
    19b4:	d1c3           	adda.l d3,a0
    19b6:	7018           	moveq #24,d0
    19b8:	5089           	addq.l #8,a1
    19ba:	4eae fd90      	jsr -624(a6)
            CopyMem(&identity[ata_identify_fw_rev],&((struct SCSI_Inquiry *)data)->revision,4);
    19be:	307c 002e      	movea.w #46,a0
    19c2:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    19c8:	327c 0020      	movea.w #32,a1
    19cc:	d1c3           	adda.l d3,a0
    19ce:	7004           	moveq #4,d0
    19d0:	d3c2           	adda.l d2,a1
    19d2:	4eae fd90      	jsr -624(a6)
            CopyMem(&identity[ata_identify_serial],&((struct SCSI_Inquiry *)data)->serial,8);
    19d6:	307c 0014      	movea.w #20,a0
    19da:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    19e0:	327c 0024      	movea.w #36,a1
    19e4:	d1c3           	adda.l d3,a0
    19e6:	7008           	moveq #8,d0
    19e8:	d3c2           	adda.l d2,a1
    19ea:	4eae fd90      	jsr -624(a6)
            FreeMem(identity,512);
    19ee:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    19f4:	203c 0000 0200 	move.l #512,d0
    19fa:	2243           	movea.l d3,a1
    19fc:	4eae ff2e      	jsr -210(a6)
            scsi_command->scsi_Actual = scsi_command->scsi_Length;
    1a00:	296c 0004 0008 	move.l 4(a4),8(a4)
    ioreq->io_Error = error;
    1a06:	422a 001f      	clr.b 31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    1a0a:	396c 0010 0012 	move.w 16(a4),18(a4)
    scsi_command->scsi_SenseActual = 0; // TODO: Add Sense data
    1a10:	426c 001c      	clr.w 28(a4)
    1a14:	6000 fdf6      	bra.w 180c 180c _ide_task+0x288
            scsi_command->scsi_Actual = 0;
    1a18:	42ac 0008      	clr.l 8(a4)
    ioreq->io_Error = error;
    1a1c:	422a 001f      	clr.b 31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    1a20:	396c 0010 0012 	move.w 16(a4),18(a4)
    scsi_command->scsi_SenseActual = 0; // TODO: Add Sense data
    1a26:	426c 001c      	clr.w 28(a4)
    1a2a:	6000 fde0      	bra.w 180c 180c _ide_task+0x288
    enum xfer_dir direction = WRITE;
    1a2e:	3c7c 0001      	movea.w #1,a6
            lba    = ((struct SCSI_CDB_10 *)command)->lba;
    1a32:	7200           	moveq #0,d1
    1a34:	1228 0002      	move.b 2(a0),d1
    1a38:	e149           	lsl.w #8,d1
    1a3a:	4841           	swap d1
    1a3c:	4241           	clr.w d1
    1a3e:	7000           	moveq #0,d0
    1a40:	1028 0003      	move.b 3(a0),d0
    1a44:	4840           	swap d0
    1a46:	4240           	clr.w d0
    1a48:	8280           	or.l d0,d1
    1a4a:	7000           	moveq #0,d0
    1a4c:	1028 0004      	move.b 4(a0),d0
    1a50:	e188           	lsl.l #8,d0
    1a52:	8081           	or.l d1,d0
    1a54:	7800           	moveq #0,d4
    1a56:	1828 0005      	move.b 5(a0),d4
    1a5a:	8880           	or.l d0,d4
            count  = ((struct SCSI_CDB_10 *)command)->length;
    1a5c:	7600           	moveq #0,d3
    1a5e:	1628 0007      	move.b 7(a0),d3
    1a62:	e18b           	lsl.l #8,d3
    1a64:	7000           	moveq #0,d0
    1a66:	1028 0008      	move.b 8(a0),d0
    1a6a:	8680           	or.l d0,d3
    1a6c:	6000 fd5e      	bra.w 17cc 17cc _ide_task+0x248
            error  = ata_transfer(data,lba,count,&scsi_command->scsi_Actual,unit,direction);
    1a70:	2f0e           	move.l a6,-(sp)
    1a72:	2f0b           	move.l a3,-(sp)
    1a74:	486c 0008      	pea 8(a4)
    1a78:	2f03           	move.l d3,-(sp)
    1a7a:	2f04           	move.l d4,-(sp)
    1a7c:	2f02           	move.l d2,-(sp)
    1a7e:	4eb9 0000 108c 	jsr 108c 108c _ata_transfer
    ioreq->io_Error = error;
    1a84:	1540 001f      	move.b d0,31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    1a88:	396c 0010 0012 	move.w 16(a4),18(a4)
    if (error != 0) scsi_command->scsi_Status = SCSI_CHECK_CONDITION;
    1a8e:	4fef 0018      	lea 24(sp),sp
    1a92:	4a00           	tst.b d0
    1a94:	6600 fd6c      	bne.w 1802 1802 _ide_task+0x27e
    scsi_command->scsi_SenseActual = 0; // TODO: Add Sense data
    1a98:	426c 001c      	clr.w 28(a4)
    1a9c:	6000 fd6e      	bra.w 180c 180c _ide_task+0x288
    ioreq->io_Error = error;
    1aa0:	157c 002d 001f 	move.b #45,31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    1aa6:	396c 0010 0012 	move.w 16(a4),18(a4)
    if (error != 0) scsi_command->scsi_Status = SCSI_CHECK_CONDITION;
    1aac:	197c 0002 0015 	move.b #2,21(a4)
    1ab2:	6000 fd54      	bra.w 1808 1808 _ide_task+0x284
            switch (ioreq->io_Command) {
    1ab6:	0c40 0002      	cmpi.w #2,d0
    1aba:	6600 fb96      	bne.w 1652 1652 _ide_task+0xce
                    direction = READ;
    1abe:	7600           	moveq #0,d3
                    lba = (((long long)ioreq->io_Actual << 32 | ioreq->io_Offset) >> blockShift);
    1ac0:	7000           	moveq #0,d0
    1ac2:	206a 0020      	movea.l 32(a2),a0
    1ac6:	302b 004c      	move.w 76(a3),d0
    1aca:	2808           	move.l a0,d4
    1acc:	307c ffe0      	movea.w #-32,a0
    1ad0:	d1c0           	adda.l d0,a0
    1ad2:	2c2a 002c      	move.l 44(a2),d6
    1ad6:	2208           	move.l a0,d1
    1ad8:	6c00 fbc4      	bge.w 169e 169e _ide_task+0x11a
    1adc:	741f           	moveq #31,d2
    1ade:	9480           	sub.l d0,d2
    1ae0:	2204           	move.l d4,d1
    1ae2:	d281           	add.l d1,d1
    1ae4:	e5a9           	lsl.l d2,d1
    1ae6:	e0ae           	lsr.l d0,d6
    1ae8:	2f46 0030      	move.l d6,48(sp)
    1aec:	83af 0030      	or.l d1,48(sp)
    1af0:	e0a4           	asr.l d0,d4
    1af2:	2f44 002c      	move.l d4,44(sp)
                    count = (ioreq->io_Length >> blockShift);
    1af6:	242a 0024      	move.l 36(a2),d2
    1afa:	e0aa           	lsr.l d0,d2
                    if ((lba + count) > ((unit->cylinders * unit->heads * unit->sectorsPerTrack) -1)) {
    1afc:	2802           	move.l d2,d4
    1afe:	d8af 0030      	add.l 48(sp),d4
    1b02:	7000           	moveq #0,d0
    1b04:	302b 0048      	move.w 72(a3),d0
    1b08:	2f00           	move.l d0,-(sp)
    1b0a:	302b 0044      	move.w 68(a3),d0
    1b0e:	c0eb 0046      	mulu.w 70(a3),d0
    1b12:	2f00           	move.l d0,-(sp)
    1b14:	4eb9 0000 3514 	jsr 3514 3514 ___mulsi3
    1b1a:	508f           	addq.l #8,sp
    1b1c:	5380           	subq.l #1,d0
    1b1e:	b084           	cmp.l d4,d0
    1b20:	6500 fbbc      	bcs.w 16de 16de _ide_task+0x15a
                    ioreq->io_Error = ata_transfer(ioreq->io_Data, lba, count, &ioreq->io_Actual, unit, direction);
    1b24:	2f03           	move.l d3,-(sp)
    1b26:	2f0b           	move.l a3,-(sp)
    1b28:	486a 0020      	pea 32(a2)
    1b2c:	2f02           	move.l d2,-(sp)
    1b2e:	2f2f 0040      	move.l 64(sp),-(sp)
    1b32:	2f2a 0028      	move.l 40(a2),-(sp)
    1b36:	4eb9 0000 108c 	jsr 108c 108c _ata_transfer
    1b3c:	1540 001f      	move.b d0,31(a2)
                    break;
    1b40:	4fef 0018      	lea 24(sp),sp
            ReplyMsg(&ioreq->io_Message);
    1b44:	224a           	movea.l a2,a1
    1b46:	2c45           	movea.l d5,a6
    1b48:	4eae fe86      	jsr -378(a6)
        while ((ioreq = (struct IOStdReq *)GetMsg(mp))) {
    1b4c:	204d           	movea.l a5,a0
    1b4e:	4eae fe8c      	jsr -372(a6)
    1b52:	2440           	movea.l d0,a2
    1b54:	4a80           	tst.l d0
    1b56:	6600 fb1a      	bne.w 1672 1672 _ide_task+0xee
    1b5a:	6000 fa82      	bra.w 15de 15de _ide_task+0x5a
            switch (ioreq->io_Command) {
    1b5e:	0c40 c001      	cmpi.w #-16383,d0
    1b62:	6700 fb1c      	beq.w 1680 1680 _ide_task+0xfc
    1b66:	0c40 c003      	cmpi.w #-16381,d0
    1b6a:	6700 fb14      	beq.w 1680 1680 _ide_task+0xfc
                    ioreq->io_Error = IOERR_NOCMD;
    1b6e:	157c 00fd 001f 	move.b #-3,31(a2)
                    ioreq->io_Actual = 0;
    1b74:	42aa 0020      	clr.l 32(a2)
    1b78:	6000 fae2      	bra.w 165c 165c _ide_task+0xd8
            switch (ioreq->io_Command) {
    1b7c:	0c40 0018      	cmpi.w #24,d0
    1b80:	6600 fad0      	bne.w 1652 1652 _ide_task+0xce
                    direction = READ;
    1b84:	7600           	moveq #0,d3
    1b86:	6000 ff38      	bra.w 1ac0 1ac0 _ide_task+0x53c
                data[idx++] = 0x03; // Page Code: Format Parameters
    1b8a:	2042           	movea.l d2,a0
    1b8c:	5888           	addq.l #4,a0
    1b8e:	10fc 0003      	move.b #3,(a0)+
                data[idx++] = 0x16; // Page length
    1b92:	10fc 0016      	move.b #22,(a0)+
                    data[idx++] = 0;
    1b96:	4218           	clr.b (a0)+
    1b98:	4218           	clr.b (a0)+
    1b9a:	4218           	clr.b (a0)+
    1b9c:	4218           	clr.b (a0)+
    1b9e:	4218           	clr.b (a0)+
    1ba0:	4218           	clr.b (a0)+
    1ba2:	4218           	clr.b (a0)+
    1ba4:	4218           	clr.b (a0)+
                data[idx++] = (unit->sectorsPerTrack >> 8);
    1ba6:	10eb 0048      	move.b 72(a3),(a0)+
                data[idx++] = unit->sectorsPerTrack;
    1baa:	10eb 0049      	move.b 73(a3),(a0)+
                data[idx++] = (unit->blockSize >> 8);
    1bae:	10eb 004a      	move.b 74(a3),(a0)+
                data[idx++] = unit->blockSize;
    1bb2:	10eb 004b      	move.b 75(a3),(a0)+
                    data[idx++] = 0;
    1bb6:	4218           	clr.b (a0)+
    1bb8:	4218           	clr.b (a0)+
    1bba:	4218           	clr.b (a0)+
    1bbc:	4218           	clr.b (a0)+
    1bbe:	4218           	clr.b (a0)+
    1bc0:	4218           	clr.b (a0)+
    1bc2:	4218           	clr.b (a0)+
    1bc4:	4218           	clr.b (a0)+
    1bc6:	4218           	clr.b (a0)+
    1bc8:	4218           	clr.b (a0)+
    1bca:	4218           	clr.b (a0)+
    1bcc:	4210           	clr.b (a0)
            if (page == 0x3F || page == 0x04) {
    1bce:	307c 0023      	movea.w #35,a0
    1bd2:	7824           	moveq #36,d4
    1bd4:	7222           	moveq #34,d1
    1bd6:	7020           	moveq #32,d0
    1bd8:	327c 0021      	movea.w #33,a1
    1bdc:	2f40 0038      	move.l d0,56(sp)
    1be0:	701f           	moveq #31,d0
    1be2:	2f40 003c      	move.l d0,60(sp)
    1be6:	3c7c 001e      	movea.w #30,a6
    1bea:	0c03 003f      	cmpi.b #63,d3
    1bee:	6600 fd52      	bne.w 1942 1942 _ide_task+0x3be
                data[idx++] = 0x04; // Page code: Rigid Drive Geometry Parameters
    1bf2:	1dbc 0004 2800 	move.b #4,(0,a6,d2.l)
                data[idx++] = 0x16; // Page length
    1bf8:	2c42           	movea.l d2,a6
    1bfa:	1dbc 0016 0800 	move.b #22,(0,a6,d0.l)
                data[idx++] = 0;
    1c00:	202f 0038      	move.l 56(sp),d0
    1c04:	4236 0800      	clr.b (0,a6,d0.l)
                data[idx++] = (unit->cylinders >> 8);
    1c08:	13ab 0044 2800 	move.b 68(a3),(0,a1,d2.l)
                data[idx++] = unit->cylinders;
    1c0e:	1dab 0045 1800 	move.b 69(a3),(0,a6,d1.l)
                data[idx++] = unit->heads;
    1c14:	11ab 0047 2800 	move.b 71(a3),(0,a0,d2.l)
    1c1a:	1004           	move.b d4,d0
    1c1c:	7612           	moveq #18,d3
    1c1e:	2042           	movea.l d2,a0
                    data[idx++] = 0;
    1c20:	7200           	moveq #0,d1
    1c22:	1200           	move.b d0,d1
    1c24:	4230 1800      	clr.b (0,a0,d1.l)
    1c28:	5200           	addq.b #1,d0
                for (int i=0; i<19; i++) {
    1c2a:	51cb fff4      	dbf d3,1c20 1c20 _ide_task+0x69c
    1c2e:	4243           	clr.w d3
    1c30:	5383           	subq.l #1,d3
    1c32:	64ec           	bcc.s 1c20 1c20 _ide_task+0x69c
    1c34:	2408           	move.l a0,d2
    1c36:	1010           	move.b (a0),d0
    1c38:	0600 0014      	addi.b #20,d0
    1c3c:	d004           	add.b d4,d0
    1c3e:	1200           	move.b d0,d1
            *data_length += (idx + 1);
    1c40:	2042           	movea.l d2,a0
    1c42:	5281           	addq.l #1,d1
    1c44:	1080           	move.b d0,(a0)
            scsi_command->scsi_Actual = *data_length + 1;
    1c46:	2941 0008      	move.l d1,8(a4)
    ioreq->io_Error = error;
    1c4a:	422a 001f      	clr.b 31(a2)
    scsi_command->scsi_CmdActual = scsi_command->scsi_CmdLength;
    1c4e:	396c 0010 0012 	move.w 16(a4),18(a4)
    1c54:	6000 fd0a      	bra.w 1960 1960 _ide_task+0x3dc
    1c58:	202f 003c      	move.l 60(sp),d0
                data[idx++] = 0x04; // Page code: Rigid Drive Geometry Parameters
    1c5c:	1dbc 0004 2800 	move.b #4,(0,a6,d2.l)
                data[idx++] = 0x16; // Page length
    1c62:	2c42           	movea.l d2,a6
    1c64:	1dbc 0016 0800 	move.b #22,(0,a6,d0.l)
                data[idx++] = 0;
    1c6a:	202f 0038      	move.l 56(sp),d0
    1c6e:	4236 0800      	clr.b (0,a6,d0.l)
                data[idx++] = (unit->cylinders >> 8);
    1c72:	13ab 0044 2800 	move.b 68(a3),(0,a1,d2.l)
                data[idx++] = unit->cylinders;
    1c78:	1dab 0045 1800 	move.b 69(a3),(0,a6,d1.l)
                data[idx++] = unit->heads;
    1c7e:	11ab 0047 2800 	move.b 71(a3),(0,a0,d2.l)
    1c84:	1004           	move.b d4,d0
    1c86:	7612           	moveq #18,d3
    1c88:	2042           	movea.l d2,a0
    1c8a:	6094           	bra.s 1c20 1c20 _ide_task+0x69c

00001c8c 00001c8c _W_CreateIORequest:
	int blocksize;
};

// KS 1.3 compatibility functions
APTR W_CreateIORequest(struct MsgPort *ioReplyPort, ULONG size, struct ExecBase *SysBase)
{
    1c8c:	48e7 3002      	movem.l d2-d3/a6,-(sp)
    1c90:	242f 0010      	move.l 16(sp),d2
    1c94:	262f 0014      	move.l 20(sp),d3
	struct IORequest *ret = NULL;
	if(ioReplyPort == NULL)
    1c98:	4a82           	tst.l d2
    1c9a:	6724           	beq.s 1cc0 1cc0 _W_CreateIORequest+0x34
		return NULL;
	ret = (struct IORequest*)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
    1c9c:	2c6f 0018      	movea.l 24(sp),a6
    1ca0:	2003           	move.l d3,d0
    1ca2:	223c 0001 0001 	move.l #65537,d1
    1ca8:	4eae ff3a      	jsr -198(a6)
	if(ret != NULL)
    1cac:	4a80           	tst.l d0
    1cae:	670a           	beq.s 1cba 1cba _W_CreateIORequest+0x2e
	{
		ret->io_Message.mn_ReplyPort = ioReplyPort;
    1cb0:	2040           	movea.l d0,a0
    1cb2:	2142 000e      	move.l d2,14(a0)
		ret->io_Message.mn_Length = size;
    1cb6:	3143 0012      	move.w d3,18(a0)
	}
	return ret;
}
    1cba:	4cdf 400c      	movem.l (sp)+,d2-d3/a6
    1cbe:	4e75           	rts
		return NULL;
    1cc0:	2002           	move.l d2,d0
}
    1cc2:	4cdf 400c      	movem.l (sp)+,d2-d3/a6
    1cc6:	4e75           	rts

00001cc8 00001cc8 _W_DeleteIORequest:
void W_DeleteIORequest(APTR iorequest, struct ExecBase *SysBase)
{
    1cc8:	2f0e           	move.l a6,-(sp)
    1cca:	206f 0008      	movea.l 8(sp),a0
	if(iorequest != NULL) {
    1cce:	2008           	move.l a0,d0
    1cd0:	6710           	beq.s 1ce2 1ce2 _W_DeleteIORequest+0x1a
		FreeMem(iorequest, ((struct Message*)iorequest)->mn_Length);
    1cd2:	2c6f 000c      	movea.l 12(sp),a6
    1cd6:	2248           	movea.l a0,a1
    1cd8:	7000           	moveq #0,d0
    1cda:	3028 0012      	move.w 18(a0),d0
    1cde:	4eae ff2e      	jsr -210(a6)
	}
}
    1ce2:	2c5f           	movea.l (sp)+,a6
    1ce4:	4e75           	rts

00001ce6 00001ce6 _W_CreateMsgPort:
struct MsgPort *W_CreateMsgPort(struct ExecBase *SysBase)
{
    1ce6:	48e7 2022      	movem.l d2/a2/a6,-(sp)
	struct MsgPort *ret;
	ret = (struct MsgPort*)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    1cea:	2c6f 0010      	movea.l 16(sp),a6
    1cee:	7022           	moveq #34,d0
    1cf0:	223c 0001 0001 	move.l #65537,d1
    1cf6:	4eae ff3a      	jsr -198(a6)
    1cfa:	2440           	movea.l d0,a2
	if(ret != NULL)
    1cfc:	4a80           	tst.l d0
    1cfe:	6734           	beq.s 1d34 1d34 _W_CreateMsgPort+0x4e
	{
		BYTE sb = AllocSignal(-1);
    1d00:	50c0           	st d0
    1d02:	4eae feb6      	jsr -330(a6)
    1d06:	1400           	move.b d0,d2
		if (sb != -1)
    1d08:	0c00 00ff      	cmpi.b #-1,d0
    1d0c:	672c           	beq.s 1d3a 1d3a _W_CreateMsgPort+0x54
		{
			ret->mp_Flags = PA_SIGNAL;
    1d0e:	422a 000e      	clr.b 14(a2)
			ret->mp_Node.ln_Type = NT_MSGPORT;
    1d12:	157c 0004 0008 	move.b #4,8(a2)
  			NewList(&ret->mp_MsgList);
    1d18:	486a 0014      	pea 20(a2)
    1d1c:	4eb9 0000 3314 	jsr 3314 3314 _NewList
			ret->mp_SigBit = sb;
    1d22:	1542 000f      	move.b d2,15(a2)
			ret->mp_SigTask = FindTask(NULL);
    1d26:	93c9           	suba.l a1,a1
    1d28:	4eae feda      	jsr -294(a6)
    1d2c:	2540 0010      	move.l d0,16(a2)
			return ret;
    1d30:	588f           	addq.l #4,sp
    1d32:	200a           	move.l a2,d0
		}
		FreeMem(ret, sizeof(struct MsgPort));
	}
	return NULL;
}
    1d34:	4cdf 4404      	movem.l (sp)+,d2/a2/a6
    1d38:	4e75           	rts
		FreeMem(ret, sizeof(struct MsgPort));
    1d3a:	7022           	moveq #34,d0
    1d3c:	224a           	movea.l a2,a1
    1d3e:	4eae ff2e      	jsr -210(a6)
	return NULL;
    1d42:	7000           	moveq #0,d0
}
    1d44:	4cdf 4404      	movem.l (sp)+,d2/a2/a6
    1d48:	4e75           	rts

00001d4a 00001d4a _W_DeleteMsgPort:
void W_DeleteMsgPort(struct MsgPort *port, struct ExecBase *SysBase)
{
    1d4a:	2f0e           	move.l a6,-(sp)
    1d4c:	2f0a           	move.l a2,-(sp)
    1d4e:	246f 000c      	movea.l 12(sp),a2
	if(port != NULL)
    1d52:	200a           	move.l a2,d0
    1d54:	6714           	beq.s 1d6a 1d6a _W_DeleteMsgPort+0x20
	{
		FreeSignal(port->mp_SigBit);
    1d56:	102a 000f      	move.b 15(a2),d0
    1d5a:	2c6f 0010      	movea.l 16(sp),a6
    1d5e:	4eae feb0      	jsr -336(a6)
		FreeMem(port, sizeof(struct MsgPort));
    1d62:	7022           	moveq #34,d0
    1d64:	224a           	movea.l a2,a1
    1d66:	4eae ff2e      	jsr -210(a6)
	}
}
    1d6a:	245f           	movea.l (sp)+,a2
    1d6c:	2c5f           	movea.l (sp)+,a6
    1d6e:	4e75           	rts

#define MAX_RETRIES 3

// Read single block with retries
static BOOL readblock(UBYTE *buf, ULONG block, ULONG id, struct MountData *md)
{
    1d70:	48e7 203a      	movem.l d2/a2-a4/a6,-(sp)
    1d74:	41ef 0018      	lea 24(sp),a0
    1d78:	2458           	movea.l (a0)+,a2
    1d7a:	2018           	move.l (a0)+,d0
    1d7c:	2418           	move.l (a0)+,d2
    1d7e:	2850           	movea.l (a0),a4
	struct ExecBase *SysBase = md->SysBase;
    1d80:	2c54           	movea.l (a4),a6
	struct IOExtTD *request = md->request;
    1d82:	266c 000c      	movea.l 12(a4),a3
	UWORD i;

	request->iotd_Req.io_Command = CMD_READ;
    1d86:	377c 0002 001c 	move.w #2,28(a3)
	request->iotd_Req.io_Offset = block << 9;
    1d8c:	e188           	lsl.l #8,d0
    1d8e:	e388           	lsl.l #1,d0
    1d90:	2740 002c      	move.l d0,44(a3)
	request->iotd_Req.io_Data = buf;
    1d94:	274a 0028      	move.l a2,40(a3)
	request->iotd_Req.io_Length = md->blocksize;
    1d98:	276c 183c 0024 	move.l 6204(a4),36(a3)
	for (i = 0; i < MAX_RETRIES; i++) {
		LONG err = DoIO((struct IORequest*)request);
    1d9e:	224b           	movea.l a3,a1
    1da0:	4eae fe38      	jsr -456(a6)
		if (!err) {
    1da4:	4a00           	tst.b d0
    1da6:	671c           	beq.s 1dc4 1dc4 _W_DeleteMsgPort+0x7a
		LONG err = DoIO((struct IORequest*)request);
    1da8:	224b           	movea.l a3,a1
    1daa:	4eae fe38      	jsr -456(a6)
		if (!err) {
    1dae:	4a00           	tst.b d0
    1db0:	6712           	beq.s 1dc4 1dc4 _W_DeleteMsgPort+0x7a
		LONG err = DoIO((struct IORequest*)request);
    1db2:	224b           	movea.l a3,a1
    1db4:	4eae fe38      	jsr -456(a6)
		if (!err) {
    1db8:	4a00           	tst.b d0
    1dba:	6708           	beq.s 1dc4 1dc4 _W_DeleteMsgPort+0x7a
			break;
		}
		dbg("Read block %"PRIu32" error %"PRId32"\n", block, err);
	}
	if (i == MAX_RETRIES) {
		return FALSE;
    1dbc:	4240           	clr.w d0
	}
	if (!checksum(buf, md)) {
		return FALSE;
	}
	return TRUE;
}
    1dbe:	4cdf 5c04      	movem.l (sp)+,d2/a2-a4/a6
    1dc2:	4e75           	rts
    1dc4:	7200           	moveq #0,d1
    1dc6:	1212           	move.b (a2),d1
    1dc8:	2001           	move.l d1,d0
    1dca:	e148           	lsl.w #8,d0
    1dcc:	4840           	swap d0
    1dce:	4240           	clr.w d0
    1dd0:	122a 0001      	move.b 1(a2),d1
    1dd4:	4841           	swap d1
    1dd6:	4241           	clr.w d1
    1dd8:	8280           	or.l d0,d1
    1dda:	7000           	moveq #0,d0
    1ddc:	102a 0002      	move.b 2(a2),d0
    1de0:	e188           	lsl.l #8,d0
    1de2:	8081           	or.l d1,d0
    1de4:	7200           	moveq #0,d1
    1de6:	122a 0003      	move.b 3(a2),d1
    1dea:	8081           	or.l d1,d0
	if (id != 0xffffffff) {
    1dec:	72ff           	moveq #-1,d1
    1dee:	b282           	cmp.l d2,d1
    1df0:	6704           	beq.s 1df6 1df6 _W_DeleteMsgPort+0xac
		if (v != id) {
    1df2:	b082           	cmp.l d2,d0
    1df4:	66c6           	bne.s 1dbc 1dbc _W_DeleteMsgPort+0x72
    1df6:	266c 183c      	movea.l 6204(a4),a3
	for (UWORD i = 0; i < md->blocksize; i += 4) {
    1dfa:	220b           	move.l a3,d1
    1dfc:	6f48           	ble.s 1e46 1e46 _W_DeleteMsgPort+0xfc
    1dfe:	93c9           	suba.l a1,a1
    1e00:	3049           	movea.w a1,a0
    1e02:	2209           	move.l a1,d1
		ULONG v = (buf[i + 0] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | (buf[i + 3 ] << 0);
    1e04:	7000           	moveq #0,d0
    1e06:	1032 1800      	move.b (0,a2,d1.l),d0
    1e0a:	e148           	lsl.w #8,d0
    1e0c:	4840           	swap d0
    1e0e:	4240           	clr.w d0
    1e10:	7400           	moveq #0,d2
    1e12:	1432 1803      	move.b (3,a2,d1.l),d2
    1e16:	8082           	or.l d2,d0
    1e18:	1432 1801      	move.b (1,a2,d1.l),d2
    1e1c:	4842           	swap d2
    1e1e:	4242           	clr.w d2
    1e20:	8082           	or.l d2,d0
    1e22:	7400           	moveq #0,d2
    1e24:	1432 1802      	move.b (2,a2,d1.l),d2
    1e28:	e18a           	lsl.l #8,d2
    1e2a:	8082           	or.l d2,d0
		chk += v;
    1e2c:	d3c0           	adda.l d0,a1
	for (UWORD i = 0; i < md->blocksize; i += 4) {
    1e2e:	5848           	addq.w #4,a0
    1e30:	7200           	moveq #0,d1
    1e32:	3208           	move.w a0,d1
    1e34:	b28b           	cmp.l a3,d1
    1e36:	6dcc           	blt.s 1e04 1e04 _W_DeleteMsgPort+0xba
    1e38:	2209           	move.l a1,d1
    1e3a:	57c0           	seq d0
    1e3c:	4880           	ext.w d0
    1e3e:	4440           	neg.w d0
}
    1e40:	4cdf 5c04      	movem.l (sp)+,d2/a2-a4/a6
    1e44:	4e75           	rts
	for (UWORD i = 0; i < md->blocksize; i += 4) {
    1e46:	7001           	moveq #1,d0
}
    1e48:	4cdf 5c04      	movem.l (sp)+,d2/a2-a4/a6
    1e4c:	4e75           	rts

// Read multiple longs from LSEG blocks
static BOOL lseg_read_longs(struct MountData *md, ULONG longs, ULONG *data)
{
    1e4e:	48e7 3c3e      	movem.l d2-d5/a2-a6,-(sp)
    1e52:	266f 0028      	movea.l 40(sp),a3
    1e56:	262f 002c      	move.l 44(sp),d3
    1e5a:	286f 0030      	movea.l 48(sp),a4
	dbg("lseg_read_longs, longs %"PRId32"  ptr %p, remaining %"PRId32"\n", longs, data, md->lseglongs);
	ULONG cnt = 0;
	md->lseghasword = FALSE;
    1e5e:	426b 002e      	clr.w 46(a3)
	while (longs > cnt) {
    1e62:	4a83           	tst.l d3
    1e64:	673c           	beq.s 1ea2 1ea2 _W_DeleteMsgPort+0x158
    1e66:	202b 0020      	move.l 32(a3),d0
    1e6a:	7400           	moveq #0,d2
		if (md->lseglongs > 0) {
    1e6c:	4a80           	tst.l d0
    1e6e:	673a           	beq.s 1eaa 1eaa _W_DeleteMsgPort+0x160
    1e70:	206b 0028      	movea.l 40(a3),a0
			data[cnt] = md->lsegbuf->lsb_LoadData[md->lsegoffset];
    1e74:	202b 0024      	move.l 36(a3),d0
    1e78:	5a80           	addq.l #5,d0
    1e7a:	e588           	lsl.l #2,d0
    1e7c:	2202           	move.l d2,d1
    1e7e:	e589           	lsl.l #2,d1
    1e80:	29b0 0800 1800 	move.l (0,a0,d0.l),(0,a4,d1.l)
			md->lsegoffset++;
    1e86:	52ab 0024      	addq.l #1,36(a3)
			md->lseglongs--;
    1e8a:	202b 0020      	move.l 32(a3),d0
    1e8e:	5380           	subq.l #1,d0
    1e90:	2740 0020      	move.l d0,32(a3)
			cnt++;
    1e94:	5282           	addq.l #1,d2
			if (longs == cnt) {
    1e96:	b483           	cmp.l d3,d2
    1e98:	6708           	beq.s 1ea2 1ea2 _W_DeleteMsgPort+0x158
				return TRUE;
			}
		}
		if (!md->lseglongs) {
    1e9a:	4a80           	tst.l d0
    1e9c:	670c           	beq.s 1eaa 1eaa _W_DeleteMsgPort+0x160
	while (longs > cnt) {
    1e9e:	b483           	cmp.l d3,d2
    1ea0:	65ca           	bcs.s 1e6c 1e6c _W_DeleteMsgPort+0x122
			md->lsegoffset = 0;
			dbg("lseg_read_long lseg block %"PRId32" loaded, next %"PRId32"\n", md->lsegblock, md->lsegbuf->lsb_Next);
			md->lsegblock = md->lsegbuf->lsb_Next;
		}
	}
	return TRUE;
    1ea2:	7001           	moveq #1,d0
}
    1ea4:	4cdf 7c3c      	movem.l (sp)+,d2-d5/a2-a6
    1ea8:	4e75           	rts
			if (md->lsegblock == 0xffffffff) {
    1eaa:	202b 001c      	move.l 28(a3),d0
    1eae:	72ff           	moveq #-1,d1
    1eb0:	b280           	cmp.l d0,d1
    1eb2:	6740           	beq.s 1ef4 1ef4 _W_DeleteMsgPort+0x1aa
			if (!readblock((UBYTE*)md->lsegbuf, md->lsegblock, IDNAME_LOADSEG, md)) {
    1eb4:	246b 0028      	movea.l 40(a3),a2
	struct ExecBase *SysBase = md->SysBase;
    1eb8:	2c53           	movea.l (a3),a6
	struct IOExtTD *request = md->request;
    1eba:	2a6b 000c      	movea.l 12(a3),a5
	request->iotd_Req.io_Command = CMD_READ;
    1ebe:	3b7c 0002 001c 	move.w #2,28(a5)
	request->iotd_Req.io_Offset = block << 9;
    1ec4:	e188           	lsl.l #8,d0
    1ec6:	e388           	lsl.l #1,d0
    1ec8:	2b40 002c      	move.l d0,44(a5)
	request->iotd_Req.io_Data = buf;
    1ecc:	2b4a 0028      	move.l a2,40(a5)
	request->iotd_Req.io_Length = md->blocksize;
    1ed0:	2b6b 183c 0024 	move.l 6204(a3),36(a5)
		LONG err = DoIO((struct IORequest*)request);
    1ed6:	224d           	movea.l a5,a1
    1ed8:	4eae fe38      	jsr -456(a6)
		if (!err) {
    1edc:	4a00           	tst.b d0
    1ede:	671c           	beq.s 1efc 1efc _W_DeleteMsgPort+0x1b2
		LONG err = DoIO((struct IORequest*)request);
    1ee0:	224d           	movea.l a5,a1
    1ee2:	4eae fe38      	jsr -456(a6)
		if (!err) {
    1ee6:	4a00           	tst.b d0
    1ee8:	6712           	beq.s 1efc 1efc _W_DeleteMsgPort+0x1b2
		LONG err = DoIO((struct IORequest*)request);
    1eea:	224d           	movea.l a5,a1
    1eec:	4eae fe38      	jsr -456(a6)
		if (!err) {
    1ef0:	4a00           	tst.b d0
    1ef2:	6708           	beq.s 1efc 1efc _W_DeleteMsgPort+0x1b2
				return FALSE;
    1ef4:	4240           	clr.w d0
}
    1ef6:	4cdf 7c3c      	movem.l (sp)+,d2-d5/a2-a6
    1efa:	4e75           	rts
		if (v != id) {
    1efc:	7200           	moveq #0,d1
    1efe:	1212           	move.b (a2),d1
    1f00:	2001           	move.l d1,d0
    1f02:	e148           	lsl.w #8,d0
    1f04:	4840           	swap d0
    1f06:	4240           	clr.w d0
    1f08:	122a 0001      	move.b 1(a2),d1
    1f0c:	4841           	swap d1
    1f0e:	4241           	clr.w d1
    1f10:	8280           	or.l d0,d1
    1f12:	7000           	moveq #0,d0
    1f14:	102a 0002      	move.b 2(a2),d0
    1f18:	e188           	lsl.l #8,d0
    1f1a:	8081           	or.l d1,d0
    1f1c:	7200           	moveq #0,d1
    1f1e:	122a 0003      	move.b 3(a2),d1
    1f22:	8081           	or.l d1,d0
    1f24:	0c80 4c53 4547 	cmpi.l #1280525639,d0
    1f2a:	66c8           	bne.s 1ef4 1ef4 _W_DeleteMsgPort+0x1aa
    1f2c:	282b 183c      	move.l 6204(a3),d4
	for (UWORD i = 0; i < md->blocksize; i += 4) {
    1f30:	6f3e           	ble.s 1f70 1f70 _W_DeleteMsgPort+0x226
    1f32:	93c9           	suba.l a1,a1
    1f34:	3049           	movea.w a1,a0
    1f36:	2a09           	move.l a1,d5
		ULONG v = (buf[i + 0] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | (buf[i + 3 ] << 0);
    1f38:	7000           	moveq #0,d0
    1f3a:	1032 5800      	move.b (0,a2,d5.l),d0
    1f3e:	e148           	lsl.w #8,d0
    1f40:	4840           	swap d0
    1f42:	4240           	clr.w d0
    1f44:	7200           	moveq #0,d1
    1f46:	1232 5803      	move.b (3,a2,d5.l),d1
    1f4a:	8081           	or.l d1,d0
    1f4c:	1232 5801      	move.b (1,a2,d5.l),d1
    1f50:	4841           	swap d1
    1f52:	4241           	clr.w d1
    1f54:	8081           	or.l d1,d0
    1f56:	7200           	moveq #0,d1
    1f58:	1232 5802      	move.b (2,a2,d5.l),d1
    1f5c:	e189           	lsl.l #8,d1
    1f5e:	8081           	or.l d1,d0
		chk += v;
    1f60:	d3c0           	adda.l d0,a1
	for (UWORD i = 0; i < md->blocksize; i += 4) {
    1f62:	5848           	addq.w #4,a0
    1f64:	7a00           	moveq #0,d5
    1f66:	3a08           	move.w a0,d5
    1f68:	ba84           	cmp.l d4,d5
    1f6a:	6dcc           	blt.s 1f38 1f38 _W_DeleteMsgPort+0x1ee
			if (!readblock((UBYTE*)md->lsegbuf, md->lsegblock, IDNAME_LOADSEG, md)) {
    1f6c:	2209           	move.l a1,d1
    1f6e:	6684           	bne.s 1ef4 1ef4 _W_DeleteMsgPort+0x1aa
			md->lseglongs = LSEG_DATASIZE;
    1f70:	707b           	moveq #123,d0
    1f72:	2740 0020      	move.l d0,32(a3)
			md->lsegoffset = 0;
    1f76:	42ab 0024      	clr.l 36(a3)
			md->lsegblock = md->lsegbuf->lsb_Next;
    1f7a:	206b 0028      	movea.l 40(a3),a0
    1f7e:	2768 0010 001c 	move.l 16(a0),28(a3)
	while (longs > cnt) {
    1f84:	b483           	cmp.l d3,d2
    1f86:	6500 feec      	bcs.w 1e74 1e74 _W_DeleteMsgPort+0x12a
	return TRUE;
    1f8a:	7001           	moveq #1,d0
    1f8c:	6000 ff16      	bra.w 1ea4 1ea4 _W_DeleteMsgPort+0x15a
    1f90:	4669 6c65      	not.w 27749(a1)
    1f94:	5379 7374 656d 	subq.w #1,7374656d 7374656d ___a4_init+0x7373e56f
    1f9a:	2e72 6573 6f75 	movea.l ([1869967971,a2],1694519279),sp
    1fa0:	7263 6500 4fef 
	return firstProcessedHunk;
}

// Scan FileSystem.resource, create new if it is not found or existing entry has older version number.
static struct FileSysEntry *FSHDProcess(struct FileSysHeaderBlock *fshb, ULONG dostype, ULONG version, BOOL newOnly, struct MountData *md)
{
    1fa6:	ffec           	.short 0xffec
    1fa8:	48e7 3e3e      	movem.l d2-d6/a2-a6,-(sp)
    1fac:	41ef 0040      	lea 64(sp),a0
    1fb0:	2858           	movea.l (a0)+,a4
    1fb2:	2418           	move.l (a0)+,d2
    1fb4:	2818           	move.l (a0)+,d4
    1fb6:	2a18           	move.l (a0)+,d5
    1fb8:	2050           	movea.l (a0),a0
	struct ExecBase *SysBase = md->SysBase;
    1fba:	2650           	movea.l (a0),a3
	struct FileSysEntry *fse = NULL;
	const UBYTE *creator = md->creator ? md->creator : md->zero;
    1fbc:	2628 0014      	move.l 20(a0),d3
    1fc0:	6700 00f0      	beq.w 20b2 20b2 _W_DeleteMsgPort+0x368
	const char resourceName[] = "FileSystem.resource";
    1fc4:	45ef 0028      	lea 40(sp),a2
    1fc8:	204a           	movea.l a2,a0
    1fca:	43fa ffc4      	lea 1f90 1f90 _W_DeleteMsgPort+0x246(pc),a1
    1fce:	10d9           	move.b (a1)+,(a0)+
    1fd0:	10d9           	move.b (a1)+,(a0)+
    1fd2:	10d9           	move.b (a1)+,(a0)+
    1fd4:	10d9           	move.b (a1)+,(a0)+
    1fd6:	10d9           	move.b (a1)+,(a0)+
    1fd8:	10d9           	move.b (a1)+,(a0)+
    1fda:	10d9           	move.b (a1)+,(a0)+
    1fdc:	10d9           	move.b (a1)+,(a0)+
    1fde:	10d9           	move.b (a1)+,(a0)+
    1fe0:	10d9           	move.b (a1)+,(a0)+
    1fe2:	10d9           	move.b (a1)+,(a0)+
    1fe4:	10d9           	move.b (a1)+,(a0)+
    1fe6:	10d9           	move.b (a1)+,(a0)+
    1fe8:	10d9           	move.b (a1)+,(a0)+
    1fea:	10d9           	move.b (a1)+,(a0)+
    1fec:	10d9           	move.b (a1)+,(a0)+
    1fee:	10d9           	move.b (a1)+,(a0)+
    1ff0:	10d9           	move.b (a1)+,(a0)+
    1ff2:	10d9           	move.b (a1)+,(a0)+
    1ff4:	10d9           	move.b (a1)+,(a0)+

	Forbid();
    1ff6:	2c4b           	movea.l a3,a6
    1ff8:	4eae ff7c      	jsr -132(a6)
	
	struct FileSysResource *fsr = NULL;
	fsr = OpenResource(FSRNAME);
    1ffc:	43fa ff92      	lea 1f90 1f90 _W_DeleteMsgPort+0x246(pc),a1
    2000:	4eae fe0e      	jsr -498(a6)
	if (!fsr) {
    2004:	4a80           	tst.l d0
    2006:	6700 00f8      	beq.w 2100 2100 _W_DeleteMsgPort+0x3b6
    200a:	2040           	movea.l d0,a0
			AddTail(&SysBase->ResourceList, &fsr->fsr_Node);
		}
		dbg("FileSystem.resource created %p\n", fsr);
	}
	if (fsr) {
		fse = (struct FileSysEntry*)fsr->fsr_FileSysEntries.lh_Head;
    200c:	2468 0012      	movea.l 18(a0),a2
		while (fse->fse_Node.ln_Succ)  {
    2010:	2012           	move.l (a2),d0
    2012:	670c           	beq.s 2020 2020 _W_DeleteMsgPort+0x2d6
			if (fse->fse_DosType == dostype) {
    2014:	b4aa 000e      	cmp.l 14(a2),d2
    2018:	677a           	beq.s 2094 2094 _W_DeleteMsgPort+0x34a
    201a:	2440           	movea.l d0,a2
		while (fse->fse_Node.ln_Succ)  {
    201c:	2012           	move.l (a2),d0
    201e:	66f4           	bne.s 2014 2014 _W_DeleteMsgPort+0x2ca
					goto end;
				}
			}
			fse = (struct FileSysEntry*)fse->fse_Node.ln_Succ;
		}
		if (fshb && newOnly) {
    2020:	200c           	move.l a4,d0
    2022:	675e           	beq.s 2082 2082 _W_DeleteMsgPort+0x338
    2024:	4a45           	tst.w d5
    2026:	675a           	beq.s 2082 2082 _W_DeleteMsgPort+0x338
    2028:	2043           	movea.l d3,a0
}

__MY_INLINE__ __stdargs size_t strlen(const char *string)
{	const char *s=string;

	while(*s++) {}return ~(string-s);
    202a:	1018           	move.b (a0)+,d0
    202c:	66fc           	bne.s 202a 202a _W_DeleteMsgPort+0x2e0
    202e:	2003           	move.l d3,d0
    2030:	9088           	sub.l a0,d0
    2032:	4680           	not.l d0
			fse = AllocMem(sizeof(struct FileSysEntry) + strlen(creator) + 1, MEMF_PUBLIC | MEMF_CLEAR);
    2034:	723f           	moveq #63,d1
    2036:	2c4b           	movea.l a3,a6
    2038:	d081           	add.l d1,d0
    203a:	223c 0001 0001 	move.l #65537,d1
    2040:	4eae ff3a      	jsr -198(a6)
    2044:	2440           	movea.l d0,a2
			if (fse) {
    2046:	4a80           	tst.l d0
    2048:	6754           	beq.s 209e 209e _W_DeleteMsgPort+0x354
				// Process patchflags
				ULONG *dstPatch = &fse->fse_Type;
    204a:	41ea 001a      	lea 26(a2),a0
				ULONG *srcPatch = &fshb->fhb_Type;
    204e:	43ec 002c      	lea 44(a4),a1
				ULONG patchFlags = fshb->fhb_PatchFlags;
    2052:	202c 0028      	move.l 40(a4),d0
				while (patchFlags) {
    2056:	670a           	beq.s 2062 2062 _W_DeleteMsgPort+0x318
					*dstPatch++ = *srcPatch++;
    2058:	20d9           	move.l (a1)+,(a0)+
					patchFlags >>= 1;
    205a:	e288           	lsr.l #1,d0
				while (patchFlags) {
    205c:	66fa           	bne.s 2058 2058 _W_DeleteMsgPort+0x30e
    205e:	202c 0028      	move.l 40(a4),d0
				}
				fse->fse_DosType = fshb->fhb_DosType;
    2062:	256c 0020 000e 	move.l 32(a4),14(a2)
				fse->fse_Version = fshb->fhb_Version;
    2068:	256c 0024 0012 	move.l 36(a4),18(a2)
				fse->fse_PatchFlags = fshb->fhb_PatchFlags;
    206e:	2540 0016      	move.l d0,22(a2)
				strcpy((UBYTE*)(fse + 1), creator);
    2072:	49ea 003e      	lea 62(a2),a4
    2076:	224c           	movea.l a4,a1
    2078:	2043           	movea.l d3,a0
    207a:	12d8           	move.b (a0)+,(a1)+
	while(*s++) {}return (s-string);
}

__MY_INLINE__ __stdargs char *strcpy(char *s1,const char *s2)
{	char *s=s1;
	while((*s1++=*s2++)) {}
    207c:	66fc           	bne.s 207a 207a _W_DeleteMsgPort+0x330
				fse->fse_Node.ln_Name = (UBYTE*)(fse + 1);
    207e:	254c 000a      	move.l a4,10(a2)
			}
			dbg("FileSystem.resource scan: dostype %08"PRIx32" not found or old version: created new\n", dostype);
		}
	}
end:
	Permit();
    2082:	2c4b           	movea.l a3,a6
    2084:	4eae ff76      	jsr -138(a6)
	return fse;
}
    2088:	200a           	move.l a2,d0
    208a:	4cdf 7c7c      	movem.l (sp)+,d2-d6/a2-a6
    208e:	4fef 0014      	lea 20(sp),sp
    2092:	4e75           	rts
				if (fse->fse_Version >= version) {
    2094:	b8aa 0012      	cmp.l 18(a2),d4
    2098:	6280           	bhi.s 201a 201a _W_DeleteMsgPort+0x2d0
					if (newOnly) {
    209a:	4a45           	tst.w d5
    209c:	67e4           	beq.s 2082 2082 _W_DeleteMsgPort+0x338
	Permit();
    209e:	2c4b           	movea.l a3,a6
						fse = NULL;
    20a0:	95ca           	suba.l a2,a2
	Permit();
    20a2:	4eae ff76      	jsr -138(a6)
}
    20a6:	200a           	move.l a2,d0
    20a8:	4cdf 7c7c      	movem.l (sp)+,d2-d6/a2-a6
    20ac:	4fef 0014      	lea 20(sp),sp
    20b0:	4e75           	rts
	const UBYTE *creator = md->creator ? md->creator : md->zero;
    20b2:	2608           	move.l a0,d3
    20b4:	0683 0000 1838 	addi.l #6200,d3
	const char resourceName[] = "FileSystem.resource";
    20ba:	45ef 0028      	lea 40(sp),a2
    20be:	204a           	movea.l a2,a0
    20c0:	43fa fece      	lea 1f90 1f90 _W_DeleteMsgPort+0x246(pc),a1
    20c4:	10d9           	move.b (a1)+,(a0)+
    20c6:	10d9           	move.b (a1)+,(a0)+
    20c8:	10d9           	move.b (a1)+,(a0)+
    20ca:	10d9           	move.b (a1)+,(a0)+
    20cc:	10d9           	move.b (a1)+,(a0)+
    20ce:	10d9           	move.b (a1)+,(a0)+
    20d0:	10d9           	move.b (a1)+,(a0)+
    20d2:	10d9           	move.b (a1)+,(a0)+
    20d4:	10d9           	move.b (a1)+,(a0)+
    20d6:	10d9           	move.b (a1)+,(a0)+
    20d8:	10d9           	move.b (a1)+,(a0)+
    20da:	10d9           	move.b (a1)+,(a0)+
    20dc:	10d9           	move.b (a1)+,(a0)+
    20de:	10d9           	move.b (a1)+,(a0)+
    20e0:	10d9           	move.b (a1)+,(a0)+
    20e2:	10d9           	move.b (a1)+,(a0)+
    20e4:	10d9           	move.b (a1)+,(a0)+
    20e6:	10d9           	move.b (a1)+,(a0)+
    20e8:	10d9           	move.b (a1)+,(a0)+
    20ea:	10d9           	move.b (a1)+,(a0)+
	Forbid();
    20ec:	2c4b           	movea.l a3,a6
    20ee:	4eae ff7c      	jsr -132(a6)
	fsr = OpenResource(FSRNAME);
    20f2:	43fa fe9c      	lea 1f90 1f90 _W_DeleteMsgPort+0x246(pc),a1
    20f6:	4eae fe0e      	jsr -498(a6)
	if (!fsr) {
    20fa:	4a80           	tst.l d0
    20fc:	6600 ff0c      	bne.w 200a 200a _W_DeleteMsgPort+0x2c0
    2100:	204a           	movea.l a2,a0
	while(*s++) {}return ~(string-s);
    2102:	1018           	move.b (a0)+,d0
    2104:	66fc           	bne.s 2102 2102 _W_DeleteMsgPort+0x3b8
    2106:	220a           	move.l a2,d1
    2108:	9288           	sub.l a0,d1
    210a:	4681           	not.l d1
{	const char *s=string;
    210c:	2043           	movea.l d3,a0
	while(*s++) {}return ~(string-s);
    210e:	1018           	move.b (a0)+,d0
    2110:	66fc           	bne.s 210e 210e _W_DeleteMsgPort+0x3c4
    2112:	91c3           	suba.l d3,a0
		fsr = AllocMem(sizeof(struct FileSysResource) + strlen(resourceName) + 1 + strlen(creator) + 1, MEMF_PUBLIC | MEMF_CLEAR);
    2114:	41f0 1821      	lea (33,a0,d1.l),a0
    2118:	2c4b           	movea.l a3,a6
    211a:	2008           	move.l a0,d0
    211c:	223c 0001 0001 	move.l #65537,d1
    2122:	4eae ff3a      	jsr -198(a6)
    2126:	2a40           	movea.l d0,a5
		if (fsr) {
    2128:	4a80           	tst.l d0
    212a:	6700 ff72      	beq.w 209e 209e _W_DeleteMsgPort+0x354
			char *FsResName  = (UBYTE *)(fsr + 1);
    212e:	7c20           	moveq #32,d6
    2130:	dc80           	add.l d0,d6
{	const char *s=string;
    2132:	204a           	movea.l a2,a0
	while(*s++) {}return ~(string-s);
    2134:	1018           	move.b (a0)+,d0
    2136:	66fc           	bne.s 2134 2134 _W_DeleteMsgPort+0x3ea
    2138:	224a           	movea.l a2,a1
			char *CreatorStr = (UBYTE *)FsResName + (strlen(resourceName) + 1);
    213a:	2c46           	movea.l d6,a6
    213c:	93c8           	suba.l a0,a1
    213e:	9dc9           	suba.l a1,a6
			NewList(&fsr->fsr_FileSysEntries);
    2140:	486d 0012      	pea 18(a5)
    2144:	4eb9 0000 3314 	jsr 3314 3314 _NewList
			fsr->fsr_Node.ln_Type = NT_RESOURCE;
    214a:	1b7c 0008 0008 	move.b #8,8(a5)
    2150:	2046           	movea.l d6,a0
    2152:	588f           	addq.l #4,sp
    2154:	10da           	move.b (a2)+,(a0)+
	while((*s1++=*s2++)) {}
    2156:	66fc           	bne.s 2154 2154 _W_DeleteMsgPort+0x40a
			fsr->fsr_Node.ln_Name = FsResName;
    2158:	2b46 000a      	move.l d6,10(a5)
    215c:	224e           	movea.l a6,a1
    215e:	2043           	movea.l d3,a0
    2160:	12d8           	move.b (a0)+,(a1)+
    2162:	66fc           	bne.s 2160 2160 _W_DeleteMsgPort+0x416
			fsr->fsr_Creator = CreatorStr;
    2164:	2b4e 000e      	move.l a6,14(a5)
			AddTail(&SysBase->ResourceList, &fsr->fsr_Node);
    2168:	2c4b           	movea.l a3,a6
    216a:	224d           	movea.l a5,a1
    216c:	41eb 0150      	lea 336(a3),a0
    2170:	4eae ff0a      	jsr -246(a6)
		fse = (struct FileSysEntry*)fsr->fsr_FileSysEntries.lh_Head;
    2174:	246d 0012      	movea.l 18(a5),a2
    2178:	6000 fe96      	bra.w 2010 2010 _W_DeleteMsgPort+0x2c6
    217c:	6578           	bcs.s 21f6 21f6 _MountDrive+0x5c
    217e:	7061           	moveq #97,d0
    2180:	6e73           	bgt.s 21f5 21f5 _MountDrive+0x5b
    2182:	696f           	bvs.s 21f3 21f3 _MountDrive+0x59
    2184:	6e2e           	bgt.s 21b4 21b4 _MountDrive+0x1a
    2186:	6c69           	bge.s 21f1 21f1 _MountDrive+0x57
    2188:	6272           	bhi.s 21fc 21fc _MountDrive+0x62
    218a:	6172           	bsr.s 21fe 21fe _MountDrive+0x64
    218c:	7900           	.short 0x7900
    218e:	646f           	bcc.s 21ff 21ff _MountDrive+0x65
    2190:	732e           	.short 0x732e
    2192:	6c69           	bge.s 21fd 21fd _MountDrive+0x63
    2194:	6272           	bhi.s 2208 2208 _MountDrive+0x6e
    2196:	6172           	bsr.s 220a 220a _MountDrive+0x70
    2198:	7900           	.short 0x7900

0000219a 0000219a _MountDrive:
// If unit number array:
// Unit number is replaced with error code:
// Error codes are same as above except:
// -2 = Skipped, previous unit had RDBFF_LAST set.
LONG MountDrive(struct MountStruct *ms)
{
    219a:	4e55 ff78      	link.w a5,#-136
    219e:	48e7 3f3a      	movem.l d2-d7/a2-a4/a6,-(sp)
	LONG ret = -1;
	struct MsgPort *port = NULL;
	struct IOExtTD *request = NULL;
	struct ExpansionBase *ExpansionBase;
	struct ExecBase *SysBase = ms->SysBase;
    21a2:	206d 0008      	movea.l 8(a5),a0
    21a6:	2b68 0010 ff8c 	move.l 16(a0),-116(a5)

	dbg("Starting..\n");
	ExpansionBase = (struct ExpansionBase*)OpenLibrary("expansion.library", 34);
    21ac:	2c6d ff8c      	movea.l -116(a5),a6
    21b0:	7022           	moveq #34,d0
    21b2:	43fa ffc8      	lea 217c 217c _W_DeleteMsgPort+0x432(pc),a1
    21b6:	4eae fdd8      	jsr -552(a6)
    21ba:	2b40 ffb0      	move.l d0,-80(a5)
	if (ExpansionBase) {
    21be:	6700 109e      	beq.w 325e 325e _MountDrive+0x10c4
		struct MountData *md = AllocMem(sizeof(struct MountData), MEMF_CLEAR | MEMF_PUBLIC);
    21c2:	203c 0000 1840 	move.l #6208,d0
    21c8:	223c 0001 0001 	move.l #65537,d1
    21ce:	4eae ff3a      	jsr -198(a6)
    21d2:	2640           	movea.l d0,a3
		if (md) {
    21d4:	4a80           	tst.l d0
    21d6:	6700 089e      	beq.w 2a76 2a76 _MountDrive+0x8dc
			md->DOSBase = (struct DosLibrary*)OpenLibrary("dos.library", 34);
    21da:	7022           	moveq #34,d0
    21dc:	43fa ffb0      	lea 218e 218e _W_DeleteMsgPort+0x444(pc),a1
    21e0:	4eae fdd8      	jsr -552(a6)
    21e4:	2740 0008      	move.l d0,8(a3)
			md->SysBase = SysBase;
    21e8:	26ad ff8c      	move.l -116(a5),(a3)
			md->ExpansionBase = ExpansionBase;
    21ec:	276d ffb0 0004 	move.l -80(a5),4(a3)
			dbg("SysBase=%p ExpansionBase=%p DosBase=%p\n", md->SysBase, md->ExpansionBase, md->DOSBase);
			md->configDev = ms->configDev;
    21f2:	206d 0008      	movea.l 8(a5),a0
    21f6:	2768 000c 0010 	move.l 12(a0),16(a3)
			md->creator = ms->creatorName;
    21fc:	2768 0008 0014 	move.l 8(a0),20(a3)
	ret = (struct MsgPort*)AllocMem(sizeof(struct MsgPort), MEMF_PUBLIC | MEMF_CLEAR);
    2202:	7022           	moveq #34,d0
    2204:	223c 0001 0001 	move.l #65537,d1
    220a:	4eae ff3a      	jsr -198(a6)
    220e:	2b40 ff9c      	move.l d0,-100(a5)
	if(ret != NULL)
    2212:	6700 0ed0      	beq.w 30e4 30e4 _MountDrive+0xf4a
		BYTE sb = AllocSignal(-1);
    2216:	50c0           	st d0
    2218:	4eae feb6      	jsr -330(a6)
    221c:	1400           	move.b d0,d2
		if (sb != -1)
    221e:	0c00 00ff      	cmpi.b #-1,d0
    2222:	6700 0a08      	beq.w 2c2c 2c2c _MountDrive+0xa92
			ret->mp_Flags = PA_SIGNAL;
    2226:	206d ff9c      	movea.l -100(a5),a0
    222a:	4228 000e      	clr.b 14(a0)
			ret->mp_Node.ln_Type = NT_MSGPORT;
    222e:	117c 0004 0008 	move.b #4,8(a0)
  			NewList(&ret->mp_MsgList);
    2234:	4868 0014      	pea 20(a0)
    2238:	4eb9 0000 3314 	jsr 3314 3314 _NewList
			ret->mp_SigBit = sb;
    223e:	206d ff9c      	movea.l -100(a5),a0
    2242:	1142 000f      	move.b d2,15(a0)
			ret->mp_SigTask = FindTask(NULL);
    2246:	93c9           	suba.l a1,a1
    2248:	4eae feda      	jsr -294(a6)
    224c:	206d ff9c      	movea.l -100(a5),a0
    2250:	2140 0010      	move.l d0,16(a0)
	ret = (struct IORequest*)AllocMem(size, MEMF_PUBLIC | MEMF_CLEAR);
    2254:	7038           	moveq #56,d0
    2256:	223c 0001 0001 	move.l #65537,d1
    225c:	4eae ff3a      	jsr -198(a6)
    2260:	2b40 ff84      	move.l d0,-124(a5)
	if(ret != NULL)
    2264:	588f           	addq.l #4,sp
    2266:	6700 09a4      	beq.w 2c0c 2c0c _MountDrive+0xa72
    226a:	2040           	movea.l d0,a0
		ret->io_Message.mn_ReplyPort = ioReplyPort;
    226c:	216d ff9c 000e 	move.l -100(a5),14(a0)
		ret->io_Message.mn_Length = size;
    2272:	317c 0038 0012 	move.w #56,18(a0)
			port = W_CreateMsgPort(SysBase);
			if(port) {
				request = (struct IOExtTD*)W_CreateIORequest(port, sizeof(struct IOExtTD), SysBase);
				if(request) {
					ULONG *unitNumP = ms->unitNum;
    2278:	206d 0008      	movea.l 8(a5),a0
					ULONG unitNumVal[2];
					unitNumVal[0] = 1;
    227c:	7001           	moveq #1,d0
					ULONG *unitNumP = ms->unitNum;
    227e:	2268 0004      	movea.l 4(a0),a1
					unitNumVal[0] = 1;
    2282:	2b40 fff8      	move.l d0,-8(a5)
					unitNumVal[1] = (ULONG)unitNumP;
    2286:	2b49 fffc      	move.l a1,-4(a5)
					if (unitNumVal[1] < 0x100) {
    228a:	b2fc 00ff      	cmpa.w #255,a1
    228e:	6200 00c6      	bhi.w 2356 2356 _MountDrive+0x1bc
    2292:	7001           	moveq #1,d0
    2294:	2b40 ff80      	move.l d0,-128(a5)
						unitNumP = &unitNumVal[0];
    2298:	43ed fff8      	lea -8(a5),a1
					}
					ULONG unitNumCnt = *unitNumP++;
    229c:	5889           	addq.l #4,a1
			copymem(&pp->de, &part->pb_Environment, 17 * sizeof(ULONG));
    229e:	41eb 00b8      	lea 184(a3),a0
	LONG ret = -1;
    22a2:	74ff           	moveq #-1,d2
			copymem(&pp->de, &part->pb_Environment, 17 * sizeof(ULONG));
    22a4:	2b48 ffc0      	move.l a0,-64(a5)
    22a8:	2b40 ff7c      	move.l d0,-132(a5)
    22ac:	2a09           	move.l a1,d5
					while (unitNumCnt-- > 0) {
    22ae:	5380           	subq.l #1,d0
    22b0:	2b40 ff88      	move.l d0,-120(a5)
    22b4:	4aad ff7c      	tst.l -132(a5)
    22b8:	6744           	beq.s 22fe 22fe _MountDrive+0x164
						ULONG unitNum = *unitNumP;
    22ba:	2045           	movea.l d5,a0
    22bc:	2618           	move.l (a0)+,d3
    22be:	2a08           	move.l a0,d5
						dbg("OpenDevice('%s', %"PRId32", %p, 0)\n", ms->deviceName, unitNum, request);
						UBYTE err = OpenDevice(ms->deviceName, unitNum, (struct IORequest*)request, 0);
    22c0:	2c6d ff8c      	movea.l -116(a5),a6
    22c4:	226d 0008      	movea.l 8(a5),a1
    22c8:	2051           	movea.l (a1),a0
    22ca:	2003           	move.l d3,d0
    22cc:	7200           	moveq #0,d1
    22ce:	226d ff84      	movea.l -124(a5),a1
    22d2:	4eae fe44      	jsr -444(a6)
						if (err == 0) {
    22d6:	4a00           	tst.b d0
    22d8:	6700 0098      	beq.w 2372 2372 _MountDrive+0x1d8
								dbg("RDBFF_LAST exit\n");
								break;
							}
						} else {
							dbg("OpenDevice(%s,%"PRId32") failed: %"PRId32"\n", ms->deviceName, unitNum, (BYTE)err);
							*unitNumP++ = -1;
    22dc:	2045           	movea.l d5,a0
    22de:	70ff           	moveq #-1,d0
    22e0:	2140 fffc      	move.l d0,-4(a0)
    22e4:	53ad ff7c      	subq.l #1,-132(a5)
					while (unitNumCnt-- > 0) {
    22e8:	2b6d ff88 ff80 	move.l -120(a5),-128(a5)
    22ee:	202d ff80      	move.l -128(a5),d0
    22f2:	5380           	subq.l #1,d0
    22f4:	2b40 ff88      	move.l d0,-120(a5)
    22f8:	4aad ff7c      	tst.l -132(a5)
    22fc:	66bc           	bne.s 22ba 22ba _MountDrive+0x120
		FreeMem(iorequest, ((struct Message*)iorequest)->mn_Length);
    22fe:	2c6d ff8c      	movea.l -116(a5),a6
    2302:	226d ff84      	movea.l -124(a5),a1
    2306:	7000           	moveq #0,d0
    2308:	3029 0012      	move.w 18(a1),d0
    230c:	4eae ff2e      	jsr -210(a6)
		FreeSignal(port->mp_SigBit);
    2310:	206d ff9c      	movea.l -100(a5),a0
    2314:	1028 000f      	move.b 15(a0),d0
    2318:	4eae feb0      	jsr -336(a6)
		FreeMem(port, sizeof(struct MsgPort));
    231c:	7022           	moveq #34,d0
    231e:	226d ff9c      	movea.l -100(a5),a1
    2322:	4eae ff2e      	jsr -210(a6)
					}
					W_DeleteIORequest(request, SysBase);
				}
				W_DeleteMsgPort(port, SysBase);
			}
			FreeMem(md, sizeof(struct MountData));
    2326:	2c6d ff8c      	movea.l -116(a5),a6
    232a:	203c 0000 1840 	move.l #6208,d0
    2330:	224b           	movea.l a3,a1
    2332:	4eae ff2e      	jsr -210(a6)
			if (md->DOSBase) {
    2336:	226b 0008      	movea.l 8(a3),a1
    233a:	2009           	move.l a1,d0
    233c:	6704           	beq.s 2342 2342 _MountDrive+0x1a8
				CloseLibrary(&md->DOSBase->dl_lib);
    233e:	4eae fe62      	jsr -414(a6)
			}
		}
		CloseLibrary(&ExpansionBase->LibNode);
    2342:	226d ffb0      	movea.l -80(a5),a1
    2346:	4eae fe62      	jsr -414(a6)
	}
	dbg("Exit code %"PRId32"\n", ret);
	return ret;
}
    234a:	2002           	move.l d2,d0
    234c:	4ced 5cfc ff50 	movem.l -176(a5),d2-d7/a2-a4/a6
    2352:	4e5d           	unlk a5
    2354:	4e75           	rts
    2356:	2b59 ff80      	move.l (a1)+,-128(a5)
    235a:	202d ff80      	move.l -128(a5),d0
			copymem(&pp->de, &part->pb_Environment, 17 * sizeof(ULONG));
    235e:	41eb 00b8      	lea 184(a3),a0
	LONG ret = -1;
    2362:	74ff           	moveq #-1,d2
			copymem(&pp->de, &part->pb_Environment, 17 * sizeof(ULONG));
    2364:	2b48 ffc0      	move.l a0,-64(a5)
    2368:	2b40 ff7c      	move.l d0,-132(a5)
    236c:	2a09           	move.l a1,d5
    236e:	6000 ff3e      	bra.w 22ae 22ae _MountDrive+0x114
							md->request = request;
    2372:	276d ff84 000c 	move.l -124(a5),12(a3)
							md->devicename = ms->deviceName;
    2378:	206d 0008      	movea.l 8(a5),a0
    237c:	2750 0018      	move.l (a0),24(a3)
							md->unitnum = unitNum;
    2380:	2743 0030      	move.l d3,48(a3)
                            md->blocksize=512;
    2384:	277c 0000 0200 	move.l #512,6204(a3)
    238a:	183c 
	struct ExecBase *SysBase = md->SysBase;
    238c:	2b53 ff94      	move.l (a3),-108(a5)
    2390:	49eb 0038      	lea 56(a3),a4
    2394:	246d ff84      	movea.l -124(a5),a2
    2398:	2c6d ff94      	movea.l -108(a5),a6
    239c:	7600           	moveq #0,d3
    239e:	203c 0000 0200 	move.l #512,d0
			if (rdb->rdb_ID == IDNAME_RIGIDDISK) {
    23a4:	283c 5244 534b 	move.l #1380209483,d4
    23aa:	740f           	moveq #15,d2
    23ac:	2e05           	move.l d5,d7
	request->iotd_Req.io_Command = CMD_READ;
    23ae:	357c 0002 001c 	move.w #2,28(a2)
	request->iotd_Req.io_Offset = block << 9;
    23b4:	2543 002c      	move.l d3,44(a2)
	request->iotd_Req.io_Data = buf;
    23b8:	254c 0028      	move.l a4,40(a2)
	request->iotd_Req.io_Length = md->blocksize;
    23bc:	2540 0024      	move.l d0,36(a2)
		LONG err = DoIO((struct IORequest*)request);
    23c0:	224a           	movea.l a2,a1
    23c2:	4eae fe38      	jsr -456(a6)
		if (!err) {
    23c6:	4a00           	tst.b d0
    23c8:	6700 00b0      	beq.w 247a 247a _MountDrive+0x2e0
		LONG err = DoIO((struct IORequest*)request);
    23cc:	224a           	movea.l a2,a1
    23ce:	4eae fe38      	jsr -456(a6)
		if (!err) {
    23d2:	4a00           	tst.b d0
    23d4:	6700 00a4      	beq.w 247a 247a _MountDrive+0x2e0
		LONG err = DoIO((struct IORequest*)request);
    23d8:	224a           	movea.l a2,a1
    23da:	4eae fe38      	jsr -456(a6)
		if (!err) {
    23de:	4a00           	tst.b d0
    23e0:	6700 0098      	beq.w 247a 247a _MountDrive+0x2e0
    23e4:	0683 0000 0200 	addi.l #512,d3
	for (UWORD i = 0; i < RDB_LOCATION_LIMIT; i++) {
    23ea:	51ca 0066      	dbf d2,2452 2452 _MountDrive+0x2b8
    23ee:	2a07           	move.l d7,d5
    23f0:	76ff           	moveq #-1,d3
	md->request->iotd_Req.io_Command = TD_MOTOR;
    23f2:	226b 000c      	movea.l 12(a3),a1
	LONG ret = -1;
    23f6:	2403           	move.l d3,d2
	md->request->iotd_Req.io_Command = TD_MOTOR;
    23f8:	337c 0009 001c 	move.w #9,28(a1)
	md->request->iotd_Req.io_Length  = 0;
    23fe:	42a9 0024      	clr.l 36(a1)
	DoIO((struct IORequest*)md->request);
    2402:	2c6d ff94      	movea.l -108(a5),a6
    2406:	4eae fe38      	jsr -456(a6)
							CloseDevice((struct IORequest*)request);
    240a:	2c6d ff8c      	movea.l -116(a5),a6
    240e:	226d ff84      	movea.l -124(a5),a1
    2412:	4eae fe3e      	jsr -450(a6)
							*unitNumP++ = ret;
    2416:	2045           	movea.l d5,a0
    2418:	2143 fffc      	move.l d3,-4(a0)
							if (md->wasLastDev) {
    241c:	4a6b 183a      	tst.w 6202(a3)
    2420:	6700 fec2      	beq.w 22e4 22e4 _MountDrive+0x14a
								while (unitNumCnt-- > 0) {
    2424:	202d ff80      	move.l -128(a5),d0
    2428:	5580           	subq.l #2,d0
    242a:	4aad ff88      	tst.l -120(a5)
    242e:	6700 fece      	beq.w 22fe 22fe _MountDrive+0x164
									*unitNumP++ = -2;
    2432:	72fe           	moveq #-2,d1
    2434:	20c1           	move.l d1,(a0)+
								while (unitNumCnt-- > 0) {
    2436:	51c8 fffa      	dbf d0,2432 2432 _MountDrive+0x298
    243a:	4240           	clr.w d0
    243c:	5380           	subq.l #1,d0
    243e:	64f2           	bcc.s 2432 2432 _MountDrive+0x298
		FreeMem(iorequest, ((struct Message*)iorequest)->mn_Length);
    2440:	226d ff84      	movea.l -124(a5),a1
    2444:	7000           	moveq #0,d0
    2446:	3029 0012      	move.w 18(a1),d0
    244a:	4eae ff2e      	jsr -210(a6)
    244e:	6000 fec0      	bra.w 2310 2310 _MountDrive+0x176
    2452:	2c53           	movea.l (a3),a6
    2454:	246b 000c      	movea.l 12(a3),a2
    2458:	202b 183c      	move.l 6204(a3),d0
	request->iotd_Req.io_Command = CMD_READ;
    245c:	357c 0002 001c 	move.w #2,28(a2)
	request->iotd_Req.io_Offset = block << 9;
    2462:	2543 002c      	move.l d3,44(a2)
	request->iotd_Req.io_Data = buf;
    2466:	254c 0028      	move.l a4,40(a2)
	request->iotd_Req.io_Length = md->blocksize;
    246a:	2540 0024      	move.l d0,36(a2)
		LONG err = DoIO((struct IORequest*)request);
    246e:	224a           	movea.l a2,a1
    2470:	4eae fe38      	jsr -456(a6)
		if (!err) {
    2474:	4a00           	tst.b d0
    2476:	6600 ff54      	bne.w 23cc 23cc _MountDrive+0x232
    247a:	2a2b 183c      	move.l 6204(a3),d5
	for (UWORD i = 0; i < md->blocksize; i += 4) {
    247e:	6f40           	ble.s 24c0 24c0 _MountDrive+0x326
    2480:	93c9           	suba.l a1,a1
    2482:	3049           	movea.w a1,a0
    2484:	2c09           	move.l a1,d6
		ULONG v = (buf[i + 0] << 24) | (buf[i + 1] << 16) | (buf[i + 2] << 8) | (buf[i + 3 ] << 0);
    2486:	7000           	moveq #0,d0
    2488:	1034 6800      	move.b (0,a4,d6.l),d0
    248c:	e148           	lsl.w #8,d0
    248e:	4840           	swap d0
    2490:	4240           	clr.w d0
    2492:	7200           	moveq #0,d1
    2494:	1234 6803      	move.b (3,a4,d6.l),d1
    2498:	8081           	or.l d1,d0
    249a:	1234 6801      	move.b (1,a4,d6.l),d1
    249e:	4841           	swap d1
    24a0:	4241           	clr.w d1
    24a2:	8081           	or.l d1,d0
    24a4:	7200           	moveq #0,d1
    24a6:	1234 6802      	move.b (2,a4,d6.l),d1
    24aa:	e189           	lsl.l #8,d1
    24ac:	8081           	or.l d1,d0
		chk += v;
    24ae:	d3c0           	adda.l d0,a1
	for (UWORD i = 0; i < md->blocksize; i += 4) {
    24b0:	5848           	addq.w #4,a0
    24b2:	7c00           	moveq #0,d6
    24b4:	3c08           	move.w a0,d6
    24b6:	ba86           	cmp.l d6,d5
    24b8:	6ecc           	bgt.s 2486 2486 _MountDrive+0x2ec
		if (readblock(md->buf, i, 0xffffffff, md)) {
    24ba:	2009           	move.l a1,d0
    24bc:	6600 ff26      	bne.w 23e4 23e4 _MountDrive+0x24a
			if (rdb->rdb_ID == IDNAME_RIGIDDISK) {
    24c0:	b8ab 0038      	cmp.l 56(a3),d4
    24c4:	6600 ff1e      	bne.w 23e4 23e4 _MountDrive+0x24a
    24c8:	2a07           	move.l d7,d5
	ULONG partblock = rdb->rdb_PartitionList;
    24ca:	222b 0054      	move.l 84(a3),d1
	ULONG filesysblock = rdb->rdb_FileSysHeaderList;
    24ce:	2b6b 0058 ffb4 	move.l 88(a3),-76(a5)
	ULONG flags = rdb->rdb_Flags;
    24d4:	2b6b 004c ffa0 	move.l 76(a3),-96(a5)
		if (partblock == 0xffffffff) {
    24da:	70ff           	moveq #-1,d0
    24dc:	b081           	cmp.l d1,d0
    24de:	6736           	beq.s 2516 2516 _MountDrive+0x37c
    24e0:	263c 0000 1d70 	move.l #7536,d3
    24e6:	2001           	move.l d1,d0
    24e8:	2b47 ffa4      	move.l d7,-92(a5)
    24ec:	2b4c ffd0      	move.l a4,-48(a5)
	struct ExecBase *SysBase = md->SysBase;
    24f0:	2e13           	move.l (a3),d7
	struct ExpansionBase *ExpansionBase = md->ExpansionBase;
    24f2:	2b6b 0004 ff90 	move.l 4(a3),-112(a5)
	if (!readblock(buf, block, IDNAME_PARTITION, md)) {
    24f8:	2f0b           	move.l a3,-(sp)
    24fa:	2f3c 5041 5254 	move.l #1346458196,-(sp)
    2500:	2f00           	move.l d0,-(sp)
    2502:	2f2d ffd0      	move.l -48(a5),-(sp)
    2506:	2043           	movea.l d3,a0
    2508:	4e90           	jsr (a0)
    250a:	4fef 0010      	lea 16(sp),sp
    250e:	4a40           	tst.w d0
    2510:	6648           	bne.s 255a 255a _MountDrive+0x3c0
    2512:	2a2d ffa4      	move.l -92(a5),d5
	md->wasLastDev = (flags & RDBFF_LAST) != 0;
    2516:	302d ffa2      	move.w -94(a5),d0
    251a:	0240 0001      	andi.w #1,d0
    251e:	3740 183a      	move.w d0,6202(a3)
	return md->ret;
    2522:	242b 0034      	move.l 52(a3),d2
	md->request->iotd_Req.io_Command = TD_MOTOR;
    2526:	226b 000c      	movea.l 12(a3),a1
    252a:	337c 0009 001c 	move.w #9,28(a1)
	md->request->iotd_Req.io_Length  = 0;
    2530:	42a9 0024      	clr.l 36(a1)
	DoIO((struct IORequest*)md->request);
    2534:	2c6d ff94      	movea.l -108(a5),a6
    2538:	4eae fe38      	jsr -456(a6)
							CloseDevice((struct IORequest*)request);
    253c:	2c6d ff8c      	movea.l -116(a5),a6
    2540:	226d ff84      	movea.l -124(a5),a1
    2544:	4eae fe3e      	jsr -450(a6)
							*unitNumP++ = ret;
    2548:	2045           	movea.l d5,a0
    254a:	2142 fffc      	move.l d2,-4(a0)
							if (md->wasLastDev) {
    254e:	4a6b 183a      	tst.w 6202(a3)
    2552:	6700 fd90      	beq.w 22e4 22e4 _MountDrive+0x14a
    2556:	6000 fecc      	bra.w 2424 2424 _MountDrive+0x28a
	nextpartblock = part->pb_Next;
    255a:	2a2b 0048      	move.l 72(a3),d5
	if (!(part->pb_Flags & PBFF_NOMOUNT)) {
    255e:	082b 0001 004f 	btst #1,79(a3)
    2564:	6600 0178      	bne.w 26de 26de _MountDrive+0x544
		struct ParameterPacket *pp = AllocMem(sizeof(struct ParameterPacket), MEMF_PUBLIC | MEMF_CLEAR);
    2568:	2c47           	movea.l d7,a6
    256a:	7060           	moveq #96,d0
    256c:	223c 0001 0001 	move.l #65537,d1
    2572:	4eae ff3a      	jsr -198(a6)
    2576:	2840           	movea.l d0,a4
		if (pp) {
    2578:	4a80           	tst.l d0
    257a:	6700 0162      	beq.w 26de 26de _MountDrive+0x544
			copymem(&pp->de, &part->pb_Environment, 17 * sizeof(ULONG));
    257e:	43ec 0010      	lea 16(a4),a1
	UBYTE *src = (UBYTE*)srcp;
    2582:	206d ffc0      	movea.l -64(a5),a0
			copymem(&pp->de, &part->pb_Environment, 17 * sizeof(ULONG));
    2586:	7043           	moveq #67,d0
    2588:	222d ffd0      	move.l -48(a5),d1
		*dst++ = *src++;
    258c:	12d8           	move.b (a0)+,(a1)+
	while (size != 0) {
    258e:	51c8 fffc      	dbf d0,258c 258c _MountDrive+0x3f2
    2592:	2b41 ffd0      	move.l d1,-48(a5)
			struct FileSysEntry *fse = ParseFSHD(buf + md->blocksize, filesysblock, pp->de.de_DosType, md);
    2596:	2441           	movea.l d1,a2
    2598:	242c 0050      	move.l 80(a4),d2
    259c:	d5eb 183c      	adda.l 6204(a3),a2
		if (block == 0xffffffff) {
    25a0:	202d ffb4      	move.l -76(a5),d0
    25a4:	72ff           	moveq #-1,d1
    25a6:	b280           	cmp.l d0,d1
    25a8:	6700 0b40      	beq.w 30ea 30ea _MountDrive+0xf50
    25ac:	282d ffd0      	move.l -48(a5),d4
		if (!readblock(buf, block, IDNAME_FILESYSHEADER, md)) {
    25b0:	2f0b           	move.l a3,-(sp)
    25b2:	2f3c 4653 4844 	move.l #1179863108,-(sp)
    25b8:	2f00           	move.l d0,-(sp)
    25ba:	2f0a           	move.l a2,-(sp)
    25bc:	2043           	movea.l d3,a0
    25be:	4e90           	jsr (a0)
    25c0:	4fef 0010      	lea 16(sp),sp
    25c4:	4a40           	tst.w d0
    25c6:	6712           	beq.s 25da 25da _MountDrive+0x440
		if (fshb->fhb_DosType == dostype) {
    25c8:	b4aa 0020      	cmp.l 32(a2),d2
    25cc:	6700 0224      	beq.w 27f2 27f2 _MountDrive+0x658
		block = fshb->fhb_Next;
    25d0:	202a 0010      	move.l 16(a2),d0
		if (block == 0xffffffff) {
    25d4:	72ff           	moveq #-1,d1
    25d6:	b280           	cmp.l d0,d1
    25d8:	66d6           	bne.s 25b0 25b0 _MountDrive+0x416
    25da:	2b44 ffd0      	move.l d4,-48(a5)
    25de:	4dfa f9c4      	lea 1fa4 1fa4 _W_DeleteMsgPort+0x25a(pc),a6
		fse = FSHDProcess(NULL, dostype, 0, FALSE, md);
    25e2:	2f0b           	move.l a3,-(sp)
    25e4:	42a7           	clr.l -(sp)
    25e6:	42a7           	clr.l -(sp)
    25e8:	2f02           	move.l d2,-(sp)
    25ea:	42a7           	clr.l -(sp)
    25ec:	4e96           	jsr (a6)
    25ee:	2b40 ffac      	move.l d0,-84(a5)
    25f2:	4fef 0014      	lea 20(sp),sp
			pp->execname = md->devicename;
    25f6:	296b 0018 0004 	move.l 24(a3),4(a4)
			pp->unitnum = md->unitnum;
    25fc:	296b 0030 0008 	move.l 48(a3),8(a4)
			pp->dosname = part->pb_DriveName + 1;
    2602:	45eb 005d      	lea 93(a3),a2
    2606:	288a           	move.l a2,(a4)
			part->pb_DriveName[(*part->pb_DriveName) + 1] = 0;
    2608:	7000           	moveq #0,d0
    260a:	102b 005c      	move.b 92(a3),d0
    260e:	4233 085d      	clr.b (93,a3,d0.l)
			CheckAndFixDevName(md, part->pb_DriveName);
    2612:	41eb 005c      	lea 92(a3),a0
    2616:	2b48 ffa8      	move.l a0,-88(a5)
    261a:	2b53 ff98      	move.l (a3),-104(a5)
	Forbid();
    261e:	2c6d ff98      	movea.l -104(a5),a6
    2622:	4eae ff7c      	jsr -132(a6)
	struct BootNode *bn = (struct BootNode*)md->ExpansionBase->MountList.lh_Head;
    2626:	206b 0004      	movea.l 4(a3),a0
    262a:	2068 004a      	movea.l 74(a0),a0
    262e:	2c6d ffd0      	movea.l -48(a5),a6
	while (bn->bn_Node.ln_Succ) {
    2632:	2810           	move.l (a0),d4
    2634:	671e           	beq.s 2654 2654 _MountDrive+0x4ba
		const UBYTE *bname2 = BADDR(dn->dn_Name);
    2636:	2068 0010      	movea.l 16(a0),a0
    263a:	2068 0028      	movea.l 40(a0),a0
    263e:	2008           	move.l a0,d0
    2640:	e588           	lsl.l #2,d0
    2642:	2040           	movea.l d0,a0
	UBYTE len1 = *src1++;
    2644:	142b 005c      	move.b 92(a3),d2
	if (len1 != len2) {
    2648:	b410           	cmp.b (a0),d2
    264a:	6700 00c2      	beq.w 270e 270e _MountDrive+0x574
		if (c1 != c2) {
    264e:	2044           	movea.l d4,a0
	while (bn->bn_Node.ln_Succ) {
    2650:	2810           	move.l (a0),d4
    2652:	66e2           	bne.s 2636 2636 _MountDrive+0x49c
    2654:	2b4e ffd0      	move.l a6,-48(a5)
	Permit();
    2658:	2c6d ff98      	movea.l -104(a5),a6
    265c:	4eae ff76      	jsr -138(a6)
			struct DeviceNode *dn = MakeDosNode(pp);
    2660:	204c           	movea.l a4,a0
    2662:	2c6d ff90      	movea.l -112(a5),a6
    2666:	4eae ff70      	jsr -144(a6)
    266a:	2400           	move.l d0,d2
			if (dn) {
    266c:	6766           	beq.s 26d4 26d4 _MountDrive+0x53a
				if (fse) {
    266e:	4aad ffac      	tst.l -84(a5)
    2672:	672e           	beq.s 26a2 26a2 _MountDrive+0x508
					ULONG *dstPatch = &dn->dn_Type;
    2674:	2240           	movea.l d0,a1
					ULONG *srcPatch = &fse->fse_Type;
    2676:	307c 001a      	movea.w #26,a0
					ULONG *dstPatch = &dn->dn_Type;
    267a:	5889           	addq.l #4,a1
					ULONG *srcPatch = &fse->fse_Type;
    267c:	d1ed ffac      	adda.l -84(a5),a0
					ULONG patchFlags = fse->fse_PatchFlags;
    2680:	2c6d ffac      	movea.l -84(a5),a6
    2684:	202e 0016      	move.l 22(a6),d0
					while (patchFlags) {
    2688:	6718           	beq.s 26a2 26a2 _MountDrive+0x508
    268a:	222d ffd0      	move.l -48(a5),d1
						if (patchFlags & 1) {
    268e:	0800 0000      	btst #0,d0
    2692:	6702           	beq.s 2696 2696 _MountDrive+0x4fc
							*dstPatch = *srcPatch;
    2694:	2290           	move.l (a0),(a1)
						patchFlags >>= 1;
    2696:	e288           	lsr.l #1,d0
						srcPatch++;
    2698:	5888           	addq.l #4,a0
						dstPatch++;
    269a:	5889           	addq.l #4,a1
					while (patchFlags) {
    269c:	66f0           	bne.s 268e 268e _MountDrive+0x4f4
    269e:	2b41 ffd0      	move.l d1,-48(a5)
	struct ExpansionBase *ExpansionBase = md->ExpansionBase;
    26a2:	2c6b 0004      	movea.l 4(a3),a6
	struct DosLibrary *DOSBase = md->DOSBase;
    26a6:	2c2b 0008      	move.l 8(a3),d6
	LONG bootPri = (part->pb_Flags & PBFF_BOOTABLE) ? pp->de.de_BootPri : -128;
    26aa:	082b 0000 004f 	btst #0,79(a3)
    26b0:	6600 0138      	bne.w 27ea 27ea _MountDrive+0x650
    26b4:	7880           	moveq #-128,d4
	if (ExpansionBase->LibNode.lib_Version >= 37) {
    26b6:	0c6e 0024 0014 	cmpi.w #36,20(a6)
    26bc:	6300 00de      	bls.w 279c 279c _MountDrive+0x602
		if (!md->DOSBase && bootPri > -128) {
    26c0:	4a86           	tst.l d6
    26c2:	6700 0404      	beq.w 2ac8 2ac8 _MountDrive+0x92e
			AddDosNode(bootPri, ADNF_STARTPROC, dn);
    26c6:	2004           	move.l d4,d0
    26c8:	2042           	movea.l d2,a0
    26ca:	7201           	moveq #1,d1
    26cc:	4eae ff6a      	jsr -150(a6)
				md->ret++;
    26d0:	52ab 0034      	addq.l #1,52(a3)
			FreeMem(pp, sizeof(struct ParameterPacket));
    26d4:	2c47           	movea.l d7,a6
    26d6:	7060           	moveq #96,d0
    26d8:	224c           	movea.l a4,a1
    26da:	4eae ff2e      	jsr -210(a6)
		if (partblock == 0xffffffff) {
    26de:	70ff           	moveq #-1,d0
    26e0:	b085           	cmp.l d5,d0
    26e2:	6700 fe2e      	beq.w 2512 2512 _MountDrive+0x378
	struct ExecBase *SysBase = md->SysBase;
    26e6:	2e13           	move.l (a3),d7
	struct ExpansionBase *ExpansionBase = md->ExpansionBase;
    26e8:	2b6b 0004 ff90 	move.l 4(a3),-112(a5)
	if (!readblock(buf, block, IDNAME_PARTITION, md)) {
    26ee:	2f0b           	move.l a3,-(sp)
    26f0:	2f3c 5041 5254 	move.l #1346458196,-(sp)
    26f6:	2f05           	move.l d5,-(sp)
    26f8:	2f2d ffd0      	move.l -48(a5),-(sp)
    26fc:	2043           	movea.l d3,a0
    26fe:	4e90           	jsr (a0)
    2700:	4fef 0010      	lea 16(sp),sp
    2704:	4a40           	tst.w d0
    2706:	6700 fe0a      	beq.w 2512 2512 _MountDrive+0x378
    270a:	6000 fe4e      	bra.w 255a 255a _MountDrive+0x3c0
	for (UWORD i = 0; i < len1; i++) {
    270e:	4240           	clr.w d0
    2710:	1002           	move.b d2,d0
    2712:	4a40           	tst.w d0
    2714:	6700 033a      	beq.w 2a50 2a50 _MountDrive+0x8b6
	UBYTE len2 = *src2++;
    2718:	5288           	addq.l #1,a0
    271a:	224a           	movea.l a2,a1
    271c:	220a           	move.l a2,d1
    271e:	4681           	not.l d1
    2720:	d041           	add.w d1,d0
    2722:	3c00           	move.w d0,d6
    2724:	dc4a           	add.w a2,d6
		UBYTE c1 = *src1++;
    2726:	1019           	move.b (a1)+,d0
		UBYTE c2 = *src2++;
    2728:	1218           	move.b (a0)+,d1
		c |= 0x20;
    272a:	0000 0020      	ori.b #32,d0
    272e:	0001 0020      	ori.b #32,d1
		if (c1 != c2) {
    2732:	b200           	cmp.b d0,d1
    2734:	56ce fff0      	dbne d6,2726 2726 _MountDrive+0x58c
    2738:	6600 ff14      	bne.w 264e 264e _MountDrive+0x4b4
			if (len > 2 && name[len - 2] == '.' && name[len - 1] >= '0' && name[len - 1] < '9') {
    273c:	7000           	moveq #0,d0
    273e:	1002           	move.b d2,d0
    2740:	0c02 0002      	cmpi.b #2,d2
    2744:	6308           	bls.s 274e 274e _MountDrive+0x5b4
    2746:	0c32 002e 08fe 	cmpi.b #46,(-2,a2,d0.l)
    274c:	672a           	beq.s 2778 2778 _MountDrive+0x5de
				name[len++] = '.';
    274e:	15bc 002e 0800 	move.b #46,(0,a2,d0.l)
    2754:	1202           	move.b d2,d1
    2756:	5201           	addq.b #1,d1
				name[len++] = '1';
    2758:	1001           	move.b d1,d0
    275a:	15bc 0031 0800 	move.b #49,(0,a2,d0.l)
    2760:	5402           	addq.b #2,d2
				name[len] = 0;
    2762:	1002           	move.b d2,d0
    2764:	4232 0800      	clr.b (0,a2,d0.l)
				bname[0] += 2;
    2768:	542b 005c      	addq.b #2,92(a3)
			bn = (struct BootNode*)md->ExpansionBase->MountList.lh_Head;
    276c:	206b 0004      	movea.l 4(a3),a0
    2770:	2068 004a      	movea.l 74(a0),a0
    2774:	6000 febc      	bra.w 2632 2632 _MountDrive+0x498
			if (len > 2 && name[len - 2] == '.' && name[len - 1] >= '0' && name[len - 1] < '9') {
    2778:	206d ffa8      	movea.l -88(a5),a0
    277c:	d1c0           	adda.l d0,a0
    277e:	1810           	move.b (a0),d4
    2780:	1204           	move.b d4,d1
    2782:	0601 00d0      	addi.b #-48,d1
    2786:	0c01 0008      	cmpi.b #8,d1
    278a:	62c2           	bhi.s 274e 274e _MountDrive+0x5b4
				name[len - 1]++;
    278c:	5204           	addq.b #1,d4
    278e:	1084           	move.b d4,(a0)
			bn = (struct BootNode*)md->ExpansionBase->MountList.lh_Head;
    2790:	206b 0004      	movea.l 4(a3),a0
    2794:	2068 004a      	movea.l 74(a0),a0
    2798:	6000 fe98      	bra.w 2632 2632 _MountDrive+0x498
		if (!md->DOSBase && bootPri > -128) {
    279c:	4a86           	tst.l d6
    279e:	6700 0358      	beq.w 2af8 2af8 _MountDrive+0x95e
			AddDosNode(bootPri, 0, dn);
    27a2:	2004           	move.l d4,d0
    27a4:	2042           	movea.l d2,a0
    27a6:	7200           	moveq #0,d1
    27a8:	4eae ff6a      	jsr -150(a6)
			if (md->DOSBase) {
    27ac:	4aab 0008      	tst.l 8(a3)
    27b0:	6700 ff1e      	beq.w 26d0 26d0 _MountDrive+0x536
    27b4:	204a           	movea.l a2,a0
    27b6:	222d ffd0      	move.l -48(a5),d1
	while(*s++) {}return ~(string-s);
    27ba:	1018           	move.b (a0)+,d0
    27bc:	66fc           	bne.s 27ba 27ba _MountDrive+0x620
    27be:	2b41 ffd0      	move.l d1,-48(a5)
    27c2:	220a           	move.l a2,d1
    27c4:	9288           	sub.l a0,d1
    27c6:	4681           	not.l d1
				name[len++] = ':';
    27c8:	7400           	moveq #0,d2
    27ca:	3401           	move.w d1,d2
    27cc:	15bc 003a 2800 	move.b #58,(0,a2,d2.l)
    27d2:	5241           	addq.w #1,d1
				name[len] = 0;
    27d4:	3401           	move.w d1,d2
    27d6:	1580 2800      	move.b d0,(0,a2,d2.l)
				void *mp = DeviceProc(name);
    27da:	220a           	move.l a2,d1
    27dc:	2c46           	movea.l d6,a6
    27de:	4eae ff52      	jsr -174(a6)
				md->ret++;
    27e2:	52ab 0034      	addq.l #1,52(a3)
    27e6:	6000 feec      	bra.w 26d4 26d4 _MountDrive+0x53a
	LONG bootPri = (part->pb_Flags & PBFF_BOOTABLE) ? pp->de.de_BootPri : -128;
    27ea:	282c 004c      	move.l 76(a4),d4
    27ee:	6000 fec6      	bra.w 26b6 26b6 _MountDrive+0x51c
    27f2:	2b44 ffd0      	move.l d4,-48(a5)
			fse = FSHDProcess(fshb, dostype, fshb->fhb_Version, TRUE, md);
    27f6:	2f0b           	move.l a3,-(sp)
    27f8:	4878 0001      	pea 1 1 __start+0x1
    27fc:	2f2a 0024      	move.l 36(a2),-(sp)
    2800:	2f02           	move.l d2,-(sp)
    2802:	2f0a           	move.l a2,-(sp)
    2804:	4dfa f79e      	lea 1fa4 1fa4 _W_DeleteMsgPort+0x25a(pc),a6
    2808:	4e96           	jsr (a6)
    280a:	2b40 ffac      	move.l d0,-84(a5)
			if (fse) {
    280e:	4fef 0014      	lea 20(sp),sp
    2812:	6700 fdce      	beq.w 25e2 25e2 _MountDrive+0x448
				md->lsegblock = fshb->fhb_SegListBlocks;
    2816:	276a 0048 001c 	move.l 72(a2),28(a3)
				md->lsegbuf = (struct LoadSegBlock*)(buf + md->blocksize);
    281c:	d5eb 183c      	adda.l 6204(a3),a2
    2820:	274a 0028      	move.l a2,40(a3)
				md->lseglongs = 0;
    2824:	42ab 0020      	clr.l 32(a3)
	struct ExecBase *SysBase = md->SysBase;
    2828:	2b53 ffc4      	move.l (a3),-60(a5)
	if (md->lseghasword) {
    282c:	4a6b 002e      	tst.w 46(a3)
    2830:	6600 0256      	bne.w 2a88 2a88 _MountDrive+0x8ee
		v = lseg_read_longs(md, 1, data);
    2834:	486d ffd8      	pea -40(a5)
    2838:	4878 0001      	pea 1 1 __start+0x1
    283c:	2f0b           	move.l a3,-(sp)
    283e:	283c 0000 1e4e 	move.l #7758,d4
    2844:	2c44           	movea.l d4,a6
    2846:	4e96           	jsr (a6)
    2848:	4fef 000c      	lea 12(sp),sp
	if (!lseg_read_long(md, &data)) {
    284c:	4a40           	tst.w d0
    284e:	6700 0368      	beq.w 2bb8 2bb8 _MountDrive+0xa1e
	if (data != HUNK_HEADER) {
    2852:	0cad 0000 03f3 	cmpi.l #1011,-40(a5)
    2858:	ffd8 
    285a:	6600 035c      	bne.w 2bb8 2bb8 _MountDrive+0xa1e
	if (md->lseghasword) {
    285e:	4a6b 002e      	tst.w 46(a3)
    2862:	6600 0414      	bne.w 2c78 2c78 _MountDrive+0xade
		v = lseg_read_longs(md, 1, data);
    2866:	45ed ffdc      	lea -36(a5),a2
    286a:	2f0a           	move.l a2,-(sp)
    286c:	4878 0001      	pea 1 1 __start+0x1
    2870:	2f0b           	move.l a3,-(sp)
    2872:	2244           	movea.l d4,a1
    2874:	4e91           	jsr (a1)
	if (md->lseghasword) {
    2876:	4fef 000c      	lea 12(sp),sp
    287a:	4a6b 002e      	tst.w 46(a3)
    287e:	6600 09b0      	bne.w 3230 3230 _MountDrive+0x1096
		v = lseg_read_longs(md, 1, data);
    2882:	2f0a           	move.l a2,-(sp)
    2884:	4878 0001      	pea 1 1 __start+0x1
    2888:	2f0b           	move.l a3,-(sp)
    288a:	2044           	movea.l d4,a0
    288c:	4e90           	jsr (a0)
    288e:	302b 002e      	move.w 46(a3),d0
	firstHunk = lastHunk = -1;
    2892:	72ff           	moveq #-1,d1
    2894:	2b41 ffe0      	move.l d1,-32(a5)
    2898:	2b41 ffdc      	move.l d1,-36(a5)
	if (md->lseghasword) {
    289c:	4fef 000c      	lea 12(sp),sp
    28a0:	4a40           	tst.w d0
    28a2:	6600 0568      	bne.w 2e0c 2e0c _MountDrive+0xc72
		v = lseg_read_longs(md, 1, data);
    28a6:	2f0a           	move.l a2,-(sp)
    28a8:	4878 0001      	pea 1 1 __start+0x1
    28ac:	2f0b           	move.l a3,-(sp)
    28ae:	2244           	movea.l d4,a1
    28b0:	4e91           	jsr (a1)
	if (md->lseghasword) {
    28b2:	4fef 000c      	lea 12(sp),sp
    28b6:	4a6b 002e      	tst.w 46(a3)
    28ba:	6600 0938      	bne.w 31f4 31f4 _MountDrive+0x105a
		v = lseg_read_longs(md, 1, data);
    28be:	486d ffe0      	pea -32(a5)
    28c2:	4878 0001      	pea 1 1 __start+0x1
    28c6:	2f0b           	move.l a3,-(sp)
    28c8:	2044           	movea.l d4,a0
    28ca:	4e90           	jsr (a0)
    28cc:	4fef 000c      	lea 12(sp),sp
	if (firstHunk < 0 || lastHunk < 0 || firstHunk > lastHunk) {
    28d0:	222d ffdc      	move.l -36(a5),d1
    28d4:	6d00 02e2      	blt.w 2bb8 2bb8 _MountDrive+0xa1e
    28d8:	202d ffe0      	move.l -32(a5),d0
    28dc:	6d00 02da      	blt.w 2bb8 2bb8 _MountDrive+0xa1e
    28e0:	b081           	cmp.l d1,d0
    28e2:	6d00 02d4      	blt.w 2bb8 2bb8 _MountDrive+0xa1e
	totalHunks = lastHunk - firstHunk + 1;
    28e6:	9081           	sub.l d1,d0
    28e8:	5280           	addq.l #1,d0
    28ea:	2b40 ff98      	move.l d0,-104(a5)
	relocHunks = AllocMem(totalHunks * sizeof(struct RelocHunk), MEMF_CLEAR);
    28ee:	e788           	lsl.l #3,d0
    28f0:	2b40 ffd4      	move.l d0,-44(a5)
    28f4:	7201           	moveq #1,d1
    28f6:	4841           	swap d1
    28f8:	2c6d ffc4      	movea.l -60(a5),a6
    28fc:	4eae ff3a      	jsr -198(a6)
    2900:	2b40 ffbc      	move.l d0,-68(a5)
	if (!relocHunks) {
    2904:	6700 02b2      	beq.w 2bb8 2bb8 _MountDrive+0xa1e
	while (hunkCnt < totalHunks) {
    2908:	4aad ff98      	tst.l -104(a5)
    290c:	6f00 08c6      	ble.w 31d4 31d4 _MountDrive+0x103a
    2910:	42ad ffa8      	clr.l -88(a5)
    2914:	2c2d ffa8      	move.l -88(a5),d6
    2918:	342d ffaa      	move.w -86(a5),d2
    291c:	2046           	movea.l d6,a0
		v = lseg_read_longs(md, 1, data);
    291e:	2b47 ffb8      	move.l d7,-72(a5)
    2922:	2b4c ffc8      	move.l a4,-56(a5)
    2926:	2e00           	move.l d0,d7
    2928:	2b45 ffcc      	move.l d5,-52(a5)
    292c:	2a2d ff98      	move.l -104(a5),d5
    2930:	2844           	movea.l d4,a4
		struct RelocHunk *rh = &relocHunks[hunkCnt];
    2932:	2008           	move.l a0,d0
    2934:	e788           	lsl.l #3,d0
    2936:	2440           	movea.l d0,a2
		ULONG memoryFlags = MEMF_PUBLIC;
    2938:	7001           	moveq #1,d0
		struct RelocHunk *rh = &relocHunks[hunkCnt];
    293a:	d5c7           	adda.l d7,a2
		ULONG memoryFlags = MEMF_PUBLIC;
    293c:	2b40 fff0      	move.l d0,-16(a5)
	if (md->lseghasword) {
    2940:	4a6b 002e      	tst.w 46(a3)
    2944:	6600 0210      	bne.w 2b56 2b56 _MountDrive+0x9bc
		v = lseg_read_longs(md, 1, data);
    2948:	486d ffec      	pea -20(a5)
    294c:	4878 0001      	pea 1 1 __start+0x1
    2950:	2f0b           	move.l a3,-(sp)
    2952:	4e94           	jsr (a4)
    2954:	4fef 000c      	lea 12(sp),sp
		if (!lseg_read_long(md, &hunkHeadSize)) {
    2958:	4a40           	tst.w d0
    295a:	6700 03e4      	beq.w 2d40 2d40 _MountDrive+0xba6
		if ((hunkHeadSize & (HUNKF_CHIP | HUNKF_FAST)) == (HUNKF_CHIP | HUNKF_FAST)) {
    295e:	202d ffec      	move.l -20(a5),d0
    2962:	223c c000 0000 	move.l #-1073741824,d1
    2968:	c280           	and.l d0,d1
    296a:	0c81 c000 0000 	cmpi.l #-1073741824,d1
    2970:	6700 021c      	beq.w 2b8e 2b8e _MountDrive+0x9f4
		} else if (hunkHeadSize & HUNKF_CHIP) {
    2974:	0800 001e      	btst #30,d0
    2978:	6600 01ce      	bne.w 2b48 2b48 _MountDrive+0x9ae
    297c:	222d fff0      	move.l -16(a5),d1
		hunkHeadSize &= ~(HUNKF_CHIP | HUNKF_FAST);
    2980:	0280 3fff ffff 	andi.l #1073741823,d0
    2986:	2b40 ffec      	move.l d0,-20(a5)
		rh->hunkSize = hunkHeadSize;
    298a:	2480           	move.l d0,(a2)
		rh->hunkData = AllocMem((hunkHeadSize + 2) * sizeof(ULONG), memoryFlags | MEMF_CLEAR);
    298c:	5480           	addq.l #2,d0
    298e:	2c6d ffc4      	movea.l -60(a5),a6
    2992:	e588           	lsl.l #2,d0
    2994:	08c1 0010      	bset #16,d1
    2998:	4eae ff3a      	jsr -198(a6)
    299c:	2540 0004      	move.l d0,4(a2)
		if (!rh->hunkData) {
    29a0:	6700 039e      	beq.w 2d40 2d40 _MountDrive+0xba6
    29a4:	221a           	move.l (a2)+,d1
		rh->hunkData[0] = rh->hunkSize + 2;
    29a6:	2040           	movea.l d0,a0
    29a8:	5481           	addq.l #2,d1
    29aa:	20c1           	move.l d1,(a0)+
		rh->hunkData[1] = MKBADDR(prevChunk);
    29ac:	e486           	asr.l #2,d6
    29ae:	2086           	move.l d6,(a0)
		prevChunk = &rh->hunkData[1];
    29b0:	2c00           	move.l d0,d6
    29b2:	5886           	addq.l #4,d6
		rh->hunkData += 2;
    29b4:	5080           	addq.l #8,d0
    29b6:	2480           	move.l d0,(a2)
		if (!firstProcessedHunk) {
    29b8:	4aad ffa8      	tst.l -88(a5)
    29bc:	6700 0128      	beq.w 2ae6 2ae6 _MountDrive+0x94c
    29c0:	5242           	addq.w #1,d2
	while (hunkCnt < totalHunks) {
    29c2:	3042           	movea.w d2,a0
    29c4:	b1c5           	cmpa.l d5,a0
    29c6:	6d00 ff6a      	blt.w 2932 2932 _MountDrive+0x798
    29ca:	2e2d ffb8      	move.l -72(a5),d7
    29ce:	2a2d ffcc      	move.l -52(a5),d5
    29d2:	200c           	move.l a4,d0
	while (hunkCnt <= totalHunks) {
    29d4:	9dce           	suba.l a6,a6
    29d6:	286d ffc8      	movea.l -56(a5),a4
    29da:	3b4e ffb8      	move.w a6,-72(a5)
		v = lseg_read_longs(md, 1, &temp);
    29de:	240e           	move.l a6,d2
    29e0:	206d ffd0      	movea.l -48(a5),a0
    29e4:	2440           	movea.l d0,a2
	if (md->lseghasword) {
    29e6:	4a6b 002e      	tst.w 46(a3)
    29ea:	6600 06b8      	bne.w 30a4 30a4 _MountDrive+0xf0a
		v = lseg_read_longs(md, 1, data);
    29ee:	486d ffe4      	pea -28(a5)
    29f2:	4878 0001      	pea 1 1 __start+0x1
    29f6:	2f0b           	move.l a3,-(sp)
    29f8:	2b48 ff78      	move.l a0,-136(a5)
    29fc:	4e92           	jsr (a2)
    29fe:	4fef 000c      	lea 12(sp),sp
    2a02:	206d ff78      	movea.l -136(a5),a0
		if (!lseg_read_long(md, &hunkType)) {
    2a06:	4a40           	tst.w d0
    2a08:	6700 08b2      	beq.w 32bc 32bc _MountDrive+0x1122
		switch(hunkType)
    2a0c:	203c ffff fc17 	move.l #-1001,d0
    2a12:	d0ad ffe4      	add.l -28(a5),d0
    2a16:	7213           	moveq #19,d1
    2a18:	b280           	cmp.l d0,d1
    2a1a:	6500 0890      	bcs.w 32ac 32ac _MountDrive+0x1112
    2a1e:	d080           	add.l d0,d0
    2a20:	303b 0806      	move.w (2a28 2a28 _MountDrive+0x88e,pc,d0.l),d0
    2a24:	4efb 0002      	jmp (2a28 2a28 _MountDrive+0x88e,pc,d0.w)
    2a28:	0576 0576 0576 	bchg d2,([91620418,a6],2180)
    2a2e:	0442 0884 
    2a32:	0884 0884      	bclr #-124,d4
    2a36:	0884 0884      	bclr #-124,d4
    2a3a:	0420 0884      	subi.b #-124,-(a0)
    2a3e:	0884 0884      	bclr #-124,d4
    2a42:	0884 0884      	bclr #-124,d4
    2a46:	0884 0884      	bclr #-124,d4
    2a4a:	0884 0884      	bclr #-124,d4
    2a4e:	0442 1400      	subi.w #5120,d2
	for (UWORD i = 0; i < len1; i++) {
    2a52:	7000           	moveq #0,d0
				name[len++] = '.';
    2a54:	15bc 002e 0800 	move.b #46,(0,a2,d0.l)
    2a5a:	1202           	move.b d2,d1
    2a5c:	5201           	addq.b #1,d1
				name[len++] = '1';
    2a5e:	1001           	move.b d1,d0
    2a60:	15bc 0031 0800 	move.b #49,(0,a2,d0.l)
    2a66:	5402           	addq.b #2,d2
				name[len] = 0;
    2a68:	1002           	move.b d2,d0
    2a6a:	4232 0800      	clr.b (0,a2,d0.l)
				bname[0] += 2;
    2a6e:	542b 005c      	addq.b #2,92(a3)
    2a72:	6000 fcf8      	bra.w 276c 276c _MountDrive+0x5d2
	LONG ret = -1;
    2a76:	74ff           	moveq #-1,d2
		CloseLibrary(&ExpansionBase->LibNode);
    2a78:	2c6d ff8c      	movea.l -116(a5),a6
    2a7c:	226d ffb0      	movea.l -80(a5),a1
    2a80:	4eae fe62      	jsr -414(a6)
    2a84:	6000 f8c4      	bra.w 234a 234a _MountDrive+0x1b0
		v = lseg_read_longs(md, 1, &temp);
    2a88:	486d fff4      	pea -12(a5)
    2a8c:	4878 0001      	pea 1 1 __start+0x1
    2a90:	2f0b           	move.l a3,-(sp)
    2a92:	283c 0000 1e4e 	move.l #7758,d4
    2a98:	2244           	movea.l d4,a1
    2a9a:	4e91           	jsr (a1)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    2a9c:	242d fff4      	move.l -12(a5),d2
    2aa0:	7200           	moveq #0,d1
    2aa2:	322b 002c      	move.w 44(a3),d1
    2aa6:	4841           	swap d1
    2aa8:	4241           	clr.w d1
    2aaa:	2c02           	move.l d2,d6
    2aac:	4246           	clr.w d6
    2aae:	4846           	swap d6
    2ab0:	8286           	or.l d6,d1
    2ab2:	2b41 ffd8      	move.l d1,-40(a5)
		md->lsegwordbuf = (UWORD)temp;
    2ab6:	3742 002c      	move.w d2,44(a3)
		md->lseghasword = TRUE;
    2aba:	377c 0001 002e 	move.w #1,46(a3)
    2ac0:	4fef 000c      	lea 12(sp),sp
    2ac4:	6000 fd86      	bra.w 284c 284c _MountDrive+0x6b2
		if (!md->DOSBase && bootPri > -128) {
    2ac8:	7081           	moveq #-127,d0
    2aca:	b084           	cmp.l d4,d0
    2acc:	6e00 fbf8      	bgt.w 26c6 26c6 _MountDrive+0x52c
			AddBootNode(bootPri, ADNF_STARTPROC, dn, md->configDev);
    2ad0:	2004           	move.l d4,d0
    2ad2:	7201           	moveq #1,d1
    2ad4:	226b 0010      	movea.l 16(a3),a1
    2ad8:	2042           	movea.l d2,a0
    2ada:	4eae ffdc      	jsr -36(a6)
				md->ret++;
    2ade:	52ab 0034      	addq.l #1,52(a3)
    2ae2:	6000 fbf0      	bra.w 26d4 26d4 _MountDrive+0x53a
			firstProcessedHunk = (APTR)(rh->hunkData - 1);
    2ae6:	2b46 ffa8      	move.l d6,-88(a5)
    2aea:	5242           	addq.w #1,d2
	while (hunkCnt < totalHunks) {
    2aec:	3042           	movea.w d2,a0
    2aee:	b1c5           	cmpa.l d5,a0
    2af0:	6d00 fe40      	blt.w 2932 2932 _MountDrive+0x798
    2af4:	6000 fed4      	bra.w 29ca 29ca _MountDrive+0x830
		if (!md->DOSBase && bootPri > -128) {
    2af8:	7081           	moveq #-127,d0
    2afa:	b084           	cmp.l d4,d0
    2afc:	6e00 fca4      	bgt.w 27a2 27a2 _MountDrive+0x608
			struct BootNode *bn = AllocMem(sizeof(struct BootNode), MEMF_CLEAR | MEMF_PUBLIC);
    2b00:	2c53           	movea.l (a3),a6
    2b02:	7014           	moveq #20,d0
    2b04:	223c 0001 0001 	move.l #65537,d1
    2b0a:	4eae ff3a      	jsr -198(a6)
    2b0e:	2440           	movea.l d0,a2
			if (bn) {
    2b10:	4a80           	tst.l d0
    2b12:	6700 fbbc      	beq.w 26d0 26d0 _MountDrive+0x536
				bn->bn_Node.ln_Type = NT_BOOTNODE;
    2b16:	157c 0010 0008 	move.b #16,8(a2)
				bn->bn_Node.ln_Pri = (BYTE)bootPri;
    2b1c:	1544 0009      	move.b d4,9(a2)
				bn->bn_Node.ln_Name = (UBYTE*)md->configDev;
    2b20:	256b 0010 000a 	move.l 16(a3),10(a2)
				bn->bn_DeviceNode = dn;
    2b26:	2542 0010      	move.l d2,16(a2)
				Forbid();
    2b2a:	4eae ff7c      	jsr -132(a6)
				Enqueue(&md->ExpansionBase->MountList, &bn->bn_Node);
    2b2e:	307c 004a      	movea.w #74,a0
    2b32:	224a           	movea.l a2,a1
    2b34:	d1eb 0004      	adda.l 4(a3),a0
    2b38:	4eae fef2      	jsr -270(a6)
				Permit();
    2b3c:	4eae ff76      	jsr -138(a6)
				md->ret++;
    2b40:	52ab 0034      	addq.l #1,52(a3)
    2b44:	6000 fb8e      	bra.w 26d4 26d4 _MountDrive+0x53a
			memoryFlags |= MEMF_CHIP;
    2b48:	7202           	moveq #2,d1
    2b4a:	82ad fff0      	or.l -16(a5),d1
    2b4e:	2b41 fff0      	move.l d1,-16(a5)
    2b52:	6000 fe2c      	bra.w 2980 2980 _MountDrive+0x7e6
		v = lseg_read_longs(md, 1, &temp);
    2b56:	486d fff4      	pea -12(a5)
    2b5a:	4878 0001      	pea 1 1 __start+0x1
    2b5e:	2f0b           	move.l a3,-(sp)
    2b60:	4e94           	jsr (a4)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    2b62:	206d fff4      	movea.l -12(a5),a0
    2b66:	7200           	moveq #0,d1
    2b68:	322b 002c      	move.w 44(a3),d1
    2b6c:	4841           	swap d1
    2b6e:	4241           	clr.w d1
    2b70:	2808           	move.l a0,d4
    2b72:	4244           	clr.w d4
    2b74:	4844           	swap d4
    2b76:	8881           	or.l d1,d4
    2b78:	2b44 ffec      	move.l d4,-20(a5)
		md->lsegwordbuf = (UWORD)temp;
    2b7c:	3748 002c      	move.w a0,44(a3)
		md->lseghasword = TRUE;
    2b80:	377c 0001 002e 	move.w #1,46(a3)
    2b86:	4fef 000c      	lea 12(sp),sp
    2b8a:	6000 fdcc      	bra.w 2958 2958 _MountDrive+0x7be
	if (md->lseghasword) {
    2b8e:	4a6b 002e      	tst.w 46(a3)
    2b92:	6600 00a8      	bne.w 2c3c 2c3c _MountDrive+0xaa2
		v = lseg_read_longs(md, 1, data);
    2b96:	486d fff0      	pea -16(a5)
    2b9a:	4878 0001      	pea 1 1 __start+0x1
    2b9e:	2f0b           	move.l a3,-(sp)
    2ba0:	4e94           	jsr (a4)
    2ba2:	4fef 000c      	lea 12(sp),sp
			if (!lseg_read_long(md, &memoryFlags)) {
    2ba6:	4a40           	tst.w d0
    2ba8:	6700 0196      	beq.w 2d40 2d40 _MountDrive+0xba6
    2bac:	202d ffec      	move.l -20(a5),d0
    2bb0:	222d fff0      	move.l -16(a5),d1
    2bb4:	6000 fdca      	bra.w 2980 2980 _MountDrive+0x7e6
				fse->fse_SegList = MKBADDR(seg);
    2bb8:	206d ffac      	movea.l -84(a5),a0
    2bbc:	42a8 0036      	clr.l 54(a0)
    2bc0:	2813           	move.l (a3),d4
		FreeMem(fse, sizeof(struct FileSysEntry));
    2bc2:	2c44           	movea.l d4,a6
    2bc4:	703e           	moveq #62,d0
    2bc6:	2248           	movea.l a0,a1
    2bc8:	4eae ff2e      	jsr -210(a6)
			pp->execname = md->devicename;
    2bcc:	296b 0018 0004 	move.l 24(a3),4(a4)
			pp->unitnum = md->unitnum;
    2bd2:	296b 0030 0008 	move.l 48(a3),8(a4)
			pp->dosname = part->pb_DriveName + 1;
    2bd8:	45eb 005d      	lea 93(a3),a2
    2bdc:	288a           	move.l a2,(a4)
			part->pb_DriveName[(*part->pb_DriveName) + 1] = 0;
    2bde:	7000           	moveq #0,d0
    2be0:	102b 005c      	move.b 92(a3),d0
    2be4:	4233 085d      	clr.b (93,a3,d0.l)
			CheckAndFixDevName(md, part->pb_DriveName);
    2be8:	41eb 005c      	lea 92(a3),a0
    2bec:	2b48 ffa8      	move.l a0,-88(a5)
    2bf0:	2b53 ff98      	move.l (a3),-104(a5)
	Forbid();
    2bf4:	2c6d ff98      	movea.l -104(a5),a6
    2bf8:	4eae ff7c      	jsr -132(a6)
	struct BootNode *bn = (struct BootNode*)md->ExpansionBase->MountList.lh_Head;
    2bfc:	206b 0004      	movea.l 4(a3),a0
    2c00:	2068 004a      	movea.l 74(a0),a0
    2c04:	2c6d ffd0      	movea.l -48(a5),a6
    2c08:	6000 fa28      	bra.w 2632 2632 _MountDrive+0x498
	LONG ret = -1;
    2c0c:	74ff           	moveq #-1,d2
		FreeSignal(port->mp_SigBit);
    2c0e:	2c6d ff8c      	movea.l -116(a5),a6
    2c12:	206d ff9c      	movea.l -100(a5),a0
    2c16:	1028 000f      	move.b 15(a0),d0
    2c1a:	4eae feb0      	jsr -336(a6)
		FreeMem(port, sizeof(struct MsgPort));
    2c1e:	7022           	moveq #34,d0
    2c20:	226d ff9c      	movea.l -100(a5),a1
    2c24:	4eae ff2e      	jsr -210(a6)
    2c28:	6000 f6fc      	bra.w 2326 2326 _MountDrive+0x18c
		FreeMem(ret, sizeof(struct MsgPort));
    2c2c:	7022           	moveq #34,d0
    2c2e:	226d ff9c      	movea.l -100(a5),a1
    2c32:	4eae ff2e      	jsr -210(a6)
	LONG ret = -1;
    2c36:	74ff           	moveq #-1,d2
    2c38:	6000 f6ec      	bra.w 2326 2326 _MountDrive+0x18c
		v = lseg_read_longs(md, 1, &temp);
    2c3c:	486d fff4      	pea -12(a5)
    2c40:	4878 0001      	pea 1 1 __start+0x1
    2c44:	2f0b           	move.l a3,-(sp)
    2c46:	4e94           	jsr (a4)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    2c48:	206d fff4      	movea.l -12(a5),a0
    2c4c:	7200           	moveq #0,d1
    2c4e:	322b 002c      	move.w 44(a3),d1
    2c52:	4841           	swap d1
    2c54:	4241           	clr.w d1
    2c56:	2241           	movea.l d1,a1
    2c58:	2208           	move.l a0,d1
    2c5a:	4241           	clr.w d1
    2c5c:	4841           	swap d1
    2c5e:	2809           	move.l a1,d4
    2c60:	8284           	or.l d4,d1
    2c62:	2b41 fff0      	move.l d1,-16(a5)
		md->lsegwordbuf = (UWORD)temp;
    2c66:	3748 002c      	move.w a0,44(a3)
		md->lseghasword = TRUE;
    2c6a:	377c 0001 002e 	move.w #1,46(a3)
    2c70:	4fef 000c      	lea 12(sp),sp
    2c74:	6000 ff30      	bra.w 2ba6 2ba6 _MountDrive+0xa0c
		v = lseg_read_longs(md, 1, &temp);
    2c78:	45ed fff4      	lea -12(a5),a2
    2c7c:	2f0a           	move.l a2,-(sp)
    2c7e:	4878 0001      	pea 1 1 __start+0x1
    2c82:	2f0b           	move.l a3,-(sp)
    2c84:	2044           	movea.l d4,a0
    2c86:	4e90           	jsr (a0)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    2c88:	222d fff4      	move.l -12(a5),d1
    2c8c:	7000           	moveq #0,d0
    2c8e:	302b 002c      	move.w 44(a3),d0
    2c92:	4840           	swap d0
    2c94:	4240           	clr.w d0
    2c96:	2401           	move.l d1,d2
    2c98:	4242           	clr.w d2
    2c9a:	4842           	swap d2
    2c9c:	8082           	or.l d2,d0
    2c9e:	2b40 ffdc      	move.l d0,-36(a5)
		md->lsegwordbuf = (UWORD)temp;
    2ca2:	3741 002c      	move.w d1,44(a3)
		md->lseghasword = TRUE;
    2ca6:	377c 0001 002e 	move.w #1,46(a3)
		v = lseg_read_longs(md, 1, &temp);
    2cac:	508f           	addq.l #8,sp
    2cae:	2e8a           	move.l a2,(sp)
    2cb0:	4878 0001      	pea 1 1 __start+0x1
    2cb4:	2f0b           	move.l a3,-(sp)
    2cb6:	2c44           	movea.l d4,a6
    2cb8:	4e96           	jsr (a6)
		md->lsegwordbuf = (UWORD)temp;
    2cba:	376d fff6 002c 	move.w -10(a5),44(a3)
		md->lseghasword = TRUE;
    2cc0:	377c 0001 002e 	move.w #1,46(a3)
	firstHunk = lastHunk = -1;
    2cc6:	70ff           	moveq #-1,d0
    2cc8:	2b40 ffe0      	move.l d0,-32(a5)
    2ccc:	2b40 ffdc      	move.l d0,-36(a5)
    2cd0:	4fef 000c      	lea 12(sp),sp
		v = lseg_read_longs(md, 1, &temp);
    2cd4:	2f0a           	move.l a2,-(sp)
    2cd6:	4878 0001      	pea 1 1 __start+0x1
    2cda:	2f0b           	move.l a3,-(sp)
    2cdc:	2044           	movea.l d4,a0
    2cde:	4e90           	jsr (a0)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    2ce0:	222d fff4      	move.l -12(a5),d1
    2ce4:	7000           	moveq #0,d0
    2ce6:	302b 002c      	move.w 44(a3),d0
    2cea:	4840           	swap d0
    2cec:	4240           	clr.w d0
    2cee:	2401           	move.l d1,d2
    2cf0:	4242           	clr.w d2
    2cf2:	4842           	swap d2
    2cf4:	8082           	or.l d2,d0
    2cf6:	2b40 ffdc      	move.l d0,-36(a5)
		md->lsegwordbuf = (UWORD)temp;
    2cfa:	3741 002c      	move.w d1,44(a3)
		md->lseghasword = TRUE;
    2cfe:	377c 0001 002e 	move.w #1,46(a3)
    2d04:	4fef 000c      	lea 12(sp),sp
		v = lseg_read_longs(md, 1, &temp);
    2d08:	2f0a           	move.l a2,-(sp)
    2d0a:	4878 0001      	pea 1 1 __start+0x1
    2d0e:	2f0b           	move.l a3,-(sp)
    2d10:	2c44           	movea.l d4,a6
    2d12:	4e96           	jsr (a6)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    2d14:	222d fff4      	move.l -12(a5),d1
    2d18:	7000           	moveq #0,d0
    2d1a:	302b 002c      	move.w 44(a3),d0
    2d1e:	4840           	swap d0
    2d20:	4240           	clr.w d0
    2d22:	2401           	move.l d1,d2
    2d24:	4242           	clr.w d2
    2d26:	4842           	swap d2
    2d28:	8082           	or.l d2,d0
    2d2a:	2b40 ffe0      	move.l d0,-32(a5)
		md->lsegwordbuf = (UWORD)temp;
    2d2e:	3741 002c      	move.w d1,44(a3)
		md->lseghasword = TRUE;
    2d32:	377c 0001 002e 	move.w #1,46(a3)
    2d38:	4fef 000c      	lea 12(sp),sp
    2d3c:	6000 fb92      	bra.w 28d0 28d0 _MountDrive+0x736
    2d40:	2e2d ffb8      	move.l -72(a5),d7
    2d44:	286d ffc8      	movea.l -56(a5),a4
    2d48:	2a2d ffcc      	move.l -52(a5),d5
				rh = &relocHunks[hunkCnt++];
    2d4c:	4242           	clr.w d2
    2d4e:	91c8           	suba.l a0,a0
    2d50:	282d ffd0      	move.l -48(a5),d4
			struct RelocHunk *rh = &relocHunks[hunkCnt];
    2d54:	2008           	move.l a0,d0
    2d56:	e788           	lsl.l #3,d0
    2d58:	2040           	movea.l d0,a0
    2d5a:	d1ed ffbc      	adda.l -68(a5),a0
			if (rh->hunkData) {
    2d5e:	2268 0004      	movea.l 4(a0),a1
    2d62:	2009           	move.l a1,d0
    2d64:	6710           	beq.s 2d76 2d76 _MountDrive+0xbdc
				FreeMem(rh->hunkData - 2, (rh->hunkSize + 2) * sizeof(ULONG));
    2d66:	2010           	move.l (a0),d0
    2d68:	5480           	addq.l #2,d0
    2d6a:	2c6d ffc4      	movea.l -60(a5),a6
    2d6e:	e588           	lsl.l #2,d0
    2d70:	5189           	subq.l #8,a1
    2d72:	4eae ff2e      	jsr -210(a6)
    2d76:	5242           	addq.w #1,d2
		while (hunkCnt < totalHunks) {
    2d78:	3042           	movea.w d2,a0
    2d7a:	b1ed ff98      	cmpa.l -104(a5),a0
    2d7e:	6dd4           	blt.s 2d54 2d54 _MountDrive+0xbba
    2d80:	2b44 ffd0      	move.l d4,-48(a5)
				rh = &relocHunks[hunkCnt++];
    2d84:	7400           	moveq #0,d2
	FreeMem(relocHunks, totalHunks * sizeof(struct RelocHunk));
    2d86:	2c6d ffc4      	movea.l -60(a5),a6
    2d8a:	226d ffbc      	movea.l -68(a5),a1
    2d8e:	202d ffd4      	move.l -44(a5),d0
    2d92:	4eae ff2e      	jsr -210(a6)
				fse->fse_SegList = MKBADDR(seg);
    2d96:	206d ffac      	movea.l -84(a5),a0
    2d9a:	2142 0036      	move.l d2,54(a0)
    2d9e:	2813           	move.l (a3),d4
	if (fse->fse_SegList) {
    2da0:	4a82           	tst.l d2
    2da2:	6700 fe1e      	beq.w 2bc2 2bc2 _MountDrive+0xa28
		Forbid();
    2da6:	2c44           	movea.l d4,a6
    2da8:	4eae ff7c      	jsr -132(a6)
		struct FileSysResource *fsr = OpenResource(FSRNAME);
    2dac:	43fa f1e2      	lea 1f90 1f90 _W_DeleteMsgPort+0x246(pc),a1
    2db0:	4eae fe0e      	jsr -498(a6)
		if (fsr) {
    2db4:	4a80           	tst.l d0
    2db6:	6700 034e      	beq.w 3106 3106 _MountDrive+0xf6c
			AddHead(&fsr->fsr_FileSysEntries, &fse->fse_Node);
    2dba:	307c 0012      	movea.w #18,a0
    2dbe:	226d ffac      	movea.l -84(a5),a1
    2dc2:	d1c0           	adda.l d0,a0
    2dc4:	4eae ff10      	jsr -240(a6)
		Permit();
    2dc8:	4eae ff76      	jsr -138(a6)
			pp->execname = md->devicename;
    2dcc:	296b 0018 0004 	move.l 24(a3),4(a4)
			pp->unitnum = md->unitnum;
    2dd2:	296b 0030 0008 	move.l 48(a3),8(a4)
			pp->dosname = part->pb_DriveName + 1;
    2dd8:	45eb 005d      	lea 93(a3),a2
    2ddc:	288a           	move.l a2,(a4)
			part->pb_DriveName[(*part->pb_DriveName) + 1] = 0;
    2dde:	7000           	moveq #0,d0
    2de0:	102b 005c      	move.b 92(a3),d0
    2de4:	4233 085d      	clr.b (93,a3,d0.l)
			CheckAndFixDevName(md, part->pb_DriveName);
    2de8:	41eb 005c      	lea 92(a3),a0
    2dec:	2b48 ffa8      	move.l a0,-88(a5)
    2df0:	2b53 ff98      	move.l (a3),-104(a5)
	Forbid();
    2df4:	2c6d ff98      	movea.l -104(a5),a6
    2df8:	4eae ff7c      	jsr -132(a6)
	struct BootNode *bn = (struct BootNode*)md->ExpansionBase->MountList.lh_Head;
    2dfc:	206b 0004      	movea.l 4(a3),a0
    2e00:	2068 004a      	movea.l 74(a0),a0
    2e04:	2c6d ffd0      	movea.l -48(a5),a6
    2e08:	6000 f828      	bra.w 2632 2632 _MountDrive+0x498
    2e0c:	45ed fff4      	lea -12(a5),a2
		v = lseg_read_longs(md, 1, &temp);
    2e10:	2f0a           	move.l a2,-(sp)
    2e12:	4878 0001      	pea 1 1 __start+0x1
    2e16:	2f0b           	move.l a3,-(sp)
    2e18:	2044           	movea.l d4,a0
    2e1a:	4e90           	jsr (a0)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    2e1c:	222d fff4      	move.l -12(a5),d1
    2e20:	7000           	moveq #0,d0
    2e22:	302b 002c      	move.w 44(a3),d0
    2e26:	4840           	swap d0
    2e28:	4240           	clr.w d0
    2e2a:	2401           	move.l d1,d2
    2e2c:	4242           	clr.w d2
    2e2e:	4842           	swap d2
    2e30:	8082           	or.l d2,d0
    2e32:	2b40 ffdc      	move.l d0,-36(a5)
		md->lsegwordbuf = (UWORD)temp;
    2e36:	3741 002c      	move.w d1,44(a3)
		md->lseghasword = TRUE;
    2e3a:	377c 0001 002e 	move.w #1,46(a3)
    2e40:	4fef 000c      	lea 12(sp),sp
    2e44:	6000 fec2      	bra.w 2d08 2d08 _MountDrive+0xb6e
			if (hunkCnt >= totalHunks) {
    2e48:	b4ad ff98      	cmp.l -104(a5),d2
    2e4c:	6c0e           	bge.s 2e5c 2e5c _MountDrive+0xcc2
    2e4e:	342d ffb8      	move.w -72(a5),d2
	while (hunkCnt <= totalHunks) {
    2e52:	48c2           	ext.l d2
    2e54:	b4ad ff98      	cmp.l -104(a5),d2
    2e58:	6f00 fb8c      	ble.w 29e6 29e6 _MountDrive+0x84c
    2e5c:	2b48 ffd0      	move.l a0,-48(a5)
    2e60:	242d ffa8      	move.l -88(a5),d2
    2e64:	e482           	asr.l #2,d2
    2e66:	6000 ff1e      	bra.w 2d86 2d86 _MountDrive+0xbec
				if (rh == NULL) {
    2e6a:	200e           	move.l a6,d0
    2e6c:	6700 043e      	beq.w 32ac 32ac _MountDrive+0x1112
		v = lseg_read_longs(md, 1, data);
    2e70:	7cf0           	moveq #-16,d6
    2e72:	dc8d           	add.l a5,d6
    2e74:	2248           	movea.l a0,a1
	if (md->lseghasword) {
    2e76:	4a6b 002e      	tst.w 46(a3)
    2e7a:	6600 02ce      	bne.w 314a 314a _MountDrive+0xfb0
		v = lseg_read_longs(md, 1, data);
    2e7e:	486d ffe8      	pea -24(a5)
    2e82:	4878 0001      	pea 1 1 __start+0x1
    2e86:	2f0b           	move.l a3,-(sp)
    2e88:	2b49 ff78      	move.l a1,-136(a5)
    2e8c:	4e92           	jsr (a2)
    2e8e:	4fef 000c      	lea 12(sp),sp
    2e92:	226d ff78      	movea.l -136(a5),a1
					if (!lseg_read_long(md, &relocCnt)) {
    2e96:	4a40           	tst.w d0
    2e98:	6700 01f2      	beq.w 308c 308c _MountDrive+0xef2
					if (!relocCnt) {
    2e9c:	4aad ffe8      	tst.l -24(a5)
    2ea0:	6700 02e8      	beq.w 318a 318a _MountDrive+0xff0
	if (md->lseghasword) {
    2ea4:	4a6b 002e      	tst.w 46(a3)
    2ea8:	6600 02ea      	bne.w 3194 3194 _MountDrive+0xffa
		v = lseg_read_longs(md, 1, data);
    2eac:	486d ffec      	pea -20(a5)
    2eb0:	4878 0001      	pea 1 1 __start+0x1
    2eb4:	2f0b           	move.l a3,-(sp)
    2eb6:	2b49 ff78      	move.l a1,-136(a5)
    2eba:	4e92           	jsr (a2)
    2ebc:	4fef 000c      	lea 12(sp),sp
    2ec0:	226d ff78      	movea.l -136(a5),a1
					if (!lseg_read_long(md, &relocHunk)) {
    2ec4:	4a40           	tst.w d0
    2ec6:	6700 01c4      	beq.w 308c 308c _MountDrive+0xef2
					relocHunk -= firstHunk;
    2eca:	202d ffec      	move.l -20(a5),d0
    2ece:	90ad ffdc      	sub.l -36(a5),d0
    2ed2:	2b40 ffec      	move.l d0,-20(a5)
					if (relocHunk >= totalHunks) {
    2ed6:	b0ad ff98      	cmp.l -104(a5),d0
    2eda:	6400 01b0      	bcc.w 308c 308c _MountDrive+0xef2
					struct RelocHunk *rhr = &relocHunks[relocHunk];
    2ede:	e788           	lsl.l #3,d0
    2ee0:	242d ffbc      	move.l -68(a5),d2
    2ee4:	d480           	add.l d0,d2
					while (relocCnt != 0) {
    2ee6:	4aad ffe8      	tst.l -24(a5)
    2eea:	678a           	beq.s 2e76 2e76 _MountDrive+0xcdc
    2eec:	2b49 ffd0      	move.l a1,-48(a5)
						if (hunkType == HUNK_RELOC32SHORT) {
    2ef0:	0cad 0000 03fc 	cmpi.l #1020,-28(a5)
    2ef6:	ffe4 
    2ef8:	6700 0140      	beq.w 303a 303a _MountDrive+0xea0
	if (md->lseghasword) {
    2efc:	4a6b 002e      	tst.w 46(a3)
    2f00:	6600 0152      	bne.w 3054 3054 _MountDrive+0xeba
		v = lseg_read_longs(md, 1, data);
    2f04:	2f06           	move.l d6,-(sp)
    2f06:	4878 0001      	pea 1 1 __start+0x1
    2f0a:	2f0b           	move.l a3,-(sp)
    2f0c:	4e92           	jsr (a2)
    2f0e:	4fef 000c      	lea 12(sp),sp
							if (!lseg_read_long(md, &relocOffset)) {
    2f12:	4a40           	tst.w d0
    2f14:	6700 017a      	beq.w 3090 3090 _MountDrive+0xef6
    2f18:	202d fff0      	move.l -16(a5),d0
						if (relocOffset > (rh->hunkSize - 1) * sizeof(ULONG)) {
    2f1c:	223c 3fff ffff 	move.l #1073741823,d1
    2f22:	d296           	add.l (a6),d1
    2f24:	e589           	lsl.l #2,d1
    2f26:	b081           	cmp.l d1,d0
    2f28:	6200 0166      	bhi.w 3090 3090 _MountDrive+0xef6
						UBYTE *hData = (UBYTE*)rh->hunkData + relocOffset;
    2f2c:	206e 0004      	movea.l 4(a6),a0
    2f30:	d1c0           	adda.l d0,a0
						if (relocOffset & 1) {
    2f32:	0800 0000      	btst #0,d0
    2f36:	6700 00e0      	beq.w 3018 3018 _MountDrive+0xe7e
							v += (ULONG)rhr->hunkData;
    2f3a:	7000           	moveq #0,d0
    2f3c:	1010           	move.b (a0),d0
    2f3e:	e148           	lsl.w #8,d0
    2f40:	4840           	swap d0
    2f42:	4240           	clr.w d0
    2f44:	2240           	movea.l d0,a1
    2f46:	7000           	moveq #0,d0
    2f48:	1028 0001      	move.b 1(a0),d0
    2f4c:	2200           	move.l d0,d1
    2f4e:	4841           	swap d1
    2f50:	4241           	clr.w d1
    2f52:	2009           	move.l a1,d0
    2f54:	8280           	or.l d0,d1
    2f56:	7000           	moveq #0,d0
    2f58:	1028 0002      	move.b 2(a0),d0
    2f5c:	e188           	lsl.l #8,d0
    2f5e:	8280           	or.l d0,d1
    2f60:	7000           	moveq #0,d0
    2f62:	1028 0003      	move.b 3(a0),d0
    2f66:	2242           	movea.l d2,a1
    2f68:	8081           	or.l d1,d0
    2f6a:	d0a9 0004      	add.l 4(a1),d0
							hData[0] = v >> 24;
    2f6e:	2200           	move.l d0,d1
    2f70:	4241           	clr.w d1
    2f72:	4841           	swap d1
    2f74:	e049           	lsr.w #8,d1
    2f76:	10c1           	move.b d1,(a0)+
							hData[1] = v >> 16;
    2f78:	2200           	move.l d0,d1
    2f7a:	4241           	clr.w d1
    2f7c:	4841           	swap d1
    2f7e:	10c1           	move.b d1,(a0)+
							hData[2] = v >>  8;
    2f80:	2200           	move.l d0,d1
    2f82:	e089           	lsr.l #8,d1
    2f84:	10c1           	move.b d1,(a0)+
    2f86:	1080           	move.b d0,(a0)
						relocCnt--;
    2f88:	202d ffe8      	move.l -24(a5),d0
    2f8c:	5380           	subq.l #1,d0
    2f8e:	2b40 ffe8      	move.l d0,-24(a5)
					while (relocCnt != 0) {
    2f92:	6600 ff5c      	bne.w 2ef0 2ef0 _MountDrive+0xd56
    2f96:	226d ffd0      	movea.l -48(a5),a1
    2f9a:	6000 feda      	bra.w 2e76 2e76 _MountDrive+0xcdc
				if (hunkCnt >= totalHunks) {
    2f9e:	b4ad ff98      	cmp.l -104(a5),d2
    2fa2:	6c00 0308      	bge.w 32ac 32ac _MountDrive+0x1112
				rh = &relocHunks[hunkCnt++];
    2fa6:	342d ffb8      	move.w -72(a5),d2
    2faa:	5242           	addq.w #1,d2
    2fac:	302d ffb8      	move.w -72(a5),d0
    2fb0:	48c0           	ext.l d0
    2fb2:	2c6d ffbc      	movea.l -68(a5),a6
    2fb6:	e788           	lsl.l #3,d0
    2fb8:	ddc0           	adda.l d0,a6
	if (md->lseghasword) {
    2fba:	4a6b 002e      	tst.w 46(a3)
    2fbe:	6600 02ac      	bne.w 326c 326c _MountDrive+0x10d2
		v = lseg_read_longs(md, 1, data);
    2fc2:	486d fff0      	pea -16(a5)
    2fc6:	4878 0001      	pea 1 1 __start+0x1
    2fca:	2f0b           	move.l a3,-(sp)
    2fcc:	2b48 ff78      	move.l a0,-136(a5)
    2fd0:	4e92           	jsr (a2)
    2fd2:	4fef 000c      	lea 12(sp),sp
    2fd6:	206d ff78      	movea.l -136(a5),a0
				if (!lseg_read_long(md, &hunkSize)) {
    2fda:	4a40           	tst.w d0
    2fdc:	6700 02ce      	beq.w 32ac 32ac _MountDrive+0x1112
				if (hunkSize > rh->hunkSize) {
    2fe0:	202d fff0      	move.l -16(a5),d0
    2fe4:	b096           	cmp.l (a6),d0
    2fe6:	6200 02c4      	bhi.w 32ac 32ac _MountDrive+0x1112
				if (hunkType != HUNK_BSS) {
    2fea:	0cad 0000 03eb 	cmpi.l #1003,-28(a5)
    2ff0:	ffe4 
    2ff2:	671c           	beq.s 3010 3010 _MountDrive+0xe76
					if (!lseg_read_longs(md, hunkSize, rh->hunkData)) {
    2ff4:	2f2e 0004      	move.l 4(a6),-(sp)
    2ff8:	2f00           	move.l d0,-(sp)
    2ffa:	2f0b           	move.l a3,-(sp)
    2ffc:	2b48 ff78      	move.l a0,-136(a5)
    3000:	4e92           	jsr (a2)
    3002:	4fef 000c      	lea 12(sp),sp
    3006:	206d ff78      	movea.l -136(a5),a0
    300a:	4a40           	tst.w d0
    300c:	6700 029e      	beq.w 32ac 32ac _MountDrive+0x1112
				rh = &relocHunks[hunkCnt++];
    3010:	3b42 ffb8      	move.w d2,-72(a5)
    3014:	6000 fe3c      	bra.w 2e52 2e52 _MountDrive+0xcb8
							*((ULONG*)hData) += (ULONG)rhr->hunkData;
    3018:	2242           	movea.l d2,a1
    301a:	2029 0004      	move.l 4(a1),d0
    301e:	d190           	add.l d0,(a0)
						relocCnt--;
    3020:	202d ffe8      	move.l -24(a5),d0
    3024:	5380           	subq.l #1,d0
    3026:	2b40 ffe8      	move.l d0,-24(a5)
					while (relocCnt != 0) {
    302a:	6700 ff6a      	beq.w 2f96 2f96 _MountDrive+0xdfc
						if (hunkType == HUNK_RELOC32SHORT) {
    302e:	0cad 0000 03fc 	cmpi.l #1020,-28(a5)
    3034:	ffe4 
    3036:	6600 fec4      	bne.w 2efc 2efc _MountDrive+0xd62
	if (md->lseghasword) {
    303a:	4a6b 002e      	tst.w 46(a3)
    303e:	6700 00da      	beq.w 311a 311a _MountDrive+0xf80
		*data = md->lsegwordbuf;
    3042:	7000           	moveq #0,d0
    3044:	302b 002c      	move.w 44(a3),d0
    3048:	2b40 fff0      	move.l d0,-16(a5)
		md->lseghasword = FALSE;
    304c:	426b 002e      	clr.w 46(a3)
    3050:	6000 feca      	bra.w 2f1c 2f1c _MountDrive+0xd82
		v = lseg_read_longs(md, 1, &temp);
    3054:	486d fff4      	pea -12(a5)
    3058:	4878 0001      	pea 1 1 __start+0x1
    305c:	2f0b           	move.l a3,-(sp)
    305e:	4e92           	jsr (a2)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    3060:	206d fff4      	movea.l -12(a5),a0
    3064:	7200           	moveq #0,d1
    3066:	322b 002c      	move.w 44(a3),d1
    306a:	4841           	swap d1
    306c:	4241           	clr.w d1
    306e:	2808           	move.l a0,d4
    3070:	4244           	clr.w d4
    3072:	4844           	swap d4
    3074:	8881           	or.l d1,d4
    3076:	2b44 fff0      	move.l d4,-16(a5)
		md->lsegwordbuf = (UWORD)temp;
    307a:	3748 002c      	move.w a0,44(a3)
		md->lseghasword = TRUE;
    307e:	377c 0001 002e 	move.w #1,46(a3)
    3084:	4fef 000c      	lea 12(sp),sp
    3088:	6000 fe88      	bra.w 2f12 2f12 _MountDrive+0xd78
    308c:	2b49 ffd0      	move.l a1,-48(a5)
		while (hunkCnt < totalHunks) {
    3090:	4aad ff98      	tst.l -104(a5)
    3094:	6f00 fcee      	ble.w 2d84 2d84 _MountDrive+0xbea
				rh = &relocHunks[hunkCnt++];
    3098:	4242           	clr.w d2
    309a:	91c8           	suba.l a0,a0
    309c:	282d ffd0      	move.l -48(a5),d4
    30a0:	6000 fcb2      	bra.w 2d54 2d54 _MountDrive+0xbba
		v = lseg_read_longs(md, 1, &temp);
    30a4:	486d fff4      	pea -12(a5)
    30a8:	4878 0001      	pea 1 1 __start+0x1
    30ac:	2f0b           	move.l a3,-(sp)
    30ae:	2b48 ff78      	move.l a0,-136(a5)
    30b2:	4e92           	jsr (a2)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    30b4:	282d fff4      	move.l -12(a5),d4
    30b8:	7200           	moveq #0,d1
    30ba:	322b 002c      	move.w 44(a3),d1
    30be:	4841           	swap d1
    30c0:	4241           	clr.w d1
    30c2:	2c04           	move.l d4,d6
    30c4:	4246           	clr.w d6
    30c6:	4846           	swap d6
    30c8:	8286           	or.l d6,d1
    30ca:	2b41 ffe4      	move.l d1,-28(a5)
		md->lsegwordbuf = (UWORD)temp;
    30ce:	3744 002c      	move.w d4,44(a3)
		md->lseghasword = TRUE;
    30d2:	377c 0001 002e 	move.w #1,46(a3)
    30d8:	4fef 000c      	lea 12(sp),sp
    30dc:	206d ff78      	movea.l -136(a5),a0
    30e0:	6000 f924      	bra.w 2a06 2a06 _MountDrive+0x86c
	LONG ret = -1;
    30e4:	74ff           	moveq #-1,d2
    30e6:	6000 f23e      	bra.w 2326 2326 _MountDrive+0x18c
    30ea:	4dfa eeb8      	lea 1fa4 1fa4 _W_DeleteMsgPort+0x25a(pc),a6
		fse = FSHDProcess(NULL, dostype, 0, FALSE, md);
    30ee:	2f0b           	move.l a3,-(sp)
    30f0:	42a7           	clr.l -(sp)
    30f2:	42a7           	clr.l -(sp)
    30f4:	2f02           	move.l d2,-(sp)
    30f6:	42a7           	clr.l -(sp)
    30f8:	4e96           	jsr (a6)
    30fa:	2b40 ffac      	move.l d0,-84(a5)
    30fe:	4fef 0014      	lea 20(sp),sp
    3102:	6000 f4f2      	bra.w 25f6 25f6 _MountDrive+0x45c
		Permit();
    3106:	4eae ff76      	jsr -138(a6)
		FreeMem(fse, sizeof(struct FileSysEntry));
    310a:	2c44           	movea.l d4,a6
    310c:	703e           	moveq #62,d0
    310e:	226d ffac      	movea.l -84(a5),a1
    3112:	4eae ff2e      	jsr -210(a6)
    3116:	6000 fab4      	bra.w 2bcc 2bcc _MountDrive+0xa32
	BOOL v = lseg_read_longs(md, 1, &temp);
    311a:	486d fff4      	pea -12(a5)
    311e:	4878 0001      	pea 1 1 __start+0x1
    3122:	2f0b           	move.l a3,-(sp)
    3124:	4e92           	jsr (a2)
	if (v) {
    3126:	4fef 000c      	lea 12(sp),sp
    312a:	4a40           	tst.w d0
    312c:	6700 ff62      	beq.w 3090 3090 _MountDrive+0xef6
		md->lseghasword = TRUE;
    3130:	377c 0001 002e 	move.w #1,46(a3)
		md->lsegwordbuf = (UWORD)temp;
    3136:	202d fff4      	move.l -12(a5),d0
    313a:	3740 002c      	move.w d0,44(a3)
		*data = temp >> 16;
    313e:	4240           	clr.w d0
    3140:	4840           	swap d0
    3142:	2b40 fff0      	move.l d0,-16(a5)
    3146:	6000 fdd4      	bra.w 2f1c 2f1c _MountDrive+0xd82
		v = lseg_read_longs(md, 1, &temp);
    314a:	486d fff4      	pea -12(a5)
    314e:	4878 0001      	pea 1 1 __start+0x1
    3152:	2f0b           	move.l a3,-(sp)
    3154:	2b49 ff78      	move.l a1,-136(a5)
    3158:	4e92           	jsr (a2)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    315a:	242d fff4      	move.l -12(a5),d2
    315e:	7200           	moveq #0,d1
    3160:	322b 002c      	move.w 44(a3),d1
    3164:	4841           	swap d1
    3166:	4241           	clr.w d1
    3168:	2802           	move.l d2,d4
    316a:	4244           	clr.w d4
    316c:	4844           	swap d4
    316e:	8881           	or.l d1,d4
    3170:	2b44 ffe8      	move.l d4,-24(a5)
		md->lsegwordbuf = (UWORD)temp;
    3174:	3742 002c      	move.w d2,44(a3)
		md->lseghasword = TRUE;
    3178:	377c 0001 002e 	move.w #1,46(a3)
    317e:	4fef 000c      	lea 12(sp),sp
    3182:	226d ff78      	movea.l -136(a5),a1
    3186:	6000 fd0e      	bra.w 2e96 2e96 _MountDrive+0xcfc
    318a:	2049           	movea.l a1,a0
    318c:	342d ffb8      	move.w -72(a5),d2
    3190:	6000 fcc0      	bra.w 2e52 2e52 _MountDrive+0xcb8
		v = lseg_read_longs(md, 1, &temp);
    3194:	486d fff4      	pea -12(a5)
    3198:	4878 0001      	pea 1 1 __start+0x1
    319c:	2f0b           	move.l a3,-(sp)
    319e:	2b49 ff78      	move.l a1,-136(a5)
    31a2:	4e92           	jsr (a2)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    31a4:	242d fff4      	move.l -12(a5),d2
    31a8:	7200           	moveq #0,d1
    31aa:	322b 002c      	move.w 44(a3),d1
    31ae:	4841           	swap d1
    31b0:	4241           	clr.w d1
    31b2:	2802           	move.l d2,d4
    31b4:	4244           	clr.w d4
    31b6:	4844           	swap d4
    31b8:	8881           	or.l d1,d4
    31ba:	2b44 ffec      	move.l d4,-20(a5)
		md->lsegwordbuf = (UWORD)temp;
    31be:	3742 002c      	move.w d2,44(a3)
		md->lseghasword = TRUE;
    31c2:	377c 0001 002e 	move.w #1,46(a3)
    31c8:	4fef 000c      	lea 12(sp),sp
    31cc:	226d ff78      	movea.l -136(a5),a1
    31d0:	6000 fcf2      	bra.w 2ec4 2ec4 _MountDrive+0xd2a
	while (hunkCnt <= totalHunks) {
    31d4:	4aad ff98      	tst.l -104(a5)
    31d8:	6600 fbaa      	bne.w 2d84 2d84 _MountDrive+0xbea
    31dc:	2b6d ff98 ffa8 	move.l -104(a5),-88(a5)
    31e2:	9dce           	suba.l a6,a6
    31e4:	3b4e ffb8      	move.w a6,-72(a5)
		v = lseg_read_longs(md, 1, &temp);
    31e8:	240e           	move.l a6,d2
    31ea:	206d ffd0      	movea.l -48(a5),a0
    31ee:	2444           	movea.l d4,a2
    31f0:	6000 f7f4      	bra.w 29e6 29e6 _MountDrive+0x84c
    31f4:	41ed fff4      	lea -12(a5),a0
    31f8:	2f08           	move.l a0,-(sp)
    31fa:	4878 0001      	pea 1 1 __start+0x1
    31fe:	2f0b           	move.l a3,-(sp)
    3200:	2c44           	movea.l d4,a6
    3202:	4e96           	jsr (a6)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    3204:	222d fff4      	move.l -12(a5),d1
    3208:	7000           	moveq #0,d0
    320a:	302b 002c      	move.w 44(a3),d0
    320e:	4840           	swap d0
    3210:	4240           	clr.w d0
    3212:	2401           	move.l d1,d2
    3214:	4242           	clr.w d2
    3216:	4842           	swap d2
    3218:	8082           	or.l d2,d0
    321a:	2b40 ffe0      	move.l d0,-32(a5)
		md->lsegwordbuf = (UWORD)temp;
    321e:	3741 002c      	move.w d1,44(a3)
		md->lseghasword = TRUE;
    3222:	377c 0001 002e 	move.w #1,46(a3)
    3228:	4fef 000c      	lea 12(sp),sp
    322c:	6000 f6a2      	bra.w 28d0 28d0 _MountDrive+0x736
    3230:	45ed fff4      	lea -12(a5),a2
		v = lseg_read_longs(md, 1, &temp);
    3234:	2f0a           	move.l a2,-(sp)
    3236:	4878 0001      	pea 1 1 __start+0x1
    323a:	2f0b           	move.l a3,-(sp)
    323c:	2c44           	movea.l d4,a6
    323e:	4e96           	jsr (a6)
		md->lsegwordbuf = (UWORD)temp;
    3240:	376d fff6 002c 	move.w -10(a5),44(a3)
		md->lseghasword = TRUE;
    3246:	377c 0001 002e 	move.w #1,46(a3)
	firstHunk = lastHunk = -1;
    324c:	70ff           	moveq #-1,d0
    324e:	2b40 ffe0      	move.l d0,-32(a5)
    3252:	2b40 ffdc      	move.l d0,-36(a5)
    3256:	4fef 000c      	lea 12(sp),sp
    325a:	6000 fa78      	bra.w 2cd4 2cd4 _MountDrive+0xb3a
	LONG ret = -1;
    325e:	72ff           	moveq #-1,d1
}
    3260:	2001           	move.l d1,d0
    3262:	4ced 5cfc ff50 	movem.l -176(a5),d2-d7/a2-a4/a6
    3268:	4e5d           	unlk a5
    326a:	4e75           	rts
		v = lseg_read_longs(md, 1, &temp);
    326c:	486d fff4      	pea -12(a5)
    3270:	4878 0001      	pea 1 1 __start+0x1
    3274:	2f0b           	move.l a3,-(sp)
    3276:	2b48 ff78      	move.l a0,-136(a5)
    327a:	4e92           	jsr (a2)
		*data = (md->lsegwordbuf << 16) | (temp >> 16);
    327c:	282d fff4      	move.l -12(a5),d4
    3280:	7200           	moveq #0,d1
    3282:	322b 002c      	move.w 44(a3),d1
    3286:	4841           	swap d1
    3288:	4241           	clr.w d1
    328a:	2c04           	move.l d4,d6
    328c:	4246           	clr.w d6
    328e:	4846           	swap d6
    3290:	8286           	or.l d6,d1
    3292:	2b41 fff0      	move.l d1,-16(a5)
		md->lsegwordbuf = (UWORD)temp;
    3296:	3744 002c      	move.w d4,44(a3)
		md->lseghasword = TRUE;
    329a:	377c 0001 002e 	move.w #1,46(a3)
    32a0:	4fef 000c      	lea 12(sp),sp
    32a4:	206d ff78      	movea.l -136(a5),a0
    32a8:	6000 fd30      	bra.w 2fda 2fda _MountDrive+0xe40
    32ac:	2b48 ffd0      	move.l a0,-48(a5)
		while (hunkCnt < totalHunks) {
    32b0:	4aad ff98      	tst.l -104(a5)
    32b4:	6e00 fde2      	bgt.w 3098 3098 _MountDrive+0xefe
    32b8:	6000 faca      	bra.w 2d84 2d84 _MountDrive+0xbea
    32bc:	2b48 ffd0      	move.l a0,-48(a5)
			if (hunkCnt >= totalHunks) {
    32c0:	b4ad ff98      	cmp.l -104(a5),d2
    32c4:	6c00 fb9a      	bge.w 2e60 2e60 _MountDrive+0xcc6
		while (hunkCnt < totalHunks) {
    32c8:	4aad ff98      	tst.l -104(a5)
    32cc:	6e00 fdca      	bgt.w 3098 3098 _MountDrive+0xefe
    32d0:	6000 fab2      	bra.w 2d84 2d84 _MountDrive+0xbea

000032d4 000032d4 _mount_drives:

}
*/

int mount_drives(struct ConfigDev *cd, char *devName)
{
    32d4:	4e55 ffe0      	link.w a5,#-32
	struct MountStruct ms;
	int ret = 0;

	dbg("Mounter:\n");
	ms.deviceName = devName;
    32d8:	2b6d 000c ffec 	move.l 12(a5),-20(a5)
	ULONG units[] = {2,0,1};
    32de:	41ed ffe0      	lea -32(a5),a0
    32e2:	43f9 0000 003c 	lea 3c 3c __sdata+0x3c,a1
    32e8:	20d9           	move.l (a1)+,(a0)+
    32ea:	20d9           	move.l (a1)+,(a0)+
    32ec:	20d9           	move.l (a1)+,(a0)+
	ms.unitNum = &units;
    32ee:	41ed ffe0      	lea -32(a5),a0
    32f2:	2b48 fff0      	move.l a0,-16(a5)
	ms.creatorName = NULL;
    32f6:	42ad fff4      	clr.l -12(a5)
	ms.configDev = cd;
    32fa:	2b6d 0008 fff8 	move.l 8(a5),-8(a5)
	ms.SysBase =  *(struct ExecBase **)4UL;
    3300:	2b78 0004 fffc 	move.l 4 4 __start+0x4,-4(a5)

	ret = MountDrive(&ms);
    3306:	486d ffec      	pea -20(a5)
    330a:	4eba ee8e      	jsr 219a 219a _MountDrive(pc)

	return ret;
    330e:	4e5d           	unlk a5
    3310:	4e75           	rts
    3312:	0000           	Address 0x0000000000003314 is out of bounds.
.short 0x0000

00003314 00003314 _NewList:
    3314:	206f 0004      	movea.l 4(sp),a0
    3318:	2148 0008      	move.l a0,8(a0)
    331c:	5848           	addq.w #4,a0
    331e:	4290           	clr.l (a0)
    3320:	2148 fffc      	move.l a0,-4(a0)
    3324:	4e75           	rts
    3326:	4e71           	nop

00003328 00003328 _CreatePort:
    3328:	48e7 2032      	movem.l d2/a2-a3/a6,-(sp)
    332c:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    3332:	70ff           	moveq #-1,d0
    3334:	4eae feb6      	jsr -330(a6)
    3338:	1400           	move.b d0,d2
    333a:	6c04           	bge.s 3340 3340 _CreatePort+0x18
    333c:	7000           	moveq #0,d0
    333e:	605c           	bra.s 339c 339c _CreatePort+0x74
    3340:	7022           	moveq #34,d0
    3342:	223c 0001 0001 	move.l #65537,d1
    3348:	4eae ff3a      	jsr -198(a6)
    334c:	2440           	movea.l d0,a2
    334e:	b4fc 0000      	cmpa.w #0,a2
    3352:	660a           	bne.s 335e 335e _CreatePort+0x36
    3354:	4280           	clr.l d0
    3356:	1002           	move.b d2,d0
    3358:	4eae feb0      	jsr -336(a6)
    335c:	603c           	bra.s 339a 339a _CreatePort+0x72
    335e:	157c 0004 0008 	move.b #4,8(a2)
    3364:	156f 001b 0009 	move.b 27(sp),9(a2)
    336a:	256f 0014 000a 	move.l 20(sp),10(a2)
    3370:	1542 000f      	move.b d2,15(a2)
    3374:	93c9           	suba.l a1,a1
    3376:	4eae feda      	jsr -294(a6)
    337a:	2540 0010      	move.l d0,16(a2)
    337e:	4aaa 000a      	tst.l 10(a2)
    3382:	6708           	beq.s 338c 338c _CreatePort+0x64
    3384:	224a           	movea.l a2,a1
    3386:	4eae fe9e      	jsr -354(a6)
    338a:	600e           	bra.s 339a 339a _CreatePort+0x72
    338c:	41ea 0014      	lea 20(a2),a0
    3390:	2548 001c      	move.l a0,28(a2)
    3394:	47ea 0018      	lea 24(a2),a3
    3398:	208b           	move.l a3,(a0)
    339a:	200a           	move.l a2,d0
    339c:	4cdf 4c04      	movem.l (sp)+,d2/a2-a3/a6
    33a0:	4e75           	rts

000033a2 000033a2 _DeletePort:
    33a2:	48e7 2022      	movem.l d2/a2/a6,-(sp)
    33a6:	246f 0010      	movea.l 16(sp),a2
    33aa:	4aaa 000a      	tst.l 10(a2)
    33ae:	670c           	beq.s 33bc 33bc _DeletePort+0x1a
    33b0:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    33b6:	224a           	movea.l a2,a1
    33b8:	4eae fe98      	jsr -360(a6)
    33bc:	50ea 0008      	st 8(a2)
    33c0:	74ff           	moveq #-1,d2
    33c2:	2542 0014      	move.l d2,20(a2)
    33c6:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    33cc:	4280           	clr.l d0
    33ce:	102a 000f      	move.b 15(a2),d0
    33d2:	4eae feb0      	jsr -336(a6)
    33d6:	224a           	movea.l a2,a1
    33d8:	7022           	moveq #34,d0
    33da:	4eae ff2e      	jsr -210(a6)
    33de:	4cdf 4404      	movem.l (sp)+,d2/a2/a6
    33e2:	4e75           	rts

000033e4 000033e4 _CreateExtIO:
    33e4:	48e7 3002      	movem.l d2-d3/a6,-(sp)
    33e8:	242f 0010      	move.l 16(sp),d2
    33ec:	262f 0014      	move.l 20(sp),d3
    33f0:	91c8           	suba.l a0,a0
    33f2:	4a82           	tst.l d2
    33f4:	6728           	beq.s 341e 341e _CreateExtIO+0x3a
    33f6:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    33fc:	2003           	move.l d3,d0
    33fe:	223c 0001 0001 	move.l #65537,d1
    3404:	4eae ff3a      	jsr -198(a6)
    3408:	2040           	movea.l d0,a0
    340a:	b0fc 0000      	cmpa.w #0,a0
    340e:	670e           	beq.s 341e 341e _CreateExtIO+0x3a
    3410:	117c 0005 0008 	move.b #5,8(a0)
    3416:	2142 000e      	move.l d2,14(a0)
    341a:	3143 0012      	move.w d3,18(a0)
    341e:	2008           	move.l a0,d0
    3420:	4cdf 400c      	movem.l (sp)+,d2-d3/a6
    3424:	4e75           	rts

00003426 00003426 _CreateStdIO:
    3426:	4878 0030      	pea 30 30 _device_id_string+0x12
    342a:	2f2f 0008      	move.l 8(sp),-(sp)
    342e:	61b4           	bsr.s 33e4 33e4 _CreateExtIO
    3430:	504f           	addq.w #8,sp
    3432:	4e75           	rts
    3434:	4e71           	nop

00003436 00003436 _DeleteExtIO:
    3436:	2f0e           	move.l a6,-(sp)
    3438:	226f 0008      	movea.l 8(sp),a1
    343c:	70ff           	moveq #-1,d0
    343e:	50e9 0008      	st 8(a1)
    3442:	2340 0014      	move.l d0,20(a1)
    3446:	2340 0018      	move.l d0,24(a1)
    344a:	2c79 0000 0004 	movea.l 4 4 _SysBase,a6
    3450:	4280           	clr.l d0
    3452:	3029 0012      	move.w 18(a1),d0
    3456:	4eae ff2e      	jsr -210(a6)
    345a:	2c5f           	movea.l (sp)+,a6
    345c:	4e75           	rts
    345e:	0000           	Address 0x0000000000003460 is out of bounds.
.short 0x0000

00003460 00003460 _CreateTask:
    3460:	48e7 303a      	movem.l d2-d3/a2-a4/a6,-(sp)
    3464:	4e55 ffe0      	link.w a5,#-32
    3468:	202d 002c      	move.l 44(a5),d0
    346c:	5680           	addq.l #3,d0
    346e:	76fc           	moveq #-4,d3
    3470:	c680           	and.l d0,d3
    3472:	41fa 0082      	lea 34f6 34f6 _CreateTask+0x96(pc),a0
    3476:	224f           	movea.l sp,a1
    3478:	7006           	moveq #6,d0
    347a:	22d8           	move.l (a0)+,(a1)+
    347c:	51c8 fffc      	dbf d0,347a 347a _CreateTask+0x1a
    3480:	2283           	move.l d3,(a1)
    3482:	204f           	movea.l sp,a0
    3484:	2c78 0004      	movea.l 4 4 __start+0x4,a6
    3488:	4eae ff22      	jsr -222(a6)
    348c:	2440           	movea.l d0,a2
    348e:	240a           	move.l a2,d2
    3490:	6b58           	bmi.s 34ea 34ea _CreateTask+0x8a
    3492:	286a 0010      	movea.l 16(a2),a4
    3496:	197c 0001 0008 	move.b #1,8(a4)
    349c:	196d 0027 0009 	move.b 39(a5),9(a4)
    34a2:	296d 0020 000a 	move.l 32(a5),10(a4)
    34a8:	202a 0018      	move.l 24(a2),d0
    34ac:	d680           	add.l d0,d3
    34ae:	2943 0036      	move.l d3,54(a4)
    34b2:	2940 003a      	move.l d0,58(a4)
    34b6:	2943 003e      	move.l d3,62(a4)
    34ba:	41ec 004a      	lea 74(a4),a0
    34be:	2148 0008      	move.l a0,8(a0)
    34c2:	5888           	addq.l #4,a0
    34c4:	2108           	move.l a0,-(a0)
    34c6:	224a           	movea.l a2,a1
    34c8:	4eae ff10      	jsr -240(a6)
    34cc:	224c           	movea.l a4,a1
    34ce:	246d 0028      	movea.l 40(a5),a2
    34d2:	97cb           	suba.l a3,a3
    34d4:	4eae fee6      	jsr -282(a6)
    34d8:	0c6e 0025 0014 	cmpi.w #37,20(a6)
    34de:	650c           	bcs.s 34ec 34ec _CreateTask+0x8c
    34e0:	4a80           	tst.l d0
    34e2:	6608           	bne.s 34ec 34ec _CreateTask+0x8c
    34e4:	2042           	movea.l d2,a0
    34e6:	4eae ff1c      	jsr -228(a6)
    34ea:	99cc           	suba.l a4,a4
    34ec:	200c           	move.l a4,d0
    34ee:	4e5d           	unlk a5
    34f0:	4cdf 5c0c      	movem.l (sp)+,d2-d3/a2-a4/a6
    34f4:	4e75           	rts
    34f6:	0000 0000      	ori.b #0,d0
    34fa:	0000 0000      	ori.b #0,d0
    34fe:	0000 0000      	ori.b #0,d0
    3502:	0000 0002      	ori.b #2,d0
    3506:	0001 0001      	ori.b #1,d1
    350a:	0000 005c      	ori.b #92,d0
    350e:	0001 0000      	ori.b #0,d1
    3512:	0000           	Address 0x0000000000003514 is out of bounds.
.short 0x0000

00003514 00003514 ___mulsi3:
    3514:	4cef 0003 0004 	movem.l 4(sp),d0-d1
    351a:	2f03           	move.l d3,-(sp)
    351c:	2f02           	move.l d2,-(sp)
    351e:	3401           	move.w d1,d2
    3520:	c4c0           	mulu.w d0,d2
    3522:	2601           	move.l d1,d3
    3524:	4843           	swap d3
    3526:	c6c0           	mulu.w d0,d3
    3528:	4843           	swap d3
    352a:	4243           	clr.w d3
    352c:	d483           	add.l d3,d2
    352e:	4840           	swap d0
    3530:	c0c1           	mulu.w d1,d0
    3532:	4840           	swap d0
    3534:	4240           	clr.w d0
    3536:	d082           	add.l d2,d0
    3538:	241f           	move.l (sp)+,d2
    353a:	261f           	move.l (sp)+,d3
    353c:	4e75           	rts
    353e:	0000           	Address 0x0000000000003540 is out of bounds.
.short 0x0000
