# Simple C

A compiler for a subset of `C`

# Tokens

- SYM: `/[a-zA-Z_][a-zA-Z_0-9]*/`
- INT: `/[0-9]+/`
- HEX: `/0[xX]([0-9a-fA-F]{2}|[0-9a-fA-F]{4})/`
- STRING: `/"[^"]*?"/`
- PARO: `(`
- PARC: `)`
- SQO: `[`
- SQC: `]`
- BRO: `{`
- BRC: `}`
- RETURN: `return`
- TYPEDEF: `typedef`
- STRUCT: `struct`
- SEMICOLON: `;`
- PLUS: `+`
- MINUS: `-`
- STAR: `*`
- SLASH: `/`
- EQUAL: `=`
- AND: `&`
- COMMA: `,`
- DOT: `.`
- VOIDKW: `void`
- INTKW: `int`
- CHARKW: `char`
- CHAR: `/'.'/`
- ENUM: `enum`
- ASM: `__asm__`

## Comments

Single-line comments: `//` (till the end of line)
Multi-line comments: `/* ... */`

# Grammar

- global ::= ( funcdecl | typedef | decl )\*
- funcdecl ::= type sym paramdef block
- paramdef ::= PARO ( type SYM ( COMMA type SYM )\* )? PARC
- block ::= BRO code BRC
- code ::= statement
- statement ::= ( RETURN expr ? | DECL | expr EQUAL expr | expr | asm ) SEMICOLON
- decl ::= type SYM ( SQO expr SQC )? ( EQUAL expr )?
- expr ::= term ( PLUS term | MINUS term )\*
- term ::= unary ( STAR unary | SLASH unary )\*
- unary ::= PLUS access | MINUS access | AND access | STAR access | access SQO expr SQC | access
- access ::= fac DOT SYM | fac
- fac ::= INT | SYM | STRING | HEX | CHAR | funcall | PARO expr PARC | cast
- funcall ::= SYM PARO ( expr ( COMMA expr )\* )? PARC
- cast ::= PARO type PARC fac
- type ::= STRUCT ? SYM STAR ? | VOIDKW | INTKW | CHARKW
- typedef ::= TYPEDEF ( type | structdef | enumdef ) SYM SEMICOLON
- structdef ::= STRUCT SYM ? BRO ( type SYM SEMICOLON )\* BRC
- enumdef ::= ENUM BRO ( SYM COMMA )\* BRC
- asm ::= ASM PARO STRING PARC
