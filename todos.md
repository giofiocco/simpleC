compile asm

typedef array
struct with array

# Wierd output

- `char a[2] = {0x23, 0x10 + 0x01};`

# Tokenizer

- [ ] binary litterals
- [ ] escape char in string
- [ ] maybe find the row end when changing the row instead of finding it when printing error

# Parser

- [ ] code block
- [ ] is cast necessary?
- [ ] int a,b;
- [ ] <index> -> <expr>[<expr>]
- [ ] int \*\*a;

# Typechecker

- [ ] maybe check if code is if or something with a block and then call typecheck_funcbody on it
- [ ] warn if no return in some function

# Compiler

- [ ] compile_data array of str or ptr
- [ ] array with expression as length in the stack
- [ ] return array

# PostOpt

- [ ] both init and code
- [ ] remove dead code
- [ ] A_SP SP_A -> A_SP

# Roadmap

- [ ] inline asm
- [ ] macros
- [ ] include
- [ ] union
