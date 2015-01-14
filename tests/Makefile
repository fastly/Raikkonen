CFLAGS=-ggdb3 -O0 -Wall -Werror --std=gnu99 -DRK_ENABLED
INCLUDES=-I../include -I/usr/local/include
LIBS=../lib/libraikkonen.a
PTHREAD=-lpthread

.PHONY: all clean check

all: test

clean:
	rm -rf test out.fi test.out

test: test.c
	gcc $(CFLAGS) $(INCLUDES) test.c -o test $(LIBS) $(PTHREAD)

check: test
	../bin/kimi.pl
	./test > test.out 2>&1 &
	../bin/fi_client.pl
	diff test.out out.expect

