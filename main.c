#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#define TODO assert(0 && "TODO")

typedef struct {
  char *start;
  unsigned int len;
} sv_t;

#define SV_FMT "%*.*s"
#define SV_UNPACK(__sv__) (__sv__).len, (__sv__).len, (__sv__).start

bool sv_eq(sv_t a, sv_t b) {
  if (a.len != b.len) { return false; }
  for (unsigned int i = 0; i < a.len; ++i) {
    if (a.start[i] != b.start[i]) { return false; }
  }
  return true;
}

sv_t sv_from_cstr(char *str) {
return (sv_t) {str, strlen(str)};
}

typedef enum {
  T_NONE,
  T_SYM,
  T_INT,
  T_PARO,
  T_PARC,
  T_BRO,
  T_BRC,
  T_RETURN,
  T_SEMICOLON,
  T_PLUS,
  T_MINUS,
  T_STAR,
  T_SLASH,
  T_EQUAL,
} token_kind_t;

typedef struct {
  char *filename;
  char *row_start;
  int row, col;
} location_t;

typedef struct {
  token_kind_t kind;
  sv_t image;
  location_t loc;
} token_t;

typedef struct {
  char *buffer;
  location_t loc;
  token_t last_token;
  bool has_last_token;
} tokenizer_t;

void tokenizer_init(tokenizer_t *tokenizer, char *buffer, char *filename) {
  assert(tokenizer);
  tokenizer->buffer = buffer;
  tokenizer->loc = (location_t) {
    filename, buffer, 1, 1
  };
} 

void eprintf(token_t token, char *fmt, ...) {
  char *row_end = token.loc.row_start;
  while (*row_end != '\n' && *row_end != '\0') { ++row_end; }
  int row_len = row_end - token.loc.row_start;
  fprintf(stderr, "ERROR:%s:%d:%d: ", token.loc.filename, token.loc.row, token.loc.col);
  va_list argptr;
  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fprintf(stderr, "\n");
  fprintf(stderr, "%*d | %*.*s\n", token.loc.row < 1000 ? 3: 5, token.loc.row, row_len, row_len, token.loc.row_start);
  fprintf(stderr, "%s   %*.*s%c%*.*s\n", 
      token.loc.row < 1000 ? "   " : "     ", 
      token.loc.col-1, token.loc.col-1, 
      "                                                                                                                                                                                                        ", '^',
      token.image.len-1, token.image.len-1,
      "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
  exit(1);
}

token_t token_next(tokenizer_t *tokenizer) {
  assert(tokenizer);

  if (tokenizer->has_last_token) {
    tokenizer->has_last_token = false;
    return tokenizer->last_token;
  }

  token_kind_t table[128] = {0};
  table['('] = T_PARO;
  table[')'] = T_PARC;
  table['{'] = T_BRO;
  table['}'] = T_BRC;
  table[';'] = T_SEMICOLON;
  table['+'] = T_PLUS;
  table['-'] = T_MINUS;
  table['*'] = T_STAR;
  table['/'] = T_SLASH;
  table['='] = T_EQUAL;

  token_t token = {0};

  switch (*tokenizer->buffer) {
    case '\0': 
      break;
    case ' ': case '\t': case '\r':
      ++ tokenizer->buffer;
      ++ tokenizer->loc.col;
      return token_next(tokenizer);
    case '\n':
      ++ tokenizer->loc.row;
      tokenizer->loc.col = 1;
      ++ tokenizer->buffer;
      tokenizer->loc.row_start = tokenizer->buffer;
      return token_next(tokenizer);
    case '(': case ')': case '{': case '}': case ';':
    case '+': case '-': case '*': case '/':
    case '=':
      assert(table[(int)*tokenizer->buffer]);
      token = (token_t) {
        table[(int)*tokenizer->buffer],
        {tokenizer->buffer, 1},
        tokenizer->loc
      }; 
      ++ tokenizer->buffer;
      ++ tokenizer->loc.col;
      break;
    default:
      char *image_start = tokenizer->buffer;
      bool is_int = isdigit(*tokenizer->buffer) ? true : false;
      int len = 0;
      do {
        if (!isdigit(*tokenizer->buffer)) { is_int = false; }
        ++len;
        ++tokenizer->buffer;
      } while (isalpha(*tokenizer->buffer) || isdigit(*tokenizer->buffer) || *tokenizer->buffer == '_');
      sv_t image = {image_start, len};
      token = (token_t) {
        is_int ? T_INT : 
          sv_eq(image, sv_from_cstr("return")) ? T_RETURN : 
          T_SYM,
               image,
               tokenizer->loc
      };
      if (!is_int && isdigit(image_start[0])) {
        eprintf(token, "SYM cannot start with digit");
      }
      tokenizer->loc.col += len;
  }

  return token;
}

token_t token_peek(tokenizer_t *tokenizer) {
  assert(tokenizer);

  if (tokenizer->has_last_token) {
    return tokenizer->last_token;
  }
  token_t token = token_next(tokenizer);
  tokenizer->last_token = token;
  tokenizer->has_last_token = true;
  return token;
}

char *token_kind_to_string(token_kind_t kind) {
  switch (kind) {
    case T_NONE: return "NONE";
    case T_SYM: return "SYM";
    case T_INT: return "INT";
    case T_PARO: return "PARO";
    case T_PARC: return "PARC";
    case T_BRO: return "BRO";
    case T_BRC: return "BRC";
    case T_RETURN: return "RETURN";
    case T_SEMICOLON: return "SEMICOLON";
    case T_PLUS: return "PLUS";
    case T_MINUS: return "MINUS";
    case T_STAR: return "STAR";
    case T_SLASH: return "SLASH";
    case T_EQUAL: return "EQUAL";
  }
  assert(0);
}

void token_dump(token_t token) {
  printf("%s '" SV_FMT "' @ %d:%d\n", 
      token_kind_to_string(token.kind), 
      SV_UNPACK(token.image), 
      token.loc.row, 
      token.loc.col);
}

token_t expect_token_kind(tokenizer_t *tokenizer, token_kind_t kind) {
  assert(tokenizer);
  token_t token = token_next(tokenizer);
  if (token.kind != kind) {
    eprintf(token, "expected '%s' found '%s'", 
        token_kind_to_string(kind),
        token_kind_to_string(token.kind));
  }
  return token;
}

bool token_next_if_kind(tokenizer_t *tokenizer, token_kind_t kind) {
  assert(tokenizer);
  token_t token = token_peek(tokenizer);
  if (token.kind == kind) {
    token_next(tokenizer);
    return true;
  }
  return false;
}

typedef struct type_t_ {
  enum {
  TY_NONE,
  TY_VOID,
  TY_CHAR,
  TY_INT,
  TY_FUNC,
  } kind;
  union {
    struct {
      struct type_t_ *ret;
    } func;
  } as;
} type_t;

type_t *type_malloc(type_t type) {
  type_t *ptr = malloc(sizeof(type_t));
  assert(ptr);
  *ptr = type;
  return ptr;
}

void type_free(type_t *type) {
  if (!type) { return; }

  switch (type->kind) {
    case TY_NONE:
    case TY_VOID:
    case TY_CHAR:
    case TY_INT:
      break;
    case TY_FUNC:
      type_free(type->as.func.ret);
      free(type->as.func.ret);
      break;
  } 
}

void type_dump(type_t *type) {
  assert(type);

  switch (type->kind) {
    case TY_NONE: 
      printf("NONE"); 
      break;
    case TY_VOID:
      printf("VOID"); 
      break;
    case TY_CHAR:
      printf("CHAR");
      break;
    case TY_INT:
      printf("INT");
      break;
    case TY_FUNC:
      printf("FUNC ");
      assert(type->as.func.ret);
      type_dump(type->as.func.ret);
      break;
  }
}

void type_dump_to_string(type_t *type, char **str, int *size) {
  assert(type);
  assert(str);
  assert(*str);
  assert(size);
  assert(*size > 0);

  switch (type->kind) {
    case TY_NONE: 
      assert(0);
    case TY_VOID:
      if (*size < 4) { **str = '_'; return; }
      strcpy(*str, "VOID"); 
      *size -= 4; *str += 4;
      break;
    case TY_CHAR:
      if (*size < 4) { **str = '_'; return; }
      strcpy(*str, "CHAR"); 
      *size -= 4; *str += 4;
      break;
    case TY_INT:
      if (*size < 4) { **str = '_'; return; }
      strcpy(*str, "INT"); 
      *size -= 3; *str += 3;
      break;
    case TY_FUNC:
       if (*size < 5) { **str = '_'; return; }
      strcpy(*str, "FUNC "); 
      *size -= 5; *str += 5;
      type_dump_to_string(type->as.func.ret, str, size);
     break;
  }
}

bool type_cmp(type_t *a, type_t *b) {
  assert(a);
  assert(b);
  if (a->kind == b->kind) {
    switch (a->kind) {
      case TY_NONE:
      case TY_VOID:
      case TY_CHAR:
      case TY_INT:
        return true;
      case TY_FUNC:
        return type_cmp(a->as.func.ret, b->as.func.ret);
    }
  }
  return false;
}

typedef enum {
  A_NONE,
  A_FUNCDECL,
  A_BLOCK,
  A_RETURN,
  A_BINARYOP,
  A_UNARYOP,
  A_FAC,
  A_DECL,
} ast_kind_t;

typedef struct ast_t_ {
  ast_kind_t kind;
  token_t forerror;
  type_t type;
  union {
    struct {
      token_t type;
      token_t name;
      struct ast_t_ *block;
    } funcdecl;
    struct {
      struct ast_t_ *code;
      struct ast_t_ *next;
    } block;
    struct {
      struct ast_t_ *expr;
    } return_;
    struct {
      token_t op;
      struct ast_t_ *lhs;
      struct ast_t_ *rhs;
    } binaryop;
    struct {
      token_t op;
      struct ast_t_ *arg;
    } unaryop;
    struct {
      token_t tok;
    } fac;
    struct {
      token_t type;
      token_t name;
      struct ast_t_ *expr;
    } decl;
  } as;
} ast_t;

ast_t *ast_malloc(ast_t ast) {
  ast_t *ptr = malloc(sizeof(ast_t));
  assert(ptr);
  *ptr = ast;
  return ptr;
}

void ast_free(ast_t *ast) {
  if (!ast) { return; }

  type_free(&ast->type);

  switch (ast->kind) {
    case A_NONE:
      break;
    case A_FUNCDECL:
      ast_free(ast->as.funcdecl.block);
      break;
    case A_BLOCK:
      ast_free(ast->as.block.code);
      ast_free(ast->as.block.next);
      break;
    case A_RETURN:
      ast_free(ast->as.return_.expr);
      break;
    case A_BINARYOP:
      ast_free(ast->as.binaryop.lhs);
      ast_free(ast->as.binaryop.rhs);
      break;
    case A_UNARYOP:
      ast_free(ast->as.unaryop.arg);
      break;
    case A_FAC:
      break;
    case A_DECL:
      ast_free(ast->as.decl.expr);
      break;
  }
  free(ast);
}

void ast_dump(ast_t *ast) {
  if (!ast) { printf("NULL"); return; }

  switch (ast->kind) {
    case A_NONE: 
      assert(0);
    case A_FUNCDECL:
      printf("FUNCDECL(" SV_FMT " " SV_FMT " ", 
          SV_UNPACK(ast->as.funcdecl.type.image),
          SV_UNPACK(ast->as.funcdecl.name.image));
      ast_dump(ast->as.funcdecl.block);
      printf(") {");
      type_dump(&ast->type);
      printf("}");
      break;
    case A_BLOCK:
      printf("BLOCK(");
      ast_dump(ast->as.block.code);
      printf(" ");
      ast_dump(ast->as.block.next);
      printf(") {");
      type_dump(&ast->type);
      printf("}");
      break;
    case A_RETURN:
      printf("RETURN(");
      ast_dump(ast->as.return_.expr);
      printf(") {");
      type_dump(&ast->type);
      printf("}");

      break;
    case A_BINARYOP:
      printf("BINARYOP(" SV_FMT " ", 
          SV_UNPACK(ast->as.binaryop.op.image));
      ast_dump(ast->as.binaryop.lhs);
      printf(" ");
      ast_dump(ast->as.binaryop.rhs);
      printf(") {");
      type_dump(&ast->type);
      printf("}");
      break;
    case A_UNARYOP:
      printf("UNARYOP(" SV_FMT " ", 
          SV_UNPACK(ast->as.unaryop.op.image));
      ast_dump(ast->as.unaryop.arg);
      printf(") {");
      type_dump(&ast->type);
      printf("}");
      break;
    case A_FAC:
      printf("FAC(" SV_FMT ") {", 
          SV_UNPACK(ast->as.fac.tok.image));
      type_dump(&ast->type);
      printf("}");
      break;
    case A_DECL:
      printf("DECL(" SV_FMT " " SV_FMT " ", 
          SV_UNPACK(ast->as.decl.type.image),
          SV_UNPACK(ast->as.decl.name.image));
      ast_dump(ast->as.decl.expr);
      printf(") {");
      type_dump(&ast->type);
      printf("}");
      break;
  }
}

#define SYMBOL_MAX 128
#define SCOPE_MAX 32

typedef struct {
  sv_t image;
  type_t type;
  union {
    int local;
  } info;
} symbol_t;

typedef struct {
  symbol_t symbols[SYMBOL_MAX];
  int symbol_num;
} scope_t;

typedef struct {
  scope_t scopes[SCOPE_MAX];
  int scope_num;
  int sp;
} state_t;

void state_push_scope(state_t *state) {
  assert(state);
  assert(state->scope_num + 1 < SCOPE_MAX);
  ++ state->scope_num;
}

void state_drop_scope(state_t *state) {
  assert(state);
  -- state->scope_num;
}

void state_add_local_symbol(state_t *state, sv_t image, type_t type) {
  assert(state);
  scope_t *scope = &state->scopes[state->scope_num];
  assert(scope->symbol_num + 1 < SYMBOL_MAX);
  scope->symbols[scope->symbol_num ++] = (symbol_t) {image, type, {state->sp}};
  state->sp ++;
}

symbol_t *state_find_symbol(state_t *state, token_t token) {
  assert(state);
  scope_t *scope = &state->scopes[state->scope_num];
  for (int i = 0; i < scope->symbol_num; ++i) {
    if (sv_eq(scope->symbols[i].image, token.image)) {
      return &scope->symbols[i];
    }
  }
  eprintf(token, "symbol not declared: " SV_FMT, SV_UNPACK(token.image));
  assert(0);
}

ast_t *parse_fac(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_next(tokenizer);
  if (token.kind != T_SYM && token.kind != T_INT) {
    eprintf(token, "expected SYM or INT found '%s'", 
        token_kind_to_string(token.kind));
  }

  return ast_malloc((ast_t){A_FAC, token, {0}, {.fac = {token}}});
}

ast_t *parse_unary(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);
  if (token.kind == T_PLUS || token.kind == T_MINUS) {
    token_next(tokenizer);
    ast_t *arg = parse_fac(tokenizer);
    return ast_malloc((ast_t){A_UNARYOP, token, {0}, {.unaryop = {token, arg}}});
  }

  return parse_fac(tokenizer);
}

ast_t *parse_term(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *a = parse_unary(tokenizer);
  token_t token;
  while (token = token_peek(tokenizer), token.kind == T_STAR || token.kind == T_SLASH) {
    token_next(tokenizer);
    ast_t *b = parse_unary(tokenizer);
    a = ast_malloc((ast_t){A_BINARYOP, a->forerror, {0}, {.binaryop = {token, a, b}}});
  }

  return a;
}

ast_t *parse_expr(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *a = parse_term(tokenizer);
  token_t token;
  while (token = token_peek(tokenizer), token.kind == T_PLUS || token.kind == T_MINUS) {
    token_next(tokenizer);
    ast_t *b = parse_term(tokenizer);
    a = ast_malloc((ast_t){A_BINARYOP, a->forerror, {0}, {.binaryop = {token, a, b}}});
  }

  return a;
}

ast_t *parse_statement(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);
  if (token.kind == T_RETURN) {
    token_next(tokenizer);
    ast_t *expr = NULL;
    if (!token_next_if_kind(tokenizer, T_SEMICOLON)) {
      expr = parse_expr(tokenizer);
      expect_token_kind(tokenizer, T_SEMICOLON);
    }
    return ast_malloc((ast_t){A_RETURN, token, {0}, {.return_ = {expr}}});

  } else if (token.kind == T_SYM) {
    token_t type = token_next(tokenizer);
    token_t name = expect_token_kind(tokenizer, T_SYM);
    ast_t *expr = NULL;
    if (token_next_if_kind(tokenizer, T_EQUAL)) {
      expr = parse_expr(tokenizer);
    }
    expect_token_kind(tokenizer, T_SEMICOLON);
    return ast_malloc((ast_t){A_DECL, type, {0}, {.decl = {type, name, expr}}});
  }

  eprintf(token, "expected return or decl statement");
  return NULL;
}

ast_t *parse_code(tokenizer_t *tokenizer) {
  assert(tokenizer);

  return parse_statement(tokenizer);
}

ast_t *parse_block(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = expect_token_kind(tokenizer, T_BRO);

  ast_t ast = {A_BLOCK, token, {0}, {.block = {NULL, NULL}}};

  if (token_next_if_kind(tokenizer, T_BRC)) {
    return ast_malloc(ast);
  }

  ast.as.block.code = parse_code(tokenizer);
  ast_t *block = &ast;
  ast_t *code = NULL;

  while (token_peek(tokenizer).kind != T_BRC) {
    code = parse_code(tokenizer);
    if (code) {
      block->as.block.next = ast_malloc((ast_t){A_BLOCK, token, {0}, {.block = {code, NULL}}});
      block = block->as.block.next;
    }
  } 

  expect_token_kind(tokenizer, T_BRC);

  return ast_malloc(ast);
}

ast_t *parse_funcdecl(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t type = expect_token_kind(tokenizer, T_SYM);
  token_t name = expect_token_kind(tokenizer, T_SYM);

  expect_token_kind(tokenizer, T_PARO);
  expect_token_kind(tokenizer, T_PARC);

  ast_t *block = parse_block(tokenizer);

  return ast_malloc((ast_t){A_FUNCDECL, type, {0}, {.funcdecl = {type, name, block}}});
} 

ast_t *parse(tokenizer_t *tokenizer) {
  assert(tokenizer);

  return parse_funcdecl(tokenizer);
}

type_t parse_type(token_t token) {
  type_t type = {0};
  if (sv_eq(token.image, sv_from_cstr("int"))) {
    type.kind = TY_INT;
  } else if (sv_eq(token.image, sv_from_cstr("char"))) {
    type.kind = TY_CHAR;
  } else if (sv_eq(token.image, sv_from_cstr("void"))) {
    type.kind = TY_VOID;
  } else {
    eprintf(token, "is not a type");
  } 
  return type;
}

void typecheck(ast_t *ast, state_t *state);
void typecheck_expect(ast_t *ast, state_t *state, type_t type) {
  assert(ast);
  typecheck(ast, state);

  if (type_cmp(&ast->type, &type)) { return; }

  int size = 128;
  char str[128] = {0};
  char *str2 = str;
  type_dump_to_string(&type, &str2, &size);
  str2 += 1; size -= 1;
  char *str3 = str2;
  type_dump_to_string(&ast->type, &str2, &size);
  eprintf(ast->forerror, "expected '%s', found '%s'", str, str3);
}

void typecheck_expandable(ast_t *ast, state_t *state, type_t type) {
  assert(ast);
  typecheck(ast, state);

  if (type_cmp(&ast->type, &type)) { return; }
  if (type.kind == TY_INT && ast->type.kind == TY_CHAR) {
    return;
  }

  int size = 128;
  char str[128] = {0};
  char *str2 = str;
  type_dump_to_string(&type, &str2, &size);
  str2 += 1; size -= 1;
  char *str3 = str2;
  type_dump_to_string(&ast->type, &str2, &size);
  eprintf(ast->forerror, "expected '%s', found '%s'", str, str3);
}

void typecheck_funcbody(ast_t *ast, state_t *state, type_t ret) {
  assert(ast);
  assert(state);
  assert(ret.kind != TY_FUNC);
  assert(ast->kind == A_BLOCK);

  ast_t *code = ast->as.block.code;
  assert(code);

  if (code->kind == A_RETURN) {
    typecheck_expandable(code, state, ret);
  } else {
    typecheck(code, state);
  } 
  if (ast->as.block.next) {
    typecheck_funcbody(ast->as.block.next, state, ret);
  }
}

void typecheck(ast_t *ast, state_t *state) {
  assert(state);

  if (!ast) { return; }

  switch (ast->kind) {
    case A_NONE:
      assert(0);
    case A_FUNCDECL:
      {
        state_push_scope(state);
        type_t type = parse_type(ast->as.funcdecl.type);
        typecheck_funcbody(ast->as.funcdecl.block, state, type);
        state_drop_scope(state);
        ast->type = (type_t) {TY_FUNC, {.func = {type_malloc(type)}}};
      } break;
    case A_BLOCK:
      assert(ast->as.block.code);
      typecheck(ast->as.block.code, state);
      typecheck(ast->as.block.next, state);
      ast->type.kind = TY_VOID;
      break;
    case A_RETURN:
      typecheck(ast->as.return_.expr, state);
      if (ast->as.return_.expr) {
        ast->type.kind = ast->as.return_.expr->type.kind;
      } else {
        ast->type.kind = TY_VOID;
      }
      break;
    case A_BINARYOP:
      TODO;
    case A_UNARYOP:
      TODO;
    case A_FAC:
      if (ast->as.fac.tok.kind == T_INT) {
        ast->type.kind = TY_INT;
      } else if (ast->as.fac.tok.kind == T_SYM) {
        symbol_t *s = state_find_symbol(state, ast->as.fac.tok);
        ast->type = s->type;
      } else {
        assert(0);
      }
      break;
    case A_DECL:
      {
        type_t type = parse_type(ast->as.decl.type);
        if (ast->as.decl.expr) {
          typecheck_expandable(ast->as.decl.expr, state, type);
        }
        state_add_local_symbol(state, ast->as.decl.name.image, type);
        ast->type.kind = TY_VOID;
      } break;
  }
}

void compile(ast_t *ast, state_t *state) {
  assert(state);

  if (!ast) { return; }

  switch (ast->kind) {
    case A_NONE: 
      assert(0);
    case A_FUNCDECL: 
      printf(SV_FMT ":\n\t", SV_UNPACK(ast->as.funcdecl.name.image));
      compile(ast->as.funcdecl.block, state);
      break;
    case A_BLOCK: 
      assert(ast->as.block.code);
      compile(ast->as.block.code, state);
      compile(ast->as.block.next, state);
      break;
    case A_RETURN: 
      assert(ast->as.return_.expr);
      compile(ast->as.return_.expr, state);
      for (int i = 0; i < state->sp; ++i) { printf("INCSP "); }
      printf("RET\n\t");
      break;
    case A_BINARYOP:
      assert(ast->as.binaryop.lhs);
      compile(ast->as.binaryop.lhs, state);
      printf("A_B\n\t");
      assert(ast->as.binaryop.rhs);
      compile(ast->as.binaryop.rhs, state);
      switch (ast->as.binaryop.op.kind) {
        case T_PLUS: printf("SUM\n\t"); break;
        case T_MINUS: printf("SUB\n\t"); break;
        case T_STAR: assert(0);
        case T_SLASH: assert(0);
        default: assert(0);
      }
      break;
    case A_UNARYOP:
      if (ast->as.unaryop.op.kind == T_MINUS) {
        assert(ast->as.unaryop.arg);
        compile(ast->as.unaryop.arg, state);
        printf("RAM_BL 0x00 SUB\n\t");
      } else {
        assert(0);
      }
      break;
    case A_FAC: 
      {
        token_t token = ast->as.fac.tok;
        if (token.kind == T_INT) {
          printf("RAM_A 0x%04d\n\t", atoi(token.image.start));
        } else if (token.kind == T_SYM) {
          symbol_t *s = state_find_symbol(state, token); 
          int num = state->sp - s->info.local;
          assert(num < 256 && num >= 0);
          if (num == 1) {
            printf("PEEKA\n\t");
          } else {
            printf("PEEKAR 0x%02X\n\t", 2*num);
          }
        } else {
          assert(0);
        }
      } break;
    case A_DECL:
      type_t type = parse_type(ast->as.decl.type);
      state_add_local_symbol(state, ast->as.decl.name.image, type);
      if (ast->as.decl.expr) {
        compile(ast->as.decl.expr, state);
        printf("PUSHA\n\t");
      } else {
        printf("DECSP\n\t");
      }
      break;
  }
}

int main() {
  char *buffer = "int main() { int a = 2; return a; }";

  tokenizer_t tokenizer = {0};
  tokenizer_init(&tokenizer, buffer, "boh");

  ast_t *ast = parse(&tokenizer);
  assert(ast);

  state_t state = {0};
  typecheck(ast, &state);

  ast_dump(ast);
  printf("\n");

  state = (state_t) {0};
  compile(ast, &state);

  ast_free(ast);

  return 0;
}
