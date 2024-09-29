all: simpleC

CFLAGS=-Wall -Wextra -Werror -std=c99

.PHONY: test test-record clean

simpleC: simpleC.c 
	cc $(CFLAGS) -o $@ $(filter %.c,$^) jaris/instructions.c jaris/files.c

test: simpleC
	./rere.py replay test.list

test-record: simpleC
	./rere.py record test.list

clean:
	rm simpleC 
