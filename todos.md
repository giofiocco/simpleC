type size into the type
make array access cast to ptr
read function for compile_ir

remove atoi

maybe struct ir_list and bytecode_list

typedef array
struct with array

test multiple returns

# Tokenizer

- [ ] binary litterals
- [ ] escape char in string
- [ ] maybe find the row end when changing the row instead of finding it when printing error

# Parser

- [ ] code block
- [ ] int a,b;
- [ ] int \*\*a;

# Typechecker

- [ ] maybe check if code is if or something with a block and then call typecheck_funcbody on it
- [ ] warn if no return in some function
- [ ] - - between char
- [ ] return needs type VOID?

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
