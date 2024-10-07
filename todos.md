compile asm

array of struct
typedef array
struct with array

# Tokenizer

- [ ] location.row_start -> sv_t location.line with the len of the line
- [ ] binary litterals
- [ ] escape char in string
- [ ] maybe find the row end when changing the row instead of finding it when printing error

# Parser

- [ ] code block
- [ ] is cast necessary in decl?
- [ ] int a,b;
- [ ] <index> -> <expr>[<expr>]
- [ ] int \*\*a;

# Typechecker

- [ ] maybe check if code is if or something with a block and then call typecheck_funcbody on it
- [ ] warn if no return in some function
- [ ] operations between char

# Compiler

- [ ] compile_data array of str or ptr
- [ ] array with expression as length in the stack

# PostOpt

- [ ] both init and code
- [ ] remove dead code
- [ ] A_SP SP_A -> A_SP

# Roadmap

- [ ] inline asm
- [ ] macros
- [ ] include
- [ ] union
