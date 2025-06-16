# Simple C

A subset of `C` compiler for [Jaris](https://github.com/giofiocco/jaris) assembly.

The file will be tokenized, white spaces are skipped (tabs are treated as single space).

Tokens are then parsed to an AST with the grammar defined in the Grammar section.
The grammar of the recursive parser, each rule are defined as `- rule-name ::= match`
Where the match can contain another rule or tokens (uppercase as in the Token section) and the other symbols works as follows:

- parenthesis for groupping
- `a | b`: rule `a` or rule `b`
- `a*` rule a zero or multiple times
- `a?` rule `a` zero or one time

The parser will look for `global` untill it reaches the end of the file.

Each node of the AST is then assigned a type (coherent with the rest) through the typecheck phase.

The AST is compiled to IR (intermediate rappresentation) to be able to generate better assembly code easier.

Is possible to enable optimizzation to the AST (after typecheck), to the IR or to the ASM code.

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
- IF: `if`
- ELSE: `else`
- EQ: `==`
- NEQ: `!=`
- NOT: `!`
- FOR: `for`
- WHILE: `while`
- EXTERN: `extern`
- SHL: `<<`
- SHR: `>>`
- BREAK: `break`
- DEFINE: `#define`

## Comments

Single-line comments: `//` (till the end of line)

Multi-line comments: `/* ... */`

## Macros

It's DEFINE SYM followed by a list of tokens until the newline.
When the same SYM is used it is substituted by the list of tokens

# Grammar

- global ::= ( funcdecl | typedef | decl SEMICOLON | extern )\*
- funcdecl ::= type sym paramdef block
- funcdef ::= type sym paramdef SEMICOLON
- paramdef ::= PARO ( type SYM ( COMMA type SYM )\* )? PARC
- block ::= BRO code\* BRC
- code ::= block | if | for | while | statement
- statement ::= ( RETURN expr ? | DECL | expr EQUAL expr | expr | asm | BREAK )? SEMICOLON
- decl ::= type SYM ( SQO expr SQC )? ( EQUAL expr )? | type SYM ( EQUAL expr )? ( COMMA STAR\* SYM ( EQUAL expr )? )\*
- expr ::= NOT? comp
- comp ::= atom ( ( EQ | NEQ ) atom )?
- atom ::= atom1 ( ( SHL | SHR ) atom1 )?
- atom1 ::= term ( PLUS term | MINUS term )\*
- term ::= unary ( STAR unary | SLASH unary )\*
- unary ::= PLUS unary | MINUS unary | AND unary | STAR unary | access SQO expr SQC | access
- access ::= fac DOT SYM | fac
- fac ::= INT | SYM | STRING | HEX | CHAR | funcall | PARO expr PARC | cast
- funcall ::= SYM PARO ( expr ( COMMA expr )\* )? PARC
- cast ::= PARO type PARC fac
- type ::= STRUCT ? SYM STAR ? | VOIDKW | INTKW | CHARKW
- typedef ::= TYPEDEF ( type | structdef | enumdef ) SYM SEMICOLON
- structdef ::= STRUCT SYM ? BRO ( decl SEMICOLON )\* BRC
- enumdef ::= ENUM BRO ( SYM COMMA )\* BRC
- asm ::= ASM PARO STRING PARC
- if ::= IF PARO expr PARC block ( ELSE ( if | block ) )?
- for ::= FOR PARO statement expr SEMICOLON (expr ( EQUAL expr )?)? PARC block
- while ::= WHILE PARO expr PARC block
- extern ::= EXTERN funcdef

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
- DIV
- CALL

# AST Optimization

- IF(BINARYOP(EQ,a,b), then, else) -> IF(BINARYOP(MINUS,a,b), else, then)
- IF(BINARYOP(NEQ,a,b), then, else) -> IF(BINARYOP(MINUS,a,b), then, else)
- IF(UNARYOP(NOT,a), then, else) -> IF(a, else, then)
- WHILE(BINARYOP(EQ,a,b), then, else) -> WHILE(UNARY(NOT, BINARYOP(MINUS,a,b), else, then))
- WHILE(BINARYOP(NEQ,a,b), then, else) -> WHILE(BINARYOP(MINUS,a,b), then, else)

# IR Optimization

- CHANGE_SP(0) -> nothing
- CHANGE_SP(x) CHANGE_SP(y) -> CHANGE_SP(x+y)
- ADDR_LOCAL READ(x) CHANGE_SP(y) if x <= -y -> CHANGE_SP(y+x)
- ADDR_LOCAL(x+z) READ(y) ADDR_LOCAL(y+z) READ(x) -> ADDR_LOCAL(z) READ(x+y)
- INT(x) INT(y) OPERATION(B_AH) -> INT((x << 8) | y)
- INT(x) INT(y) OPERATION(SUM) -> INT(x + y)
- INT(x) INT(y) OPERATION(SUB) -> INT(x - y)
- ADDR_LOCAL(2) READ(x) ADDR_LOCAL(y) WRITE(x) CHANGE_SP(z) if -z >= x -> ADDR_LOCAL(y-x) WRITE(x) CHANGE_SP(z+x)

# ASM Optimization

- PEEKAR 2 -> PEEKA
- PUSHA POPA -> nothing
- PUSHA POPB -> A_B
- RAM_A/B x (x < 256) -> RAM_AL/BL x
- (SUM | SUB) CMPA -> (SUM | SUB)
- PUSHA (RAM_A | RAM_AL | PEEKAR) POPB -> A_B (RAM_A | RAM_LA | PEEAKR)
- (RAM_A | RAM_AL) A_B -> RAM_B | RAM_BL
