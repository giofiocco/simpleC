# Examples

The `src` folder contains some simpleC examples,
use `make` to compile everything and `make run prg=<name> args=<args-for-jaris/sim>` to run both the version compiled with `cc` and with `simpleC`.
`clib_wrapper.c` is the wrapper for the `cc` version, a wrapper for the gnu c stdlib.
`c_stdlib.asm` is the wrapper for the jaris stdlib becasuse of the different calling method of functions in simpleC and what the jaris stdlib expects.

To test run `make test` optionally with `prg=<name>` and/or `args=<args-for-jaris/sim>`,
it will run the `cc` and `simpleC` versions, collect the output and compare it.

## List:

1. rule110: [Rule 110 cellular automaton](https://en.wikipedia.org/wiki/Rule_110)
2. gol: [Conway's game of life](https://en.wikipedia.org/wiki/Conway%27s_Game_of_Life)
