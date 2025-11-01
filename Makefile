CC=gcc
CFLAGS=-O2 -std=c11 -Wall -Wextra

all: main

main: main.c
	$(CC) $(CFLAGS) -o main main.c

clean:
	del /Q main.exe 2> NUL || true
	rm -f main 2> /dev/null || true
