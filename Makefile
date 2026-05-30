CC = gcc
CFLAGS = -O0 -Wall

all: cache_bench

cache_bench: cache_bench.c
	$(CC) $(CFLAGS) -o cache_bench cache_bench.c -lm

clean:
	rm -f cache_bench results.txt *.png
