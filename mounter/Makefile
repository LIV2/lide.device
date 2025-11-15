INCLUDE=/opt/amiga/m68k-amigaos/ndk-include
AS=vasmm68k_mot
ASFLAGS=-Fhunk -I$(INCLUDE) -quiet -align -DROM
OBJDIR=obj

SRCS = $(wildcard *.S)
OBJ = $(SRCS:%.S=$(OBJDIR)/%.o)

$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	$(AS) $(ASFLAGS) -o $@ $<

all: $(OBJ)

clean:
	rm -f $(OBJDIR)/*