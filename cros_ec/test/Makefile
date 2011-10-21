# A temporary and big makefile to compile all.
CC=cc
CFLAGS=-g -I../../ -Wall -Werror
LD=$(CC)
LDFLAGS=
RM=rm

.SUFFIXES:
.SUFFIXES: .o .c

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

all: main_linux_test

main_linux_test: main_linux_test.o ../main.o ../keyboard.o \
                 ../chip_stub/keyboard.o
	$(LD) -o $@ $^

clean:
	$(RM) -f main_linux_test
	find ../../ -name "*.o" -delete
