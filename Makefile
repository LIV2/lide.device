PROJECT=lide.device
BUILDDIR=build
ROM=lide.rom
VERSION := $(shell git describe --tags --dirty | sed -r 's/^Release-//')
GIT_REF := "$(shell git branch --show-current)-$(shell git rev-parse --short HEAD)"
BUILD_DATE := $(shell date  +"%d.%m.%Y")
CC=m68k-amigaos-gcc
CFLAGS+=-nostartfiles -nostdlib -noixemul -mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign -Wno-attributes  -Wno-unused-value -s -Os -fomit-frame-pointer -DCDBOOT=1 -DNO_RDBLAST=1
CFLAGS+=-DGIT_REF=$(GIT_REF) -DBUILD_DATE=$(BUILD_DATE)
LDFLAGS=-lamiga -lgcc -lc
AS=m68k-amigaos-as

ifneq ($(VERSION),)
DISK=lide-update-$(VERSION).adf
else
DISK=lide-update.adf
endif

ifdef DEBUG
CFLAGS+= -DDEBUG=$(DEBUG)
LDFLAGS+= -ldebug
.PHONY: $(PROJECT)
endif

ifdef NOTIMER
CFLAGS+= -DNOTIMER=1
.PHONY: $(PROJECT)
endif

ifdef SLOWXFER
CFLAGS+= -DSLOWXFER=1
.PHONY: $(PROJECT)
endif

LDFLAGS+= -lnix13

.PHONY:	clean all lideflash disk lha rename/renamelide lidetool/lidetool
.INTERMEDIATE: move16.o

all:	$(ROM) \
		lideflash \
		rename/renamelide \
		lide-N2630-high.rom \
		lide-N2630-low.rom

OBJ = driver.o \
      ata.o \
	  atapi.o \
	  scsi.o \
	  idetask.o \
	  mounter.o \
	  debug.o

ASMOBJ = endskip.o

SRCS = $(OBJ:%.o=%.c)
SRCS += $(ASMOBJ:%.o=%.S)
SRCS += move16.o

move16.o: move16.c
	${CC} -o $@ $(CFLAGS) -c -mcpu=68060 $< $(LDFLAGS)


$(PROJECT): $(SRCS)
	${CC} -o $@ $(CFLAGS) $(SRCS) $(LDFLAGS)

$(ROM): $(PROJECT)
	make -C bootrom

lideflash/lideflash:
	make -C lideflash

lideflash: lideflash/lideflash

lidetool/lidetool:
	make -C lidetool

rename/renamelide:
	make -C rename

lideflash/bootblock/obj/bootblock.o:
	make -C lideflash/bootblock

disk: $(ROM) lideflash/lideflash rename/renamelide lidetool/lidetool lideflash/bootblock/obj/bootblock.o
	@mkdir -p $(BUILDDIR)
	cp $(ROM) build
	echo -n 'lideflash -I $(ROM)\n' > $(BUILDDIR)/startup-sequence
	xdftool $(BUILDDIR)/$(DISK) format lide-update + \
	                            boot write lideflash/bootblock/obj/bootblock.o + \
	                            write $(ROM) + \
	                            write lidetool/lidetool lidetool + \
	                            write lideflash/lideflash lideflash + \
	                            write rename/renamelide renamelide + \
	                            makedir s + \
	                            write $(BUILDDIR)/startup-sequence s/startup-sequence + \
	                            makedir Expansion + \
	                            write info/Expansion.info Expansion.info + \
	                            write info/lide.device.info Expansion/lide.device.info + \
	                            write lide.device Expansion/lide.device


$(BUILDDIR)/lide-update.lha: lideflash/lideflash $(ROM) rename/renamelide lidetool/lidetool
	@mkdir -p $(BUILDDIR)
	cp $^ $(BUILDDIR)
	cd $(BUILDDIR) && lha -c ../$@ $(notdir $^) 

lha: $(BUILDDIR)/lide-update.lha 

lide-N2630-high.rom: $(ROM)
	srec_cat lide-word.rom -binary -split 2 0 1 -out $@ -binary

lide-N2630-low.rom:  $(ROM)
	srec_cat lide-word.rom -binary -split 2 1 1 -out $@ -binary

lide-tk-29F010.rom: $(ROM)
	@cat lide-atbus.rom lide-atbus.rom lide-atbus.rom lide-atbus.rom > $@

lide-tk-29F020.rom: lide-tk-29F010.rom
	@cat $< $< > $@

lide-tk-29F040.rom: lide-tk-29F020.rom
	@cat $< $< > $@

clean:
	-rm $(PROJECT)
	make -C bootrom clean
	make -C lideflash clean
	make -C rename clean
	-rm -rf *.rom
	-rm -rf $(BUILDDIR)
	make -C lideflash/bootblock clean