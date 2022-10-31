CC = gcc
CFLAGS = -Wall
LDLIBS = -lm

all: ops

ops: ops.o add.o mul.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)
	
ops.o: ops.c
	$(CC) $(CFLAGS) -c $^ -o $@ -D HAVE_MATH

add.o: add.c
	$(CC) $(CFLAGS) -c $^ -o $@
	
mul.o: mul.c
	$(CC) $(CFLAGS) -c $^ -o $@


clean:
	rm -rf *.o ops

