CC=gcc
CFLAGS=-Wall
LDFLAGS=-lm

.PHONY: clean

all: mypipeline myshell

mypipeline: mypipeline.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

myshell: myshell.c LineParser.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f mypipeline myshell
