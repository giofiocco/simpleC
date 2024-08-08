# Simple C

A compiler for a subset of `C`

# Tokens
- SYM: `[a-zA-Z_][a-zA-Z0-9_]*`
- INT: `[0-9]+`
- HEX: `0[xX]([0-9a-fA-F]{2} | [0-9a-fA-F]{4})`
- STRING: `"[^"]*"` 
- CHAR `'.'`

## Comments
Single-line comments: `//` (till the end of line)
Multi-line comments: `/* ... */`

# Grammar
- `<global>` &rarr; `<funcdecl>\* | <typedef> | <decl>`
- `<funcdecl>` &rarr; `<type> <sym><paramdef> <block>`
- `<paramdef>` &rarr; `(\(<type> <sym> \(, <type> <type>\)\*\)\?)`
- `<block>` &rarr; `{ <code>* }`
- `<code>` &rarr; `<statement>`
- `<statement>` &rarr; `return <expr>\?; | <decl>; | <expr> \(= <expr>\)?;`
- `<decl>` &rarr; `<type> <sym>\([<expr>?]\)\? \(= <expr>\)\?`
- `<expr>` &rarr; `<term> \(+ <term> | - <term>\)\*`
- `<term>` &rarr; `<unary> \(* <unary> | / <unary>\)\*`
- `<unary>` &rarr; `+ <access> | - <access> | <access> | &<access> | *<access> | <access>[<expr>]`
- `<access>` &rarr; `<fac> . <sym> | <fac>`
- `<fac>` &rarr; `<int> | <sym> | <string> | <char> | <funccall> | (<expr>) | (<type>) <fac> {\(<expr> \(, <expr>\)\*\)\?}`
- `<funccall>` &rarr; `<sym>(\(<expr> \(, <expr>\)\*\)\?)`
- `<type>` &rarr; `struct\? <sym> \*\?`
- `<typedef>` &rarr; `typedef \(<type> | <structdef>\) <sym>;`
- `<structdef>` &rarr; `struct <sym>\? { \(<type> <sym>;\)* }`
