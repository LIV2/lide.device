PROJECT=lide
INCLUDE=/opt/amiga/m68k-amigaos/ndk-include
AS=vasmm68k_mot
ASFLAGS=-Fhunk -I$(INCLUDE) -quiet -align -DROM
LINKER=vlink
LINKFLAGS=-brawbin1 -s -sc -sd -mrel -lamiga -lauto -L/opt/amiga/m68k-amigaos/vbcc/lib
OBJDIR=obj

.PHONY: all clean rom ../lide.device

SRCS = bootldr.S \
       endrom.S
OBJS = $(SRCS:%.S=$(OBJDIR)/%.o)

all:	../$(PROJECT).rom ../$(PROJECT)-atbus.rom ../$(PROJECT)-word.rom

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

$(OBJDIR)/assets.o: assets.S $(OBJDIR)/bootnibbles ../lide.device

$(OBJDIR)/assets-word.o: assets.S $(OBJDIR)/bootldr-word ../lide.device

../$(PROJECT).rom: $(OBJDIR)/bootnibbles $(OBJDIR)/assets.o
	$(LINKER) $(LINKFLAGS) -Trom.ld -o $@ $(OBJDIR)/assets.o

../$(PROJECT)-atbus.rom: $(OBJDIR)/bootnibbles $(OBJDIR)/assets.o
	$(LINKER) $(LINKFLAGS) -Tatbusrom.ld -o $@ $(OBJDIR)/assets.o

../$(PROJECT)-word.rom: $(OBJDIR)/assets-word.o
	$(LINKER) $(LINKFLAGS) -Trom.ld -o $@ $(OBJDIR)/assets-word.o

clean:
	rm -f $(OBJDIR)/*
	rm -f ../$(PROJECT).rom
	rm -f ../$(PROJECT)-atbus.rom
	rm -f ../$(PROJECT)-word.rom
	rm -f mungerom
