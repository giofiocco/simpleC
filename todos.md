## do better pointer math:

- [x] in typecheck array + int ~> ptr
- [x] in compile binaryop if the type is ptr shl the number
- [x] dereference an array
- [x] pointer aritmethics with array

---

code block

compile asm

postopt both init and code
astlist -> binary and semplify others
typedef array
struct with array
parse with location_t start = tokenizer->loc;

# Wierd output

- `char a[2] = {0x23, 0x10 + 0x01};`

# Tokenizer

- [ ] binary litterals
- [ ] escape char in string
- [ ] maybe find the row end when changing the row instead of finding it when printing error

# Parser

- [ ] cast a string in globdecl?
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

- [ ] remove dead code
- [ ] A_SP SP_A -> A_SP

# Roadmap

- [ ] inline asm
- [ ] macros
- [ ] include
- [ ] union
