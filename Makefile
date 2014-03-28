CROSS_COMPILE ?=
CC ?= gcc
CFLAGS ?= -O2
CFLAGS_REQ = -std=c99 -Wall

all: pgm.c srt.c sup.c sup2pgm.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(CFLAGS_REQ) -o sup2pgm $^

.PHONY: clean
clean:
	-rm sup2pgm
	-rm *.o
