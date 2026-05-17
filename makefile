CC      = $(shell command -v clang || command -v cc)
CFLAGS  = -O2 -std=gnu2x -Werror
LFLAGS  = $(shell pkg-config --cflags --libs xcb xcb-aux)
PREFIX  = /usr/local

all: zxdrag

zxdrag: wm.c
	@$(CC) $< -o $@ $(CFLAGS) $(LFLAGS)

install: zxdrag
	@mkdir -p $(PREFIX)/bin
	@cp $^ $(PREFIX)/bin/

clean: zxdrag 
	@rm -f $^
