PROJECT=lide
INCLUDE=/opt/amiga/m68k-amigaos/ndk-include
AS=vasmm68k_mot
ASFLAGS=-Fhunk -I$(INCLUDE) -quiet -align -DROM
LINKER=vlink
LINKFLAGS=-brawbin1 -s -sc -sd -mrel -lamiga -lauto -L/opt/amiga/m68k-amigaos/vbcc/lib
OBJDIR=obj
DEVICE=../build/lide.device
BUILDDIR=build

.PHONY: all clean rom $(DEVICE) checksize
.INTERMEDIATE: $(OBJDIR)/$(PROJECT)-word.rom
SRCS = bootldr.S \
       endrom.S
OBJS = $(SRCS:%.S=$(OBJDIR)/%.o)

all:	../$(BUILDDIR)/$(PROJECT).rom ../$(BUILDDIR)/$(PROJECT)-atbus.rom ../$(BUILDDIR)/lide-N2630-high.rom ../$(BUILDDIR)/lide-N2630-low.rom

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

$(OBJDIR)/$(PROJECT)-word.rom: $(OBJDIR)/assets-word.o | checksize
	$(LINKER) $(LINKFLAGS) -Trom.ld -o $@ $(OBJDIR)/assets-word.o

../$(BUILDDIR)/lide-N2630-high.rom: $(OBJDIR)/$(PROJECT)-word.rom
	srec_cat $< -binary -split 2 0 1 -out $@ -binary

../$(BUILDDIR)/lide-N2630-low.rom:  $(OBJDIR)/$(PROJECT)-word.rom
	srec_cat $< -binary -split 2 1 1 -out $@ -binary

clean:
	rm -f $(OBJDIR)/*
	rm -f ../$(BUILDDIR)/$(PROJECT).rom
	rm -f ../$(BUILDDIR)/$(PROJECT)-atbus.rom
	rm -f ../$(BUILDDIR)/$(PROJECT)-word.rom
	rm -f mungerom
