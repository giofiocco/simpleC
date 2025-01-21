INCB

typedef array
struct with array

better documentation for ir
specify in ast_kind_t which ast_t.as use

tests errors
tests optimizzations

# Tokenizer

- [ ] binary litterals
- [ ] escape char in string

# Parser

- [ ] in funcdef etc sv_t instead of token?
- [ ] int a,b;
- [ ] int \*\*a;
- [ ] struct point a; only in structdef?

# Typechecker

- [ ] if cond ptr
- [ ] warn if no return in some function
- [ ] return needs type VOID?
- [ ] alias of alias?

# Compiler

- [ ] proper compilation for CALL vs CALLR
- [ ] compile_data array of str or ptr
- [ ] array with expression as length

# AST OPT

- [ ] 3+1 and 1 2 B_AH for data (or maybe in data_compile)

# IR OPT

- [ ] ADDR_LOCAL(x+z) READ(y) ADDR_LOCAL(y+z) READ(x) -> ADDR_LOCAL(z) READ(x+y)
      similar for global decl
- [ ] ADDR_LOCAL(2) READ(x) ADDR_LOCAL(y) WRITE(x) -> ADDR_LOCAL(y-x) WRITE(x)
      similar for global decl
- [ ] ADDR INT MUL -> ADDR

# OPT

- [ ] condition with constants (like if (2) {} else {})
- [ ] ptail opt
- [ ] remove ifs that do nothing
- [ ] remove dead or unreachable code
- [ ] A_SP SP_A -> A_SP
- [ ] RAM_A A_B -> RAM_B

# README

- [ ] add ast
- [ ] add ir args

# Roadmap

- [ ] inline asm
- [ ] macros
- [ ] include
- [ ] union
