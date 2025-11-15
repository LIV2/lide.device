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

all:	../$(PROJECT).rom ../$(PROJECT)-atbus.rom

# Nibble-wide boot loader with Byte-wide driver
$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) -DBYTEWIDE -o $@ $<

$(OBJDIR)/bootldr:	$(OBJDIR)/bootldr.o
	$(LINKER) $(LINKFLAGS) -o $@ $^

$(OBJDIR)/bootnibbles: $(OBJDIR)/bootldr mungerom.py
	@mkdir -p $(OBJDIR)
	./mungerom.py

$(OBJDIR)/assets.o: assets.S $(OBJDIR)/bootnibbles ../lide.device

../$(PROJECT).rom: $(OBJDIR)/bootnibbles $(OBJDIR)/assets.o
	$(LINKER) $(LINKFLAGS) -Trom.ld -o $@ $(OBJDIR)/assets.o

../$(PROJECT)-atbus.rom: $(OBJDIR)/bootnibbles $(OBJDIR)/assets.o
	$(LINKER) $(LINKFLAGS) -Tatbusrom.ld -o $@ $(OBJDIR)/assets.o

clean:
	rm -f $(OBJDIR)/*
	rm -f ../$(PROJECT).rom
	rm -f ../$(PROJECT)-atbus.rom
	rm -f mungerom
