CC = gcc
CFLAGS = -Wall -Wextra

all: build

build: libscheduler.so

libscheduler.so: so_scheduler.o
	$(CC) $(CFLAGS) -shared $^ -o $@
	
so_scheduler.o: so_scheduler.c
	$(CC) $(CFLAGS) -fPIC -lpthread -c $<
	
.PHONY: clean
clean:
	rm -f *.o *.so
