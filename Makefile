CC ?= cc
CFLAGS ?= -Os -g -Wall
SRC = frm.c utf8.c

all: frm

frm: $(SRC)
	$(CC) $(CFLAGS) -o frm $(SRC)

clean:
	rm -f frm *~
