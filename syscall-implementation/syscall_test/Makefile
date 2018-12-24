# DO NOT TOUCH THIS
CC := $(NDK_PATH)/clang

CFLAGS = -g -Wall
LDFLAGS = -g

# NOTE:
#   Use $(CC) instead of compiler name (e.g. gcc, clang).
#   Since your user-space program is simple, $(CC) is able to handle
#   everything. Do not use other executables for compiling/linking.

# ---------- Write the rest of Makefile below ----------

test: test.o
	$(CC) $(LDFLAGS) -o test test.c

test.o: test.c
	$(CC) $(CFLAGS) -c test.c

.PHONY: clean
clean:
	rm -f *.o test

.PHONY: all
all: clean test
