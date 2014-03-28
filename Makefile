NAME = sup2pgm

CROSS_COMPILE ?=
CC ?= gcc
CFLAGS ?= -O2
CFLAGS_EXTRA = -std=c99 -Wall

all: pgm.c srt.c sup.c sup2pgm.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $(CFLAGS_EXTRA) -o $(NAME) $^

.PHONY: clean
clean:
	-rm $(NAME)
	-rm *.o
