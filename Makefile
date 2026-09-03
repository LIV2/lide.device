PROJECT=lide.device
BUILDDIR=build
ROM=$(BUILDDIR)/lide.rom
VERSION := $(shell git describe --tags --dirty | sed -r 's/^Release-//')
TARGET=lide.device

ODFS_VERSION=0.7.0
ODFS_URL="https://github.com/reinauer/ODFileSystem/releases/download/v$(ODFS_VERSION)/ODFileSystem-rom"

BGREEN = \033[1;32m
GREEN = \033[0;32m
WHITE = \033[1;37m
NC    = \033[0m

GIT_REF_NAME = $(shell git branch --show-current)
GIT_REF := "$(GIT_REF_NAME)-$(shell git rev-parse --short HEAD)"
BUILD_DATE := $(shell date  +"%d.%m.%Y")

AMIGAPCI_ROMBASE=0xF00000

export BUILD_DATE
export GIT_REF

MAKE=make -j -s
CC=m68k-amigaos-gcc
CFLAGS+=-mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign -s -Os -fomit-frame-pointer -DCDBOOT=1 -DNO_RDBLAST=1
CFLAGS+=-DGIT_REF=$(GIT_REF) -DBUILD_DATE=$(BUILD_DATE)
LD=m68k-amigaos-ld
LDFLAGS=-lc lide.ld
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

.PHONY:	clean all lideflash disk lha lidetool/lidetool build

all:	$(BUILDDIR)/$(PROJECT) \
		$(ROM) \
		$(BUILDDIR)/AIDE-$(PROJECT) \
		$(BUILDDIR)/amigapci-$(PROJECT) \
		$(BUILDDIR)/amigapci-lide.rom \
		$(BUILDDIR)/lide-N2630-high.rom \
		$(BUILDDIR)/lide-N2630-low.rom \
		lideflash

OBJDIR = obj/$(TARGET)

SRCS = device.c ata.c atapi.c scsi.c iotask.c lide_alib.c 3rdparty/mounter/mounter.c debug.c

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
	@mkdir -p $(BUILDDIR)
	@printf "${BGREEN}Linking $(TARGET)${NC}\n"
	@${LD} -s -o $(BUILDDIR)/$(TARGET) $^ ${LDFLAGS} $(ASMOBJ)

$(BUILDDIR)/lide.device: $(SRCS)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} TARGET=lide.device build

$(BUILDDIR)/amigapci-lide.device: $(SRCS)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} TARGET=amigapci-lide.device AMIGAPCI=1 build
 
$(BUILDDIR)/AIDE-lide.device: $(SRCS)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} TARGET=AIDE-lide.device SIMPLE_IDE=1 build

$(OBJDIR)/%.o: %.c
	@mkdir -p $(OBJDIR)
	@printf "${GREEN}$@${NC}\n"
	@${CC} -o $@ -c $< ${CFLAGS}

$(OBJDIR)/%.o: 3rdparty/mounter/%.c
	@mkdir -p $(OBJDIR)
	@printf "${GREEN}$@${NC}\n"
	@${CC} -o $@ -c $< ${CFLAGS}

$(OBJDIR)/%.o: %.S
	@mkdir -p $(OBJDIR)
	@printf "${GREEN}$@${NC}\n"
	@${AS} -o $@ $<

$(ROM) $(BUILDDIR)/lide-N2630-high.rom $(BUILDDIR)/lide-N2630-low.rom $(BUILDDIR)/lide-atbus.rom &: $(BUILDDIR)/$(PROJECT)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} BUILDDIR=$(BUILDDIR) -C bootrom
	@printf "Done.\n"

lideflash/lideflash:
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} -C lideflash
	@printf "Done.\n"

lideflash: lideflash/lideflash

lidetool/lidetool:
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} -C lidetool
	@printf "Done.\n"

$(BUILDDIR)/AIDE-boot-$(VERSION).adf: $(BUILDDIR)/AIDE-$(PROJECT)
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@${MAKE} BUILDDIR=$(BUILDDIR) -C aide-boot
	@mkdir -p $(BUILDDIR)
	@mv aide-boot/aide-boot.adf $@

$(BUILDDIR)/cdfs.rom:
	@printf "${WHITE}#### Retrieving ODFS ####${NC}\n"
	curl -fSsL $(ODFS_URL) -o $@

$(BUILDDIR)/amigapci-lide.rom: $(BUILDDIR)/amigapci-lide.device
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	romtool build -t ext -e ${AMIGAPCI_ROMBASE} $^ -o $@

disk:	$(BUILDDIR)/$(DISK) $(BUILDDIR)/AIDE-boot-$(VERSION).adf

$(BUILDDIR)/$(DISK): $(ROM) $(BUILDDIR)/lide.device $(BUILDDIR)/AIDE-lide.device lideflash/lideflash lidetool/lidetool $(BUILDDIR)/cdfs.rom 
	@printf "${WHITE}#### Building $@ ####${NC}\n"
	@mkdir -p $(BUILDDIR)
	@echo 'lideflash -I lide.rom' > $(BUILDDIR)/startup-sequence
	@xdftool $(BUILDDIR)/$(DISK) format lide-update + \
	                            boot install + \
	                            write $(ROM) + \
								write $(BUILDDIR)/cdfs.rom cdfs.rom + \
	                            write lidetool/lidetool lidetool + \
	                            write lideflash/lideflash lideflash + \
	                            makedir s + \
	                            write $(BUILDDIR)/startup-sequence s/startup-sequence + \
	                            makedir Expansion + \
	                            write dist/Expansion.info Expansion.info + \
	                            write dist/Expansion/lide.device.info Expansion/lide.device.info + \
	                            write $(BUILDDIR)/lide.device Expansion/lide.device + \
	                            write $(BUILDDIR)/AIDE-lide.device AIDE-lide.device
	@printf "Done.\n"

$(BUILDDIR)/lide-update.lha: lideflash/lideflash $(ROM) build/lide-atbus.rom build/lide-N2630-high.rom build/lide-N2630-low.rom lidetool/lidetool $(BUILDDIR)/lide.device $(BUILDDIR)/AIDE-lide.device $(BUILDDIR)/amigapci-lide.device
	@mkdir -p $(BUILDDIR)/lha/Expansion
	cp $^ $(BUILDDIR)/lha
	cp -r dist/* $(BUILDDIR)/lha
	awk '/^Version/{sub("0.0","${VERSION}")};{print}' dist/lide-update.readme > ${BUILDDIR}/lha/lide-update.readme
	mv $(BUILDDIR)/lha/lide.device $(BUILDDIR)/lha/Expansion/
	cd $(BUILDDIR)/lha && lha -c ../../$@ *

lha: $(BUILDDIR)/lide-update.lha 

$(BUILDDIR)/lide-tk-29F010.rom: $(ROM)
	@cat $(BUILDDIR)/lide-atbus.rom $(BUILDDIR)/lide-atbus.rom $(BUILDDIR)/lide-atbus.rom $(BUILDDIR)/lide-atbus.rom > $@

$(BUILDDIR)/lide-tk-29F020.rom: lide-tk-29F010.rom
	@cat $< $< > $@

$(BUILDDIR)/lide-tk-29F040.rom: lide-tk-29F020.rom
	@cat $< $< > $@

clean:
	@-rm -rf obj
	@-rm -rf $(BUILDDIR)
	@${MAKE} BUILDDIR=$(BUILDDIR) -C bootrom clean
	@${MAKE} -C lideflash clean
	@${MAKE} -C lidetool clean
	@${MAKE} BUILDDIR=$(BUILDDIR) -C aide-boot clean
