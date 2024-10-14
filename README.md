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
- block ::= BRO code\* BRC
- code ::= statement | code
- statement ::= ( RETURN expr ? | DECL | expr EQUAL expr | expr | asm ) SEMICOLON
- decl ::= type SYM ( SQO expr SQC )? ( EQUAL expr )?
- expr ::= term ( PLUS term | MINUS term )\*
- term ::= unary ( STAR unary | SLASH unary )\*
- unary ::= PLUS unary | MINUS unary | AND unary | STAR unary | access SQO expr SQC | access
- access ::= fac DOT SYM | fac
- fac ::= INT | SYM | STRING | HEX | CHAR | funcall | PARO expr PARC | cast
- funcall ::= SYM PARO ( expr ( COMMA expr )\* )? PARC
- cast ::= PARO type PARC fac
- type ::= STRUCT ? SYM STAR ? | VOIDKW | INTKW | CHARKW
- typedef ::= TYPEDEF ( type | structdef | enumdef ) SYM SEMICOLON
- structdef ::= STRUCT SYM ? BRO ( type SYM SEMICOLON )\* BRC
- enumdef ::= ENUM BRO ( SYM COMMA )\* BRC
- asm ::= ASM PARO STRING PARC

# Intermidiate Rappresentation

- SETLABEL
- RETURN
- FUNCEND
- ADDR_LOCAL
- ADDR_GLOBAL
- ADDR_OFFSET
- READ
- WRITE
- CHANGE_SP
- INT
- STRING
- OPERATION
- MUL
- CALL

# IR Optimization

- CHANGE_SP(0) -> nothing
- CHANGE_SP(x) CHANGE_SP(y) -> CHANGE_SP(x+y)
- ADDR_LOCAL READ(x) CHANGE_SP(y) if x <= -y -> CHANGE_SP(y+x)
- ADDR_LOCAL(x+z) READ(y) ADDR_LOCAL(y+z) READ(x) -> ADDR_LOCAL(z) READ(x+y)
- INT(x) INT(y) OPERATION(B_AH) -> INT((x << 8) | y)
- INT(x) INT(y) OPERATION(SUM) -> INT(x + y)
- INT(x) INT(y) OPERATION(SUB) -> INT(x - y)
- ADDR_LOCAL(2) READ(x) ADDR_LOCAL(y) WRITE(x) CHANGE_SP(z) if -z >= x -> ADDR_LOCAL(y-x) WRITE(x) CHANGE_SP(z+x)
