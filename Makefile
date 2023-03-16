PROJECT=liv2ride
CC=m68k-amigaos-gcc
CFLAGS=-Os -lamiga -nostartfiles -mcpu=68000 -fomit-frame-pointer -Wall -Wno-multichar -Wno-pointer-sign
.PHONY:	clean all
all:	$(PROJECT)

OBJ = driver.o \
      ata.o \
	  idetask.o

SRCS = $(OBJ:%.o=%.c) *.h

liv2ride: $(SRCS)	
	${CC} -o $@ $(CFLAGS) $(SRCS)

clean:
	-rm $(PROJECT)
