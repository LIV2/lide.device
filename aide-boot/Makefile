INCLUDE=/opt/amiga/m68k-amigaos/ndk-include
AS=vasmm68k_mot
ASFLAGS=-Fbin -m68000 -I$(INCLUDE) -quiet -align
OBJDIR=obj

.PHONY: all clean

SRCS = bootblock.S
OBJS = $(SRCS:%.S=$(OBJDIR)/%.o)

all: ${OBJDIR}/bootblock.o aide-boot.adf

clean:
	-rm -rf $(OBJDIR)

$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) -o $@ $<

obj/patchdisk: patchdisk.c
	gcc -o $@ $<
	chmod +x $@

aide-boot.adf:	$(OBJDIR)/bootblock.o ../AIDE-lide.device	obj/patchdisk
	rm -f $@
	xdftool	$@ create + format lide + boot write obj/bootblock.o + write ../AIDE-lide.device AIDE-lide.device
	dd if=../AIDE-lide.device of=$@ bs=512 oseek=2 conv=notrunc
	./obj/patchdisk