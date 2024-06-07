#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

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

typedef enum {
  A_NONE,
  A_FUNCDECL,
  A_BLOCK,
  A_RETURN,
  A_EXPR,
} ast_kind_t;

typedef struct ast_t_ {
  ast_kind_t kind;
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
      token_t tok;
    } expr;
  } as;
} ast_t;

ast_t *ast_malloc(ast_t ast) {
  ast_t *ptr = malloc(sizeof(ast_t));
  assert(ptr);
  *ptr = ast;
  return ptr;
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
      printf(")");
      break;
    case A_BLOCK:
      printf("BLOCK(");
      ast_dump(ast->as.block.code);
      printf(" ");
      ast_dump(ast->as.block.next);
      printf(")");
      break;
    case A_RETURN:
      printf("RETURN(");
      ast_dump(ast->as.return_.expr);
      printf(")");
      break;
    case A_EXPR:
      printf("EXPR(" SV_FMT ")", 
          SV_UNPACK(ast->as.expr.tok.image));
      break;
  }
}

ast_t *parse_expr(tokenizer_t *tokenizer) {
assert(tokenizer);

  token_t token = token_next(tokenizer);
  if (token.kind != T_SYM && token.kind != T_INT) {
    eprintf(token, "expected SYM or INT found '%s'", 
        token_kind_to_string(token.kind));
  }

  return ast_malloc((ast_t){A_EXPR, {.expr = {token}}});
}

ast_t *parse_code(tokenizer_t *tokenizer) {
  assert(tokenizer);

  expect_token_kind(tokenizer, T_RETURN);
  ast_t *expr = parse_expr(tokenizer);
  expect_token_kind(tokenizer, T_SEMICOLON);

  return ast_malloc((ast_t){A_RETURN, {.return_ = {expr}}});
}

ast_t *parse_block(tokenizer_t *tokenizer) {
  assert(tokenizer);

  expect_token_kind(tokenizer, T_BRO);

  ast_t ast = {0};
  ast.kind = A_BLOCK;

  ast.as.block.code = parse_code(tokenizer);
  ast_t *block = &ast;
  ast_t *code = NULL;

  while (token_peek(tokenizer).kind != T_BRC) {
    code = parse_code(tokenizer);
    if (code) {
      block->as.block.next = ast_malloc((ast_t){A_BLOCK, {.block = {code, NULL}}});
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

  ast_t ast = {
    A_FUNCDECL, { .funcdecl = {type, name, block} }
  };

  return ast_malloc(ast);
} 

ast_t *parse(tokenizer_t *tokenizer) {
  assert(tokenizer);

  return parse_funcdecl(tokenizer);
}

int main() {
  char *buffer = "int main() { return 0; }";

  tokenizer_t tokenizer = {0};
  tokenizer_init(&tokenizer, buffer, "boh");

  ast_t *ast = parse(&tokenizer);
  assert(ast);
  ast_dump(ast);


  return 0;
}
