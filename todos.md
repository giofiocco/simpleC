if token_peek and then token_next_if_kind the tokenizer->loc gives you the loc after the token you want
so maybe change to tokenizer_get_loc() that checks if there is token to peek

structdef parse_decl expect no expr

void v; should not be possible

typedef array
struct with array

tests errors

maybe do better ir and keep track of SP in compile_ir?

# Tokenizer

- [ ] binary litterals
- [ ] escape char in string

# Parser

- [ ] in funcdef etc sv_t instead of token? (maybe not)
- [ ] int \*\*a;
- [ ] struct point a; only in structdef?

# Typechecker

- [ ] warn if no return in some function
- [ ] alias of alias?

# Compiler

- [ ] proper compilation for CALL vs CALLR
- [ ] compile_data array of str or ptr
- [ ] array with expression as length
- [ ] maybe merge \_start and main

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

# README

- [ ] add ast
- [ ] add ir args

# Roadmap

- [ ] macros
- [ ] include
- [ ] union
