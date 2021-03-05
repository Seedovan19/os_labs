CC=gcc
CFLAGS=-Wall -Werror -Wpedantic
CLIBS=-pthread -lpthread

all: build

build: main.o
	$(CC) $(CFLAGS) $(CLIBS) main.c -o main
    
clean:
	rm -f main main.o
