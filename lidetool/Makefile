PROJECT=lidetool
CC=m68k-amigaos-gcc
CFLAGS=-lamiga -mcrt=nix20 -mcpu=68000 -Wall -Wno-pointer-sign -g -O3 -fomit-frame-pointer
.PHONY:	clean all

all:	$(PROJECT)

OBJ = config.o \
	  main.o

SRCS = $(OBJ:%.o=%.c)


lidetool: $(SRCS) *.h
	${CC} -o $@ $(CFLAGS) $(SRCS)

clean:
	-rm $(PROJECT)
