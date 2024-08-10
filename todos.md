postopt both init and code
do better pointer math
astlist -> binary and semplify others
typedef array
struct with array
index as a *(...)
dereference an array
pointer aritmethics with array
index -> binary thing
parse with location_t start = tokenizer->loc;

# Tokenizer
- [ ] binary litterals
- [ ] escape char in string
- [ ] maybe find the row end when changing the row instead of finding it when printing error

# Parser
- [ ] int a,b;
- [ ] <index> -> <expr>[<expr>]
- [ ] int **a;

# Typechecker
- [ ] maybe check if code is if or something with a block and then call typecheck_funcbody on it
- [ ] warn if no return in some function

# Compiler
- [ ] array with expression as length in the stack

# PostOpt
- [ ] remove dead code

# Roadmap
- [x] inline asm
- [ ] macros
- [ ] include  
- [ ] union
