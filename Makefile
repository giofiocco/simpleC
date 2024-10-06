all: simpleC 

CFLAGS=-Wall -Wextra -Werror -std=c99

.PHONY: test clean

simpleC: simpleC.c 
	cc -Wno-infinite-recursion $(CFLAGS) -o $@ $(filter %.c,$^) jaris/instructions.c jaris/files.c

test:
	find tests -type f | while read f; do sh $$f; done

clean:
	rm simpleC test-file
