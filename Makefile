CC=gcc
COPTS=-O3 -fshort-enums -Wall

all: mathler-EASY mathler-NORMAL mathler-HARD mathler-NUMBLE mathler-THENUMBLE

mathler-%: mathler.c Makefile
	$(CC) -o $@ $(COPTS) $< CBack-1.0/SRC/CBack.c -D$* 
