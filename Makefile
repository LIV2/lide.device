PROJECT=lide.device
BUILDDIR=build
ROM=lide.rom
VERSION := $(shell git describe --tags --dirty | sed -r 's/^Release-//')

GIT_REF_NAME = $(shell git branch --show-current)
GIT_REF := "$(GIT_REF_NAME)-$(shell git rev-parse --short HEAD)"
BUILD_DATE := $(shell date  +"%d.%m.%Y")

export BUILD_DATE
export GIT_REF

CC=m68k-amigaos-gcc
CFLAGS+=-nostartfiles -nostdlib -mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign -Wno-unused-value -s -Os -fomit-frame-pointer -DCDBOOT=1 -DNO_RDBLAST=1
CFLAGS+=-DGIT_REF=$(GIT_REF) -DBUILD_DATE=$(BUILD_DATE)
LDFLAGS=-lc
AS=m68k-amigaos-as

ifeq ($(shell uname),Darwin)
GREP=ggrep
else
GREP=grep
endif

ifneq ($(VERSION),)
DISK=lide-update-$(VERSION).adf
DEVICE_VERSION=$(shell echo $(VERSION) | $(GREP) -oP '^(\w+-)?\K\d+')
DEVICE_REVISION=$(shell echo $(VERSION) | $(GREP) -oP '^(\w+-)?\d+\.\K\d+')
CFLAGS+=-DDEVICE_VERSION=$(DEVICE_VERSION) -DDEVICE_REVISION=$(DEVICE_REVISION)

export DEVICE_REVISION
export DEVICE_VERSION

else
DISK=lide-update.adf
endif

ifdef DEBUG
CFLAGS+= -DDEBUG=$(DEBUG)
LDFLAGS=-ldebug -lgcc -lc
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

ifdef SIMPLE_IDE
CFLAGS+= -DSIMPLE_IDE=1
.PHONY: $(PROJECT)
endif

ifdef AMIGAPCI
CFLAGS+= -DAMIGAPCI=1
.PHONY: $(PROJECT)
endif

.PHONY:	clean all lideflash disk lha lidetool/lidetool mounter

all:	$(ROM) \
		lideflash 

OBJ = device.o \
      ata.o \
	  scsi.o \
	  iotask.o \
	  lide_alib.o \
	  debug.o \


ASMOBJ = endskip.o

SRCS = $(OBJ:%.o=%.c)
SRCS += $(ASMOBJ:%.o=%.S)

mounter:
	make -C mounter

$(PROJECT): $(SRCS)	mounter
	${CC} -o $@ $(CFLAGS) $(SRCS) mounter/obj/loadseg.o mounter/obj/mounter.o $(LDFLAGS)

$(ROM): $(PROJECT)
	make -C bootrom

lideflash/lideflash:
	make -C lideflash

lideflash: lideflash/lideflash

lidetool/lidetool:
	make -C lidetool

disk:	$(BUILDDIR)/$(DISK) $(BUILDDIR)/AIDE-boot-$(VERSION).adf

$(BUILDDIR)/$(DISK): $(ROM) lideflash/lideflash lidetool/lidetool AIDE-lide.device
	@mkdir -p $(BUILDDIR)
	cp $(ROM) build
	echo -n 'lideflash -I $(ROM)\n' > $(BUILDDIR)/startup-sequence
	xdftool $(BUILDDIR)/$(DISK) format lide-update + \
	                            boot install + \
	                            write $(ROM) + \
	                            write lidetool/lidetool lidetool + \
	                            write lideflash/lideflash lideflash + \
	                            makedir s + \
	                            write $(BUILDDIR)/startup-sequence s/startup-sequence + \
	                            makedir Expansion + \
	                            write info/Expansion.info Expansion.info + \
	                            write info/lide.device.info Expansion/lide.device.info + \
	                            write lide.device Expansion/lide.device

$(BUILDDIR)/lide-update.lha: lideflash/lideflash $(ROM) lidetool/lidetool lide.device info/lide.device.info AIDE-lide.device
	@mkdir -p $(BUILDDIR)
	cp $^ $(BUILDDIR)
	cd $(BUILDDIR) && lha -c ../$@ $(notdir $^) 

lha: $(BUILDDIR)/lide-update.lha 

clean:
	-rm -f $(PROJECT)
	make -C mounter clean
	make -C bootrom clean
	make -C lideflash clean
	make -C lidetool clean
	-rm -rf *.rom
	-rm -rf $(BUILDDIR)
