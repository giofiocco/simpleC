all: simpleC test-file

CFLAGS=-Wall -Wextra -Werror -std=c99

.PHONY: test clean

simpleC: simpleC.c 
	cc $(CFLAGS) -o $@ $(filter %.c,$^) jaris/instructions.c jaris/files.c

test-file: test-file.c
	cc $(CFLAGS) -o $@ $<

test:
	find tests -type f | while read f; do sh $$f; done

clean:
	rm simpleC test-file
