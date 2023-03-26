PROJECT=liv2ride
CC=m68k-amigaos-gcc
DEBUG=0
CFLAGS=-nostartfiles -nostdlib -noixemul -mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign -DDEBUG=$(DEBUG) -g -O1

.PHONY:	clean all
all:	$(PROJECT)

OBJ = driver.o \
      ata.o \
	  idetask.o

SRCS = $(OBJ:%.o=%.c)

liv2ride: $(SRCS)	
	${CC} -o $@ $(CFLAGS) $(SRCS) -lamiga -lgcc -ldebug -lnix13

clean:
	-rm $(PROJECT)
