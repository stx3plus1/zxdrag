CC      = $(shell command -v clang || command -v cc || command -v gcc)
CFLAGS  = -O2 -std=gnu2x -Werror -march=native
LFLAGS  = $(shell pkg-config --cflags --libs xcb xcb-aux cairo)
PREFIX  = /usr/local

UTILS_FILES = $(wildcard *.c utils/*.c)
UTILS_EXECS = $(UTILS_FILES:.c=)

all: $(UTILS_EXECS)

%: %.c
	$(CC) $< -o $@ $(CFLAGS) $(LFLAGS)

install: $(UTILS_EXECS)
	mkdir -p $(PREFIX)/bin
	cp $^ $(PREFIX)/bin/

clean:
	rm -f $(UTILS_EXECS)
