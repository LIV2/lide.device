PROJECT=renamelide
CC=m68k-amigaos-gcc
CFLAGS=-lamiga -mcrt=nix13 -mcpu=68000 -Wall -Wno-pointer-sign -Wno-multichar -g -O3 -fomit-frame-pointer
.PHONY:	clean all

all:	$(PROJECT)

OBJ = renamelide.o

SRCS = $(OBJ:%.o=%.c)

renamelide: $(SRCS)
	${CC} -o $@ $(CFLAGS) $(SRCS)

clean:
	-rm -rf $(PROJECT)
