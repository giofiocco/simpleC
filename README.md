# Simple C

A compiler for a subset of `C`

# Tokens
- SYM: `[a-zA-Z_][a-zA-Z0-9_]*`
- INT: `[0-9]+`
- HEX: `0[xX]([0-9a-fA-F]{2} | [0-9a-fA-F]{4})`
- STRING: `"[^"]*"` 

# Grammar
- `<global>` &rarr; `<funcdecl>\* | <decl>`
- `<funcdecl>` &rarr; `<type> <sym><paramdef> <block>`
- `<paramdef>` &rarr; `(\(<type> <sym> \(, <type> <type>\)\*\)\?)`
- `<block>` &rarr; `{ <code>* }`
- `<code>` &rarr; `<statement>`
- `<statement>` &rarr; `return <expr>\?; | <decl> | (<sym> | *<expr>) = <expr>; | <expr>;`
- `<decl>` &rarr; `<type> <sym>\([<int>?]\)\? \(= <expr>\)\?`
- `<expr>` &rarr; `<term> \(+ <term> | - <term>\)\*`
- `<term>` &rarr; `<unary> \(* <unary> | / <unary>\)\*`
- `<unary>` &rarr; `+ <fac> | - <fac> | <fac> | &<sym> | *<sym>`
- `<fac>` &rarr; `<int> | <sym> | <string> | <funccall> | (<expr>) | {\(<expr> \(, <expr>\)\*\)\?}`
- `<funccall>` &rarr; `<sym>(\(<expr> \(, <expr>\)\*\)\?)`
- `<type>` &rarr; `<sym>*\?`
