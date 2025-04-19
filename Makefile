CC = gcc
CFLAGS = -Wall -Werror
LDFLAGS = -lm

.PHONY: all clean

all: libmlpt.a

libmlpt.a: mlpt.o
	ar rcs $@ $^

mlpt.o: mlpt.c mlpt.h config.h
	$(CC) $(CFLAGS) $(LDFLAGS) -c $< -o $@

clean:
	rm -f *.o *.a test_mlpt