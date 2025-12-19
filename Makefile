PROJECT=lide.device
BUILDDIR=build
ROM=lide.rom
VERSION := $(shell git describe --tags --dirty | sed -r 's/^Release-//')
TARGET=lide.device

BGREEN = \033[1;32m
GREEN = \033[0;32m
WHITE = \033[1;37m
NC    = \033[0m

GIT_REF_NAME = $(shell git branch --show-current)
GIT_REF := "$(GIT_REF_NAME)-$(shell git rev-parse --short HEAD)"
BUILD_DATE := $(shell date  +"%d.%m.%Y")

export BUILD_DATE
export GIT_REF

MAKE=make -j -s
CC=m68k-amigaos-gcc
CFLAGS+=-mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign -Wno-unused-value -s -Os -fomit-frame-pointer -DCDBOOT=1 -DNO_RDBLAST=1
CFLAGS+=-DGIT_REF=$(GIT_REF) -DBUILD_DATE=$(BUILD_DATE)
LD=m68k-amigaos-ld
LDFLAGS=-lc
AS=m68k-amigaos-as

ifneq ($(VERSION),)
DISK=lide-update-$(VERSION).adf
DEVICE_VERSION=$(shell echo $(VERSION) | awk -F'-' '{split($$1,a,".");print a[1]}')
DEVICE_REVISION=$(shell echo $(VERSION) | awk -F'-' '{split($$1,a,".");print a[2]}')
CFLAGS+=-DDEVICE_VERSION=$(DEVICE_VERSION) -DDEVICE_REVISION=$(DEVICE_REVISION)

export DEVICE_REVISION
export DEVICE_VERSION

else
DISK=lide-update.adf
endif

.PHONY:	clean all lideflash disk lha rename/renamelide lidetool/lidetool

all:	$(PROJECT) \
		AIDE-$(PROJECT) \
		amigapci-$(PROJECT) \
		lide-N2630-high.rom \
		lide-N2630-low.rom \
		rename/renamelide \
		lideflash \
		$(ROM)

OBJDIR = obj/$(TARGET)

SRCS = device.c ata.c atapi.c scsi.c iotask.c lide_alib.c mounter/mounter.c debug.c

OBJ = $(addprefix $(OBJDIR)/,$(notdir $(SRCS:%c=%o)))

ASMOBJ =  $(OBJDIR)/endskip.o

ifdef DEBUG
CFLAGS+= -DDEBUG=$(DEBUG)
LDFLAGS=-ldebug -lgcc -lc -L/opt/amiga/lib/gcc/m68k-amigaos/6.5.0b/
.PHONY: build $(SRCS)
endif

ifdef SIMPLE_IDE
CFLAGS+= -DSIMPLE_IDE=1
ASMOBJ+=$(OBJDIR)/bootblock.o
endif

ifdef AMIGAPCI
CFLAGS+= -DAMIGAPCI=1
ASMOBJ+= $(OBJDIR)/bootblock.o
endif

build: $(OBJ) | $(ASMOBJ)
	@printf "${BGREEN}Linking $(TARGET)${NC}\n"
	@${LD} -s -o $(TARGET) $^ ${LDFLAGS} $(ASMOBJ)

lide.device: $(SRCS)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} TARGET=lide.device build

amigapci-lide.device: $(SRCS)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} TARGET=amigapci-lide.device AMIGAPCI=1 build
 
AIDE-lide.device: $(SRCS)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} TARGET=AIDE-lide.device SIMPLE_IDE=1 build

$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR)
	@printf "${GREEN}$@${NC}\n"
	@${CC} -o $@ -c $< ${CFLAGS}

$(OBJDIR)/%.o: mounter/%.c
	@mkdir -p $(OBJDIR)
	@printf "${GREEN}$@${NC}\n"
	@${CC} -o $@ -c $< ${CFLAGS}

$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	@printf "${GREEN}$@${NC}\n"
	@${AS} -o $@ $<

$(ROM): $(PROJECT)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} -C bootrom

lideflash/lideflash:
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} -C lideflash
	@printf "Done.\n"

lideflash: lideflash/lideflash

lidetool/lidetool:
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} -C lidetool
	@printf "Done.\n"

rename/renamelide:
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} -C rename
	@printf "Done.\n"

$(BUILDDIR)/AIDE-boot-$(VERSION).adf: AIDE-$(PROJECT)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} -C aide-boot
	@mkdir -p $(BUILDDIR)
	@mv aide-boot/aide-boot.adf $@

disk:	$(BUILDDIR)/$(DISK) $(BUILDDIR)/AIDE-boot-$(VERSION).adf

$(BUILDDIR)/$(DISK): $(ROM) AIDE-lide.device lideflash/lideflash rename/renamelide lidetool/lidetool
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@mkdir -p $(BUILDDIR)
	@cp $(ROM) build
	@echo 'lideflash -I $(ROM)' > $(BUILDDIR)/startup-sequence
	@xdftool $(BUILDDIR)/$(DISK) format lide-update + \
	                            boot install + \
	                            write $(ROM) + \
	                            write lidetool/lidetool lidetool + \
	                            write lideflash/lideflash lideflash + \
	                            write rename/renamelide renamelide + \
	                            makedir s + \
	                            write $(BUILDDIR)/startup-sequence s/startup-sequence + \
	                            makedir Expansion + \
	                            write info/Expansion.info Expansion.info + \
	                            write info/lide.device.info Expansion/lide.device.info + \
	                            write lide.device Expansion/lide.device + \
	                            write AIDE-lide.device AIDE-lide.device
	@printf "Done.\n"

$(BUILDDIR)/lide-update.lha: lideflash/lideflash $(ROM) rename/renamelide lidetool/lidetool lide.device info/lide.device.info AIDE-lide.device
	@mkdir -p $(BUILDDIR)
	cp $^ $(BUILDDIR)
	cd $(BUILDDIR) && lha -c ../$@ $(notdir $^) 

lha: $(BUILDDIR)/lide-update.lha 

lide-N2630-high.rom: $(ROM)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	srec_cat lide-word.rom -binary -split 2 0 1 -out $@ -binary

lide-N2630-low.rom:  $(ROM)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	srec_cat lide-word.rom -binary -split 2 1 1 -out $@ -binary

lide-tk-29F010.rom: $(ROM)
	@cat lide-atbus.rom lide-atbus.rom lide-atbus.rom lide-atbus.rom > $@

lide-tk-29F020.rom: lide-tk-29F010.rom
	@cat $< $< > $@

lide-tk-29F040.rom: lide-tk-29F020.rom
	@cat $< $< > $@

clean:
	@-rm -rf obj
	@-rm -f *.device
	@-rm -f *.rom
	@${MAKE} -C bootrom clean
	@${MAKE} -C lideflash clean
	@${MAKE} -C lidetool clean
	@${MAKE} -C rename clean
	@-rm -rf $(BUILDDIR)
	@${MAKE} -C aide-boot clean
