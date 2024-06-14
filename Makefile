CFLAGS=-Wall -Wextra -Werror -std=c99

.PHONY: run test test-record

simpleC: main.c
	cc $(CFLAGS) -o $@ $(filter %.c,$^) ../defs.c

run: simpleC
	./simpleC

test: simpleC
	./rere.py replay test.list

test-record: simpleC
	./rere.py record test.list

