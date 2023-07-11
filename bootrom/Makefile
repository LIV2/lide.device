PROJECT=lide
INCLUDE=/opt/amiga/m68k-amigaos/ndk-include
AS=vasmm68k_mot
ASFLAGS=-Fhunk -I$(INCLUDE) -quiet -align -DROM -DBYTEWIDE
LINKER=vlink
LINKFLAGS=-brawbin1 -s -sc -sd -mrel -lamiga -lauto -L/opt/amiga/m68k-amigaos/vbcc/lib
OBJDIR=obj

.PHONY: all clean rom ../lide.device

SRCS = bootldr.S \
       endrom.S
OBJS = $(SRCS:%.S=$(OBJDIR)/%.o)

all:	../$(PROJECT).rom ../$(PROJECT)-atbus.rom

$(OBJDIR)/mungerom: mungerom.c
	@mkdir -p $(OBJDIR)
	$(CC) $< -Wall -o $@

$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) -o $@ $<

$(OBJDIR)/bootldr:	$(OBJDIR)/bootldr.o
	$(LINKER) $(LINKFLAGS) -o $@ $^

$(OBJDIR)/bootnibbles: $(OBJDIR)/bootldr $(OBJDIR)/mungerom
	@mkdir -p $(OBJDIR)
	./obj/mungerom

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
