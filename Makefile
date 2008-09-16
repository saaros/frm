CC=gcc
CFLAGS=-Os -g -Wall

all: frm

frm: frm.c
	$(CC) $(CFLAGS) -o frm frm.c

clean:
	rm -f frm *~
