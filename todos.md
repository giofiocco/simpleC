make array access cast to ptr

abstract read function for compile_ir

typedef array
struct with array

test multiple returns

better documentation for ir
specify in ast_kind_t which ast_t.as use

# Tokenizer

- [ ] binary litterals
- [ ] escape char in string

# Parser

- [ ] int a,b;
- [ ] int \*\*a;
- [ ] struct point a; only in structdef?

# Typechecker

- [ ] if cond ptr
- [ ] warn if no return in some function
- [ ] return needs type VOID?
- [ ] alias of alias?

# Compiler

- [ ] compile_data array of str or ptr
- [ ] array with expression as length

# AST OPT

- [ ] 3+1 and 1 2 B_AH for data (or maybe in data_compile)

# IR OPT

- [ ] ADDR_LOCAL(x+z) READ(y) ADDR_LOCAL(y+z) READ(x) -> ADDR_LOCAL(z) READ(x+y)
      similar for global decl
- [ ] ADDR_LOCAL(2) READ(x) ADDR_LOCAL(y) WRITE(x) -> ADDR_LOCAL(y-x) WRITE(x)
      similar for global decl

# OPT

- [ ] remove ifs that do nothing
- [ ] remove dead or unreachable code
- [ ] A_SP SP_A -> A_SP
- [ ] ptail opt

# Roadmap

- [ ] inline asm
- [ ] macros
- [ ] include
- [ ] union
