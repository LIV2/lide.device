PROJECT=lideflash
CC=m68k-amigaos-gcc
CFLAGS=-lamiga -mcrt=nix13 -mcpu=68000 -Wall -Wno-pointer-sign -g -O3 -fomit-frame-pointer
.PHONY:	clean all

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
