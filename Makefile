all: simpleC

CFLAGS=-Wall -Wextra -Werror -std=c99

.PHONY: test test-record

simpleC: simpleC.c 
	cc $(CFLAGS) -o $@ $(filter %.c,$^) jaris/instructions.c

test: simpleC
	./rere.py replay test.list

test-record: simpleC
	./rere.py record test.list
