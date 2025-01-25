PROJECT=lideflash
CC=m68k-amigaos-gcc
CFLAGS=-lamiga -mcrt=nix13 -mcpu=68000 -Wall -Wno-pointer-sign -s -O3 -fomit-frame-pointer
.PHONY:	clean all

ifneq ($(DEVICE_VERSION), )
CFLAGS+=-DDEVICE_VERSION=$(DEVICE_VERSION) -DDEVICE_REVISION=$(DEVICE_REVISION)
CFLAGS+=-DGIT_REF=$(GIT_REF) -DBUILD_DATE=$(BUILD_DATE)
endif

all:	$(PROJECT)

OBJ = matzetk.o \
	flash.o \
	config.o \
	main.o

SRCS = $(OBJ:%.o=%.c)


lideflash: $(SRCS) *.h
	${CC} -o $@ $(CFLAGS) $(SRCS)

clean:
	-rm -rf $(PROJECT)
