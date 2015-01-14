.PHONY: all lib clean check
all: lib check

lib:
	make -C lib

clean:
	make -C lib clean
	make -C tests clean

check: lib
	make -C tests check
