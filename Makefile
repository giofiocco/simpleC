CFLAGS=-Wall -Wextra -Werror -std=c99

simpleC: main.c
	cc $(CFLAGS) -o $@ $(filter %.c,$^) ../defs.c

run: simpleC
	./simpleC
