# Simple C

A compiler for a subset of `C`

# Tokens
- SYM: `[a-zA-Z_][a-zA-Z0-9_]*`
- INT: `[0-9]+`
- HEX: `0[xX]([0-9a-fA-F]{2} | [0-9a-fA-F]{4})`
- STRING: `"[^"]*"` 

# Grammar
- `<funcdecl> -> <type> <sym><paramdef> <block>`
- `<paramdef> -> (\(<type> <sym> \(, <type> <type>\)\*\)\?)`
- `<block> -> { <code>* }`
- `<code> -> <statement>`
- `<statement> -> return <expr>\?; | <type> <sym> \(= <expr>\)\?; | *\?<sym> = <expr>; <expr>;`
- `<expr> -> <term> \(+ <term> | - <term>\)\*`
- `<term> -> <unary> \(* <unary> | / <unary>\)\*`
- `<unary> -> + <fac> | - <fac> | <fac> | &<sym> | *<sym>`
- `<fac> -> <int> | <sym> | <string> | <funccall>`
- `<funccall> -> <sym>(\(<expr> \(, <expr>\)\*\)\?)`
- `<type> -> <sym>*\?`
