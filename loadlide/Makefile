PROJECT=loadlide
CC=m68k-amigaos-gcc
CFLAGS=-lamiga -mcrt=nix20 -mcpu=68000 -Wall -Wno-pointer-sign -g -O3 -fomit-frame-pointer
.PHONY:	clean all

all:	$(PROJECT)

OBJ = main.o

SRCS = $(OBJ:%.o=%.c)


$(PROJECT): $(SRCS)
	${CC} -o $@ $(CFLAGS) $(SRCS)

clean:
	-rm $(PROJECT)
