CC = gcc
CFLAGS = -Wall -Wextra

all: build

build: libso_stdio.so
	
libso_stdio.so: so_stdio.o
	$(CC) $(CFLAGS) -shared $^ -o $@
	
so_stdio.o: so_stdio.c
	$(CC) $(CFLAGS) -fPIC -c $<
	
.PHONY: clean
clean:
	rm -f *.o *.so
