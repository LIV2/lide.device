PROJECT=lide
INCLUDE=/opt/amiga/m68k-amigaos/ndk-include
AS=vasmm68k_mot
ASFLAGS=-Fhunk -I$(INCLUDE) -quiet -align -DROM
LINKER=vlink
LINKFLAGS=-brawbin1 -s -sc -sd -mrel -lamiga -lauto -L/opt/amiga/m68k-amigaos/vbcc/lib
OBJDIR=obj
DEVICE=../build/lide.device

.PHONY: all clean rom $(DEVICE) checksize

SRCS = bootldr.S \
       endrom.S
OBJS = $(SRCS:%.S=$(OBJDIR)/%.o)

all:	../$(BUILDDIR)/$(PROJECT).rom ../$(BUILDDIR)/$(PROJECT)-atbus.rom ../$(BUILDDIR)/$(PROJECT)-word.rom

checksize:
	./checksize.sh $(DEVICE)

# Nibble-wide boot loader with Byte-wide driver
$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) -DBYTEWIDE -o $@ $<

# Word-wide boot loader & driver
$(OBJDIR)/%-word.o: %.S
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) -DWORDWIDE -o $@ $<

$(OBJDIR)/bootldr:	$(OBJDIR)/bootldr.o
	$(LINKER) $(LINKFLAGS) -o $@ $^

$(OBJDIR)/bootldr-word:	$(OBJDIR)/bootldr-word.o
	$(LINKER) $(LINKFLAGS) -o $@ $^

$(OBJDIR)/bootnibbles: $(OBJDIR)/bootldr mungerom.py
	@mkdir -p $(OBJDIR)
	./mungerom.py

$(OBJDIR)/assets.o: assets.S $(OBJDIR)/bootnibbles $(DEVICE)

$(OBJDIR)/assets-word.o: assets.S $(OBJDIR)/bootldr-word $(DEVICE)

../$(BUILDDIR)/$(PROJECT).rom: $(OBJDIR)/bootnibbles $(OBJDIR)/assets.o | checksize
	$(LINKER) $(LINKFLAGS) -Trom.ld -o $@ $(OBJDIR)/assets.o

../$(BUILDDIR)/$(PROJECT)-atbus.rom: $(OBJDIR)/bootnibbles $(OBJDIR)/assets.o | checksize
	$(LINKER) $(LINKFLAGS) -Tatbusrom.ld -o $@ $(OBJDIR)/assets.o

../$(BUILDDIR)/$(PROJECT)-word.rom: $(OBJDIR)/assets-word.o | checksize
	$(LINKER) $(LINKFLAGS) -Trom.ld -o $@ $(OBJDIR)/assets-word.o

clean:
	rm -f $(OBJDIR)/*
	rm -f ../$(BUILDDIR)/$(PROJECT).rom
	rm -f ../$(BUILDDIR)/$(PROJECT)-atbus.rom
	rm -f ../$(BUILDDIR)/$(PROJECT)-word.rom
	rm -f mungerom
