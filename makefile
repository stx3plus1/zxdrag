CC      = $(shell command -v clang || command -v cc || command -v gcc)
CFLAGS  = -O2 -std=gnu2x -Werror -march=native
LFLAGS  = $(shell pkg-config --cflags --libs xcb xcb-aux cairo)
PREFIX  = /usr/local

FILES = $(wildcard *.c utils/*.c)
EXECS = $(FILES:.c=)

all: $(EXECS)

%: %.c
	$(CC) $< -o $@ $(CFLAGS) $(LFLAGS)

install: $(EXECS)
	mkdir -p $(PREFIX)/bin
	cp $^ $(PREFIX)/bin/

clean:
	rm -f $(EXECS)
