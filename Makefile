PROJECT=liv2ride
CC=m68k-amigaos-gcc
CFLAGS=-nostartfiles -nostdlib -noixemul -mcpu=68000 -Wall -Wno-multichar -Wno-pointer-sign  -Wno-unused-value -Og -fomit-frame-pointer -fno-delete-null-pointer-checks -msmall-code -s
LDFLAGS=-lamiga -lgcc -lc
AS=m68k-amigaos-as

ifdef DEBUG
CFLAGS+= -DDEBUG=$(DEBUG)
LDFLAGS+= -ldebug
.PHONY: $(PROJECT)
endif

LDFLAGS+= -lnix13

.PHONY:	clean all
all:	$(PROJECT)

OBJ = driver.o \
      ata.o \
	  idetask.o \
	  mounter.o

ASMOBJ = endskip.o

SRCS = $(OBJ:%.o=%.c)
SRCS += $(ASMOBJ:%.o=%.S)

$(PROJECT): $(SRCS)
	${CC} -o $@ $(CFLAGS) $(SRCS) $(LDFLAGS)

clean:
	-rm $(PROJECT)
