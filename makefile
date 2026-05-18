CC      = $(shell command -v clang || command -v cc || command -v gcc)
CFLAGS  = -O2 -std=gnu2x -Werror -march=native
LFLAGS  = $(shell pkg-config --cflags --libs xcb xcb-aux)
PREFIX  = /usr/local

all: zxdrag

zxdrag: wm.c
	$(CC) $< -o $@ $(CFLAGS) $(LFLAGS)

install: zxdrag
	mkdir -p $(PREFIX)/bin
	cp $^ $(PREFIX)/bin/

clean: zxdrag 
	rm -f $^
