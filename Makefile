ifndef FFINSTALL
FFINSTALL=/usr
endif

CC = gcc
CFLAGS ?= -Iinclude -I/usr/include/libdrm -I$(FFINSTALL)/include/arm-linux-gnueabihf  -W -Wall -Wextra -g -O2 -std=c11
LDFLAGS	?=
LIBS	:= -lGLESv2 -lglfw -lEGL -L$(FFINSTALL)/lib/arm-linux-gnueabihf -lavcodec -lavutil -lavformat -ldrm -lpthread

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: glHevc

glHevc: glHevc.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f *.o
	-rm -f glHevc
