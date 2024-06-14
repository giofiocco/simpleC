CFLAGS=-Wall -Wextra -Werror -std=c99

.PHONY: run test test-record

simpleC: main.c ../defs.h ../defs.c
	cc $(CFLAGS) -o $@ $(filter %.c,$^)

test: simpleC
	./rere.py replay test.list

test-record: simpleC
	./rere.py record test.list

