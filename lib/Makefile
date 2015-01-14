CFLAGS=-ggdb3 -O0 -Wall -Werror --std=gnu99 -D_DEFAULT_SOURCE -fPIC
INCLUDES=-I../include -I/usr/local/include
LIBS=
PTHREAD=
CC=clang

OBJS=		rk_array.o		\
		rk_command.o		\
		rk_config.o		\
		rk_epoch.o		\
		rk_sema.o		\
		rk_state.o		\
		rk_state_handler.o	\
		raikkonen.o		\
		finnish.o

.PHONY: all clean
all: libraikkonen.so libraikkonen.a

clean:
	rm -rf $(OBJS) libraikkonen.a libraikkonen.so

raikkonen.o: raikkonen.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o raikkonen.o raikkonen.c

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

libraikkonen.a: $(OBJS)
	ar rcs $@ $^

libraikkonen.so: $(OBJS)
	$(CC) $(INCLUDES) $^ -fPIC -shared -o libraikkonen.so $(LIBS) $(PTHREAD)
