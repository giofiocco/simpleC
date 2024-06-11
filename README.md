# Simple C

A compiler for a subset of `C`

# Tokens
- SYM: `[a-zA-Z_][a-zA-Z0-9_]*`
- INT: `[0-9]+`
- HEX: `0[xX]([0-9a-fA-F]{2} | [0-9a-fA-F]{4})`
- STRING: `"[^"]*"` 

# Grammar
- `<funcdecl> -> <type> <sym>() <block>`
- `<block> -> { <code>* }`
- `<code> -> <statement>`
- `<statement> -> return <expr>\?; | <type> <sym> \(= <expr>\)\?; | *\?<sym> = <expr>; <expr>;`
- `<expr> -> <term> \(+ <term> | - <term>\)\*`
- `<term> -> <unary> \(* <unary> | / <unary>\)\*`
- `<unary> -> + <fac> | - <fac> | <fac> | &<sym> | *<sym>`
- `<fac> -> <int> | <sym> | <string>`
- `<type> -> <sym>*\?`
