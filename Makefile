all: benchmarks

CFLAGS := -std=gnu99 -D_GNU_SOURCE -g -Og -lrt -pthread

benchmarks: benchmarks.c
	$(CC) $(CFLAGS) $^ -o $@
