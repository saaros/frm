CC=gcc
CFLAGS=-O2 -g -pedantic -Wall

all: frm

frm: frm.c
	$(CC) $(CFLAGS) -o frm frm.c

clean:
	rm -f frm *~
