all: simpleC

CFLAGS=-Wall -Wextra -std=c99 -g

.PHONY: test clean

simpleC: simpleC.c
	cc -Wno-infinite-recursion $(CFLAGS) -o $@ $(filter %.c,$^) jaris/instructions.c jaris/files.c

test: all
	perl test.pl $(wildcard tests/*)

test-%: all
	perl test.pl $(patsubst test-%,tests/%,$@)

record-test-%: all
	perl test.pl -r $(patsubst record-test-%,tests/%,$@)

clean:
	rm simpleC
