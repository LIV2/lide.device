PROJECT=liv2ride
CC=m68k-amigaos-gcc
DEBUG=0
CFLAGS=-nostartfiles -nostdlib -noixemul -mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign -DDEBUG=$(DEBUG) -O3 -fomit-frame-pointer -Wno-unused-value -fno-delete-null-pointer-checks

.PHONY:	clean all
all:	$(PROJECT)

OBJ = driver.o \
      ata.o \
	  idetask.o \
	  mounter.o

SRCS = $(OBJ:%.o=%.c)

liv2ride: $(SRCS)	
	${CC} -o $@ $(CFLAGS) $(SRCS) -lamiga -lgcc -ldebug -lnix13

clean:
	-rm $(PROJECT)
