# Compiler settings
CC = gcc
CFLAGS = -Wall -O0 -g
LDFLAGS = -lpthread

TARGET = mapreduce tests

# Default target
all: $(TARGET)

# TODO: this is obviously not an optimal Makefile. But it works.
# Linking the executable mapreduce
mapreduce: framework.c file.c partition.c bst.c sort.c
	$(CC) $(CFLAGS) -c framework.c -o framework.o
	$(CC) $(CFLAGS) -c file.c -o file.o
	$(CC) $(CFLAGS) -c partition.c -o partition.o
	$(CC) $(CFLAGS) -c bst.c -o bst.o
	$(CC) $(CFLAGS) -c sort.c -o sort.o
	$(CC) sort.o bst.o framework.o file.o partition.o -o $@ $(LDFLAGS)
	rm -f framework.o file.o tests.o partition.o bst.o sort.o

# Linking the executable tests with UNIT_TEST defined
tests: CFLAGS += -DUNIT_TEST
tests: tests.c file.c framework.c partition.c sort.c bst.c
	$(CC) $(CFLAGS) -c tests.c -o tests.o
	$(CC) $(CFLAGS) -c file.c -o file.o
	$(CC) $(CFLAGS) -c framework.c -o framework.o
	$(CC) $(CFLAGS) -c partition.c -o partition.o
	$(CC) $(CFLAGS) -c sort.c -o sort.o
	$(CC) $(CFLAGS) -c bst.c -o bst.o
	$(CC) bst.o sort.o tests.o file.o framework.o partition.o -o $@ $(LDFLAGS)
	rm -f framework.o file.o tests.o partition.o sort.o bst.o

# Generic rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleaning up the build
clean:
	rm -f $(TARGET) framework.o file.o tests.o

# Phony targets
.PHONY: all clean