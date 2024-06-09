# Simple C

A compiler for a subset of `C`

# Tokens
- SYM: `[a-zA-Z_][a-zA-Z0-9_]*`
- INT: `[0-9]+`

# Grammar
- `<funcdecl> -> <type> <sym>() <block>`
- `<block> -> { <code>* }`
- `<code> -> <statement>`
- `<statement> -> return <expr>\?; | <type> <sym> \(= <expr>\)\?;`
- `<expr> -> <term> \(+ <term> | - <term>\)\*`
- `<term> -> <unary> \(* <unary> | / <unary>\)\*`
- `<unary> -> + <fac> | - <fac> | <fac> | &<sym>`
- `<fac> -> <int> | <sym>`
- `<type> -> <sym>*\?`
