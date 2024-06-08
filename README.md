# Simple C

A compiler for a subset of `C`

# Tokens
- SYM: `[a-zA-Z_][a-zA-Z0-9_]*`
- INT: `[0-9]+`

# Grammar
- `<funcdecl> -> <sym> <sym>() <block>`
- `<block> -> { <code>* }`
- `<code> -> <statement>`
- `<statement> -> return <expr>\?; | <sym> <sym> \(= <expr>\)\?;`
- `<expr> -> <term> \(+ <term> | - <term>\)\*`
- `<term> -> <unary> \(* <unary> | / <unary>\)\*`
- `<unary> -> + <fac> | - <fac> | <fac>`
- `<fac> -> <int> | <sym>`
