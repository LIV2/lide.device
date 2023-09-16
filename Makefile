PROJECT=lide.device
BUILDDIR=build
DISK=lide-update.adf
ROM=lide.rom
CC=m68k-amigaos-gcc
CFLAGS=-nostartfiles -nostdlib -noixemul -mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign -Wno-attributes  -Wno-unused-value -s -Os -fomit-frame-pointer -DCDBOOT=1 -DNO_RDBLAST=1
LDFLAGS=-lamiga -lgcc -lc
AS=m68k-amigaos-as

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

.PHONY:	clean all lideflash disk lha
all:	$(ROM) lideflash

OBJ = driver.o \
      ata.o \
	  atapi.o \
	  scsi.o \
	  idetask.o \
	  mounter.o

ASMOBJ = endskip.o

SRCS = $(OBJ:%.o=%.c)
SRCS += $(ASMOBJ:%.o=%.S)

$(PROJECT): $(SRCS)
	${CC} -o $@ $(CFLAGS) $(SRCS) $(LDFLAGS)

$(ROM): $(PROJECT)
	make -C bootrom

lideflash/lideflash:
	make -C lideflash

lideflash: lideflash/lideflash

disk: $(ROM) lideflash/lideflash
	@mkdir -p $(BUILDDIR)
	cp $(ROM) build
	echo -n 'lideflash -I $(ROM)\n' > $(BUILDDIR)/startup-sequence
	xdftool $(BUILDDIR)/$(DISK) format lide-update + \
	                            boot install boot1x + \
	                            write $(ROM) + \
	                            write lideflash/lideflash lideflash + \
	                            makedir s + \
	                            write $(BUILDDIR)/startup-sequence s/startup-sequence

$(BUILDDIR)/lide-update.lha: lideflash/lideflash $(ROM)
	@mkdir -p $(BUILDDIR)
	cp $^ $(BUILDDIR)
	cd $(BUILDDIR) && lha -c ../$@ $(notdir $^) 

lha: $(BUILDDIR)/lide-update.lha

clean:
	-rm $(PROJECT)
	make -C bootrom clean
	make -C lideflash clean
	-rm -rf $(BUILDDIR)
