CC      = clang
CFLAGS  = -Og -g -std=gnu2x -Werror
LFLAGS  = -lX11 $(shell pkg-config --cflags --libs xcb xcb-aux)
PREFIX  = /usr/local

all: zxdrag

zxdrag: wm.c
	@$(CC) $< -o $@ $(CFLAGS) $(LFLAGS)

install: zxdrag
	@mkdir -p $(PREFIX)/bin
	@cp $^ $(PREFIX)/bin/

clean: zxdrag 
	@rm -f $^
