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

aide-boot.adf:	$(OBJDIR)/bootblock.o ../$(BUILDDIR)/AIDE-lide.device
	rm -f $@
	xdftool	$@ create + format lide + boot write obj/bootblock.o + write ../$(BUILDDIR)/AIDE-lide.device AIDE-lide.device
	./patchdisk.py --device ../$(BUILDDIR)/AIDE-lide.device --adf $@
