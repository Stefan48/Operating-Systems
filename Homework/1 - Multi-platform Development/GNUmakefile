CC = gcc
CFLAGS = -Wall

all: build

build: so-cpp

so-cpp: main.o hashmap.o
	$(CC) $^ -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $<
	
hashmap.o: hashmap.c
	$(CC) $(CFLAGS) -c $<
	
.PHONY: clean
clean:
	rm -f *.o so-cpp
