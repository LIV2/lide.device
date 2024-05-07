INCLUDE=/opt/amiga/m68k-amigaos/ndk-include
AS=vasmm68k_mot
ASFLAGS=-Fbin -m68000 -I$(INCLUDE) -quiet -align
OBJDIR=obj

.PHONY: all clean

SRCS = bootblock.S
OBJS = $(SRCS:%.S=$(OBJDIR)/%.o)

all: ${OBJDIR}/bootblock.o

clean:
	-rm -rf $(OBJDIR)

$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) -o $@ $<