# Compiler settings
CC = gcc
CFLAGS = -Wall -O2 -g
LDFLAGS = -lpthread


TARGET = mapreduce tests

# Common source files
SRCS = framework.c file.c partition.c bst.c sort.c
TEST_SRCS = tests.c

OBJS = $(SRCS:.c=.o)
TEST_OBJS = $(SRCS:.c=_test.o) tests.o


all: $(TARGET)

# Targets
mapreduce: $(OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)

tests: $(TEST_OBJS)
	$(CC) $^ -o $@ $(LDFLAGS)


# Generic rule for object files for mapreduce
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# Generic rule for object files for tests
%_test.o: %.c
	$(CC) $(CFLAGS) -DUNIT_TEST -c $< -o $@

tests.o: tests.c
	$(CC) $(CFLAGS) -DUNIT_TEST -c $< -o $@


clean:
	rm -f mapreduce tests $(OBJS) $(TEST_OBJS)

# Phony targets
.PHONY: all clean