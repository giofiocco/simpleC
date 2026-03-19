1. in optimize_ast a flag with is_condition and then do all the optimizzations
    if an ast subtree is a condition you can do all the optimization
    otherwise make a function optimize_condition_ast
1. in optimize ast first a switch with all the recursive calls and then a switch with optimizations
1. '\n'
1. continue should goto the update statement
1. maybe in grammar atom EQ atom should be atom EQ expr? etc
1. a separate compiled field only for externs and globals
1. when error on param type instead of printing PARAM{...} print the type

fix error when c_put_char(<int>);

if token_peek and then token_next_if_kind the tokenizer->loc gives you the loc after the token you want
so maybe change to tokenizer_get_loc() that checks if there is token to peek

test array of ptr

tests errors

semplify opt_ast output

# Tokenizer

- [ ] binary litterals

# Parser

- [ ] in funcdef etc sv_t instead of token? (maybe not)
- [ ] int \*\*a;
- [ ] struct point a; only in structdef?

# Typechecker

- [ ] warn if no return in some function
- [ ] alias of alias?

# Compiler

- [ ] proper compilation for CALL vs CALLR
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

- [ ] include
- [ ] union
