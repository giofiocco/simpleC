compile asm

forerror -> loc
datauli(compiled, state) -> datauli(state)
type size into the type
make array access cast to ptr

maybe my own bytecode compiled to asm file
typedef array
struct with array

test multiple returns

# Tokenizer

- [ ] location.row_start -> sv_t location.line with the len of the line
- [ ] binary litterals
- [ ] escape char in string
- [ ] maybe find the row end when changing the row instead of finding it when printing error

# Parser

- [ ] code block
- [ ] int a,b;
- [ ] int \*\*a;
- [ ] check proper location print when error

# Typechecker

- [ ] maybe check if code is if or something with a block and then call typecheck_funcbody on it
- [ ] warn if no return in some function
- [ ] - - between char

# Compiler

- [ ] compile_data array of str or ptr
- [ ] array with expression as length

# PostOpt

- [ ] both init and code
- [ ] remove dead code
- [ ] A_SP SP_A -> A_SP

# Roadmap

- [ ] inline asm
- [ ] macros
- [ ] include
- [ ] union
