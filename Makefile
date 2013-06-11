NAME = sup2pgm

CC = gcc
CFLAGS =

all: pgm.c srt.c sup.c sup2pgm.c
	$(CC) $(CFLAGS) -o $(NAME) $^

.PHONY: clean
clean:
	-rm *.o
