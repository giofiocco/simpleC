#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <setjmp.h>

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

#define ALLOCED_MAX 2048
void *alloced[ALLOCED_MAX];
int alloced_num = 0;

void *alloc(int size) {
  assert(alloced_num + 1 < ALLOCED_MAX);
  void *ptr = malloc(size);
  alloced[alloced_num ++] = ptr;
  return ptr;
}

void free_all() {
  for (int i = 0; i < alloced_num; ++i) {
    assert(alloced[i] != NULL);
    free(alloced[i]);
  }
}

typedef enum {
  T_NONE,
  T_SYM,
  T_INT,
  T_HEX,
  T_STRING,
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
  T_AND,
  T_COLON,
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

void print_location(token_t token) {
  char *row_end = token.loc.row_start;
  while (*row_end != '\n' && *row_end != '\0') { ++row_end; }
  int row_len = row_end - token.loc.row_start;
  fprintf(stderr, "%*d | %*.*s\n", token.loc.row < 1000 ? 3: 5, token.loc.row, row_len, row_len, token.loc.row_start);
  fprintf(stderr, "%s   %*.*s%c%*.*s\n", 
      token.loc.row < 1000 ? "   " : "     ", 
      token.loc.col-1, token.loc.col-1, 
      "                                                                                                                                                                                                        ", '^',
      token.image.len-1, token.image.len-1,
      "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

static bool catch = false;
static jmp_buf catch_buf;
void eprintf(token_t token, char *fmt, ...) {
  if (catch) { catch = false; longjmp(catch_buf, 1); }
  fprintf(stderr, "ERROR:%s:%d:%d: ", token.loc.filename, token.loc.row, token.loc.col);
  va_list argptr;
  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fprintf(stderr, "\n");
  print_location(token);
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
  table['&'] = T_AND;
  table[','] = T_COLON;

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
    case '=': case '&': case ',':
      assert(table[(int)*tokenizer->buffer]);
      token = (token_t) {
        table[(int)*tokenizer->buffer],
        {tokenizer->buffer, 1},
        tokenizer->loc
      }; 
      ++ tokenizer->buffer;
      ++ tokenizer->loc.col;
      break;
    case '"':
      {
        char *start = tokenizer->buffer;
        do {
          ++ tokenizer->buffer;
        } while (*tokenizer->buffer != '"');
        ++ tokenizer->buffer;
        sv_t str = {start, tokenizer->buffer - start};
        token = (token_t) {
          T_STRING,
            str,
            tokenizer->loc
        };
        tokenizer->loc.col += str.len;
      } break;
    case '0':
      if (*(tokenizer->buffer + 1) == 'x' || *(tokenizer->buffer + 1) == 'X') {
        char *image_start = tokenizer->buffer;
        tokenizer->buffer += 2;
        while (isdigit(*tokenizer->buffer) || 
            ('a' <= *tokenizer->buffer && *tokenizer->buffer <= 'f') ||
            ('A' <= *tokenizer->buffer && *tokenizer->buffer <= 'F')) {
          ++ tokenizer->buffer;
        }
        token = (token_t) {
          T_HEX,
            {image_start, tokenizer->buffer - image_start},
            tokenizer->loc
        };
        if (token.image.len - 2 != 2 && token.image.len - 2 != 4) {
          eprintf(token, "HEX can be 1 or 2 bytes\n");
        }
        tokenizer->loc.col += token.image.len;
        break;
      }
      __attribute__((fallthrough));
    default:
      {
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
    case T_HEX: return "HEX";
    case T_STRING: return "STRING";
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
    case T_AND: return "AND";
    case T_COLON: return "COLON";
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

token_t token_expect(tokenizer_t *tokenizer, token_kind_t kind) {
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
    TY_PTR,
    TY_PARAM,
  } kind;
  union {
    struct {
      struct type_t_ *ret;
      struct type_t_ *params;
    } func;
    struct type_t_ *ptr;
    struct {
      struct type_t_ *type;
      struct type_t_ *next;
    } param;
  } as;
} type_t;

type_t type_clone(type_t type);
type_t *type_malloc(type_t type) {
  type_t *ptr = alloc(sizeof(type_t));
  assert(ptr);
  *ptr = type_clone(type);
  return ptr;
}

type_t type_clone(type_t type) {
  type_t clone = {type.kind, {{0}}};

  switch (type.kind) {
    case TY_NONE:
    case TY_VOID:
    case TY_CHAR:
    case TY_INT:
      break;
    case TY_FUNC:
      assert(type.as.func.ret);
      clone.as.func.ret = type_malloc(*type.as.func.ret);
      if (type.as.func.params) {
        clone.as.func.params = type_malloc(type_clone(*type.as.func.params));
      }
      break;
    case TY_PTR:
      assert(type.as.ptr);
      clone.as.ptr = type_malloc(*type.as.ptr);
      break;
    case TY_PARAM:
      assert(type.as.param.type);
      clone.as.param.type = type_malloc(*type.as.param.type);
      if (type.as.param.next) {
        clone.as.param.next = type_malloc(type_clone(*type.as.param.next));
      }
      break;
  }

  return clone;
}

char *type_dump_to_string(type_t *type) {
  assert(type);

  char *string = alloc(128);
  assert(string);
  memset(string, 0, 128);

  switch (type->kind) {
    case TY_NONE: 
      assert(0);
    case TY_VOID:
      strcpy(string, "VOID");
      break;
    case TY_CHAR:
      strcpy(string, "CHAR");
      break;
    case TY_INT:
      strcpy(string, "INT");
      break;
    case TY_FUNC:
      {
        strcpy(string, "FUNC ");
        assert(type->as.func.ret);
        char *str = type_dump_to_string(type->as.func.ret);
        int len = strlen(str);
        assert(len < 128 - 5);
        strcpy(string + 5, str);
        if (type->as.func.params) {
          str = type_dump_to_string(type->as.func.params);
          int plen = strlen(str);
          assert(plen < 128 - 5 - len);
          string[5+len] = ' ';
          strcpy(string + 5 + 1 + len, str);
        }
      } break;
    case TY_PTR:
      {
        strcpy(string, "PTR ");
        assert(type->as.ptr);
        char *str = type_dump_to_string(type->as.ptr);
        assert(strlen(str) < 128 - 4);
        strcpy(string + 4, str);
      } break;
    case TY_PARAM:
      {
        strcpy(string, "PARAM ");
        assert(type->as.param.type);
        char *str = type_dump_to_string(type->as.param.type);
        int len = strlen(str);
        assert(len < 128 - 6);
        strcpy(string + 6, str);
        if (type->as.param.next) {
          str = type_dump_to_string(type->as.param.next);
          int plen = strlen(str);
          assert(plen < 128 - 6 - len);
          string[6+len] = ' ';
          strcpy(string + 6 + 1 + len, str);
        }
      } break;
  }

  return string;
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
        assert(a->as.func.ret); assert(b->as.func.ret);
        if ((a->as.func.params ? 1 : 0) ^ (b->as.func.params ? 1 : 0)) {
          return false;
        }
        return type_cmp(a->as.func.ret, b->as.func.ret) 
          && (a->as.func.params ? type_cmp(a->as.func.params, b->as.func.params) : true);
      case TY_PTR:
        assert(a->as.ptr); assert(b->as.ptr);
        return type_cmp(a->as.ptr, b->as.ptr);
      case TY_PARAM:
        assert(a->as.param.type); assert(b->as.param.type);
        if ((a->as.param.next ? 1 : 0) ^ (b->as.param.next ? 1 : 0)) {
          return false;
        }
        return type_cmp(a->as.param.type, b->as.param.type) 
          && (a->as.param.next ? type_cmp(a->as.param.next, b->as.param.next) : true);
    }
  }
  return false;
}

int type_size(type_t *type) {
  assert(type);
  switch (type->kind) {
    case TY_NONE:
      assert(0);
    case TY_VOID:
      return 0;
    case TY_CHAR:
      return 1;
    case TY_INT:
      return 2;
    case TY_FUNC:
      assert(0);
    case TY_PTR:
      return 2;
    case TY_PARAM:
      assert(0);
  }
  assert(0);
}

typedef enum {
  A_NONE,
  A_GLOBAL,
  A_FUNCDECL,
  A_PARAMDEF,
  A_BLOCK,
  A_RETURN,
  A_BINARYOP,
  A_UNARYOP,
  A_FAC,
  A_DECL,
  A_GLOBDECL,
  A_ASSIGN,
  A_FUNCALL,
  A_PARAM,
  A_GROUP,
} ast_kind_t;

typedef struct ast_t_ {
  ast_kind_t kind;
  token_t forerror;
  type_t type;
  union {
    struct {
      struct ast_t_ *ast;
      struct ast_t_ *next;
    } astlist;
    struct {
      type_t type;
      token_t name;
      struct ast_t_ *params;
      struct ast_t_ *block;
    } funcdecl;
    struct {
      type_t type;
      token_t name;
      struct ast_t_ *next;
    } paramdef;
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
      type_t type;
      token_t name;
      struct ast_t_ *expr;
    } decl;
    struct {
      struct ast_t_ *dest;
      struct ast_t_ *expr;
    } assign;
    struct {
      token_t name;
      struct ast_t_ *params;
    } funcall;
    token_t fac;
    struct ast_t_ *group;
  } as;
} ast_t;

ast_t *ast_malloc(ast_t ast) {
  ast_t *ptr = alloc(sizeof(ast_t));
  assert(ptr);
  *ptr = ast;
  return ptr;
}

void ast_dump(ast_t *ast, bool dumptype) {
  if (!ast) { printf("NULL"); return; }

  switch (ast->kind) {
    case A_NONE: 
      assert(0);
    case A_GLOBAL:
      printf("GLOBAL(");
      ast_dump(ast->as.astlist.ast, dumptype);
      printf(" ");
      ast_dump(ast->as.astlist.next, dumptype);
      printf(")");
      break;
    case A_FUNCDECL:
      {
        printf("FUNCDECL(");
        char *str = type_dump_to_string(&ast->as.funcdecl.type);
        printf("%s " SV_FMT " ", 
            str,
            SV_UNPACK(ast->as.funcdecl.name.image));
        ast_dump(ast->as.funcdecl.params, dumptype);
        printf(" ");
        ast_dump(ast->as.funcdecl.block, dumptype);
        printf(")");
      } break;
    case A_PARAMDEF:
      {
        printf("PARAMDEF(");
        char *str = type_dump_to_string(&ast->as.paramdef.type);
        printf("%s " SV_FMT " ", 
            str,
            SV_UNPACK(ast->as.paramdef.name.image));
        ast_dump(ast->as.paramdef.next, dumptype);
        printf(")");
      } break;
    case A_BLOCK:
      printf("BLOCK(");
      ast_dump(ast->as.astlist.ast, dumptype);
      printf(" ");
      ast_dump(ast->as.astlist.next, dumptype);
      printf(")");
      break;
    case A_RETURN:
      printf("RETURN(");
      ast_dump(ast->as.return_.expr, dumptype);
      printf(")");
      break;
    case A_BINARYOP:
      printf("BINARYOP(" SV_FMT " ", 
          SV_UNPACK(ast->as.binaryop.op.image));
      ast_dump(ast->as.binaryop.lhs, dumptype);
      printf(" ");
      ast_dump(ast->as.binaryop.rhs, dumptype);
      printf(")");
      break;
    case A_UNARYOP:
      printf("UNARYOP(" SV_FMT " ", 
          SV_UNPACK(ast->as.unaryop.op.image));
      ast_dump(ast->as.unaryop.arg, dumptype);
      printf(")");
      break;
    case A_FAC:
      printf("FAC(" SV_FMT ")", 
          SV_UNPACK(ast->as.fac.image));
      break;
    case A_GLOBDECL:
      printf("GLOB");
      __attribute__((fallthrough));
    case A_DECL:
      {
        printf("DECL(");
        char *str = type_dump_to_string(&ast->as.decl.type);
        printf("%s " SV_FMT " ", 
            str,
            SV_UNPACK(ast->as.decl.name.image));
        ast_dump(ast->as.decl.expr, dumptype);
        printf(")");
      } break;
    case A_ASSIGN:
      printf("ASSIGN(");
      ast_dump(ast->as.assign.dest, dumptype);
      printf(" ");
      ast_dump(ast->as.assign.expr, dumptype);
      printf(")");
      break;
    case A_FUNCALL:
      printf("FUNCALL(" SV_FMT " ",
          SV_UNPACK(ast->as.funcall.name.image));
      ast_dump(ast->as.funcall.params, dumptype);
      printf(")");
      break;
    case A_PARAM:
      printf("PARAM(");
      ast_dump(ast->as.astlist.ast, dumptype);
      printf(" ");
      ast_dump(ast->as.astlist.next, dumptype);
      printf(")");
      break;
    case A_GROUP:
      printf("GROUP((");
      ast_dump(ast->as.group, dumptype);
      printf(")");
      break;
  }
  if (dumptype) {
    char *str = type_dump_to_string(&ast->type);
    printf(" {%s}", str);
  }
}

#define LABEL_MAX 128
#include "../defs.h"

typedef struct {
  enum {
    B_INST,
    B_HEX,
    B_HEX2,
    B_SETLABEL,
    B_LABEL,
    B_RELLABEL,
    B_STRING,
  } kind;
  struct {
    instruction_t inst;
    int num;
    char str[LABEL_MAX];
  } arg;
} bytecode_t;

void bytecode_dump(bytecode_t b) {
  switch (b.kind) {
    case B_INST:
      printf("%s ", instruction_to_string(b.arg.inst));
      break;
    case B_HEX:
      printf("0x%02X ", b.arg.num);
      break;
    case B_HEX2:
      printf("0x%04X ", b.arg.num);
      break;
    case B_SETLABEL:
      printf("%s: ", b.arg.str);
      break;
    case B_LABEL:
      printf("%s ", b.arg.str);
      break;
    case B_RELLABEL:
      printf("$%s ", b.arg.str);
      break;
    case B_STRING:
      printf("%s ", b.arg.str);
      break;
  }
}

#define SYMBOL_MAX 128
#define SCOPE_MAX 32
#define DATA_MAX 128
#define CODE_MAX 128

typedef struct {
  token_t name;
  type_t type;
  enum {
    INFO_LOCAL,
    INFO_GLOBAL,
  } kind;
  union {
    int local;
    int global;
  } info;
} symbol_t;

typedef struct {
  symbol_t symbols[SYMBOL_MAX];
  int symbol_num;
} scope_t;

typedef struct {
  bytecode_t data[DATA_MAX];
  int data_num;
  bytecode_t init[CODE_MAX];
  int init_num;
  bytecode_t code[CODE_MAX];
  int code_num;
  bool is_init;
} compiled_t;

typedef struct {
  scope_t scopes[SCOPE_MAX];
  int scope_num;
  int sp;
  int param;
  int uli; // unique label id
  compiled_t compiled;
} state_t;

void state_push_scope(state_t *state) {
  assert(state);
  assert(state->scope_num + 1 < SCOPE_MAX);
  ++ state->scope_num;
}

void state_drop_scope(state_t *state) {
  assert(state);
  assert(state->scope_num > 0);
  -- state->scope_num;
}

symbol_t *state_find_symbol(state_t *state, token_t token) {
  assert(state);
  assert(state->scope_num >= 1);
  for (int j = state->scope_num-1; j >= 0; --j) {
    scope_t *scope = &state->scopes[j];
    for (int i = 0; i < scope->symbol_num; ++i) {
      if (sv_eq(scope->symbols[i].name.image, token.image)) {
        return &scope->symbols[i];
      }
    }
  }
  eprintf(token, "symbol not declared: " SV_FMT, SV_UNPACK(token.image));
  assert(0);
}

void state_add_local_symbol(state_t *state, token_t token, type_t type) {
  assert(state);
  assert(state->scope_num >= 1);
  for (int j = state->scope_num-1; j >= 0; --j) {
    scope_t *scope = &state->scopes[j];
    for (int i = 0; i < scope->symbol_num; ++i) {
      if (sv_eq(scope->symbols[i].name.image, token.image)) {
        eprintf(token, "redefinition of symbol '" SV_FMT "', defined at %d:%d", SV_UNPACK(token.image), scope->symbols[i].name.loc.row, scope->symbols[i].name.loc.col);
      }
    }
  }
  scope_t *scope = &state->scopes[state->scope_num-1];
  assert(scope->symbol_num + 1 < SYMBOL_MAX);
  scope->symbols[scope->symbol_num ++] = (symbol_t) {token, type, INFO_LOCAL, {state->sp}};
  state->sp ++;
}

void state_add_global_symbol(state_t *state, token_t token, type_t type, int id) {
  assert(state);
  scope_t *scope = &state->scopes[0];
  for (int i = 0; i < scope->symbol_num; ++i) {
    if (sv_eq(scope->symbols[i].name.image, token.image)) {
      eprintf(token, "redefinition of symbol '" SV_FMT "', defined at %d:%d", SV_UNPACK(token.image), scope->symbols[i].name.loc.row, scope->symbols[i].name.loc.col);
    }
  }
  assert(scope->symbol_num + 1 < SYMBOL_MAX);
  scope->symbols[scope->symbol_num ++] = (symbol_t) {token, type, INFO_GLOBAL, {.global = id}};
} 

void code_dump(compiled_t *compiled) {
  assert(compiled);

  printf("GLOBAL _start\n");
  for (int i = 0; i < compiled->data_num; ++i) {
    bytecode_dump(compiled->data[i]);
  }
  if (compiled->data_num > 0) { printf("\n"); }
  printf("_start:\n\t");
  for (int i = 0; i < compiled->init_num; ++i) {
    bytecode_dump(compiled->init[i]);
  }
  if (compiled->init_num > 0) { printf("\n"); }
  printf("\tJMPR $main\n");
  for (int i = 0; i < compiled->code_num; ++i) {
    if (i != 0 && compiled->code[i].kind == B_SETLABEL) { printf("\n"); }
    bytecode_dump(compiled->code[i]);
  }
  printf("\n");
}

type_t parse_type(tokenizer_t *tokenizer) {
  assert(tokenizer);

  type_t type = {0};
  token_t token = token_expect(tokenizer, T_SYM);

  if (sv_eq(token.image, sv_from_cstr("int"))) {
    type.kind = TY_INT;
  } else if (sv_eq(token.image, sv_from_cstr("char"))) {
    type.kind = TY_CHAR;
  } else if (sv_eq(token.image, sv_from_cstr("void"))) {
    type.kind = TY_VOID;
  } else {
    eprintf(token, "is not a type");
  } 

  if (token_next_if_kind(tokenizer, T_STAR)) {
    type = (type_t) {TY_PTR, {.ptr = type_malloc(type)}};
  }

  return type;
}

ast_t *parse_expr(tokenizer_t *tokenizer);
ast_t *parse_param(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t par = token_expect(tokenizer, T_PARO);
  if (token_next_if_kind(tokenizer, T_PARC)) {
    return NULL;
  }

  ast_t *expr = parse_expr(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_PARAM, par, {0}, {.astlist = {expr, NULL}}});
  ast_t *asti = ast;

  while (token_next_if_kind(tokenizer, T_COLON)) {
    expr = parse_expr(tokenizer);
    asti->as.astlist.next = ast_malloc((ast_t){A_PARAM, par, {0}, {.astlist = {expr, NULL}}});
    asti = asti->as.astlist.next;
  }

  token_expect(tokenizer, T_PARC);

  return ast;
}

ast_t *parse_funccall(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *param = parse_param(tokenizer);

  return ast_malloc((ast_t){A_FUNCALL, name, {0}, {.funcall = {name, param}}});
}

ast_t *parse_fac(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);

  if (token.kind == T_PARO) {
    token_next(tokenizer);
    ast_t *expr = parse_expr(tokenizer);
    token_expect(tokenizer, T_PARC);
    return ast_malloc((ast_t){A_GROUP, token, {0}, {.group = expr}});
  };

  tokenizer_t savetok = *tokenizer;
  if (token.kind == T_SYM) {
    token_next(tokenizer);
    if (token_peek(tokenizer).kind == T_PARO) {
      *tokenizer = savetok;
      return parse_funccall(tokenizer);
    }
    *tokenizer = savetok;
  }

  if (token.kind != T_INT && 
      token.kind != T_SYM && 
      token.kind != T_STRING && 
      token.kind != T_HEX) {
    eprintf(token, "unvalid token for fac: '%s'", 
        token_kind_to_string(token.kind));
  }
  token_next(tokenizer);

  return ast_malloc((ast_t){A_FAC, token, {0}, {.fac = token}});
}

ast_t *parse_unary(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);
  switch (token.kind) {
    case T_PLUS: case T_MINUS:
    case T_AND: case T_STAR:
      token_next(tokenizer);
      ast_t *arg = parse_fac(tokenizer);
      return ast_malloc((ast_t){A_UNARYOP, token, {0}, {.unaryop = {token, arg}}});
    default:
      break;
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

ast_t *parse_decl(tokenizer_t *tokenizer) {
  assert(tokenizer);
  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *expr = NULL;
  if (token_next_if_kind(tokenizer, T_EQUAL)) {
    expr = parse_expr(tokenizer);
  }
  token_expect(tokenizer, T_SEMICOLON);
  return ast_malloc((ast_t){A_DECL, name, {0}, {.decl = {type, name, expr}}});
}

ast_t *parse_statement(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);

  if (token.kind == T_RETURN) {
    token_next(tokenizer);
    ast_t *expr = NULL;
    if (!token_next_if_kind(tokenizer, T_SEMICOLON)) {
      expr = parse_expr(tokenizer);
      token_expect(tokenizer, T_SEMICOLON);
    }
    return ast_malloc((ast_t){A_RETURN, token, {0}, {.return_ = {expr}}});
  }

  tokenizer_t savetok = *tokenizer;
  catch = true;
  if (setjmp(catch_buf) == 0) {
    ast_t *ast = parse_decl(tokenizer);
    catch = false;
    return ast;
  }
  *tokenizer = savetok;

  savetok = *tokenizer;
  catch = true;
  if (setjmp(catch_buf) == 0) {
    token_t token = token_peek(tokenizer);
    ast_t *dest = NULL;
    switch (token.kind) {
      case T_STAR: 
        dest = parse_expr(tokenizer);
        break;
      case T_SYM:
        token_next(tokenizer);
        dest = ast_malloc((ast_t){A_FAC, token, {0}, {.fac = token}});
        break;
      default:
        eprintf(token, "assertion");
    }
    assert(dest);
    token_expect(tokenizer, T_EQUAL);
    ast_t *expr = parse_expr(tokenizer);
    ast_t *ast = ast_malloc((ast_t){A_ASSIGN, dest->forerror, {0}, {.assign = {dest, expr}}});
    token_expect(tokenizer, T_SEMICOLON);
    catch = false;
    return ast;
  }
  *tokenizer = savetok;

  catch = true;
  if (setjmp(catch_buf) == 0) {
    ast_t *ast = parse_expr(tokenizer);
    token_expect(tokenizer, T_SEMICOLON);
    catch = false;
    return ast;
  }

  eprintf(token, "expected a statement");
  assert(0);
}

ast_t *parse_code(tokenizer_t *tokenizer) {
  assert(tokenizer);

  return parse_statement(tokenizer);
}

ast_t *parse_block(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_expect(tokenizer, T_BRO);

  if (token_next_if_kind(tokenizer, T_BRC)) {
    return NULL;
  }

  ast_t ast = {A_BLOCK, token, {0}, {.astlist = {parse_code(tokenizer), NULL}}};
  ast_t *block = &ast;
  ast_t *code = NULL;

  while (token_peek(tokenizer).kind != T_BRC) {
    code = parse_code(tokenizer);
    if (code) {
      block->as.astlist.next = ast_malloc((ast_t){A_BLOCK, token, {0}, {.astlist = {code, NULL}}});
      block = block->as.astlist.next;
    }
  } 

  token_expect(tokenizer, T_BRC);

  return ast_malloc(ast);
}

ast_t *parse_paramdef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t par = token_expect(tokenizer, T_PARO);
  if (token_next_if_kind(tokenizer, T_PARC)) {
    return NULL;
  }

  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *ast = ast_malloc((ast_t){A_PARAMDEF, par, {0}, {.paramdef = {type, name, NULL}}});
  ast_t *asti = ast;

  // TODO maybe change the forerror

  while (token_next_if_kind(tokenizer, T_COLON)) {
    type = parse_type(tokenizer);
    name = token_expect(tokenizer, T_SYM); 
    asti->as.paramdef.next = ast_malloc((ast_t){A_PARAMDEF, par, {0}, {.paramdef = {type, name, NULL}}});
    asti = asti->as.paramdef.next;
  }

  token_expect(tokenizer, T_PARC);

  return ast;
}

ast_t *parse_funcdecl(tokenizer_t *tokenizer) {
  assert(tokenizer);

  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);

  ast_t *param = parse_paramdef(tokenizer);

  ast_t *block = parse_block(tokenizer);

  return ast_malloc((ast_t){A_FUNCDECL, name, {0}, {.funcdecl = {type, name, param, block}}});
} 

ast_t *parse_global(tokenizer_t *tokenizer) {
  assert(tokenizer);

  tokenizer_t savetok = *tokenizer;
  catch = true;
  if (setjmp(catch_buf) == 0) {
    ast_t *ast = parse_funcdecl(tokenizer);
    catch = false;
    return ast;
  }
  *tokenizer = savetok;

  ast_t *decl = parse_decl(tokenizer);
  decl->kind = A_GLOBDECL;

  return decl;
}

ast_t *parse(tokenizer_t *tokenizer) {
  assert(tokenizer);

  if (token_peek(tokenizer).kind == T_NONE) {
    return NULL;
  }

  ast_t *glob = parse_global(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_GLOBAL, glob->forerror, {0}, {.astlist = {glob, NULL}}});
  ast_t *asti = ast;

  while (token_peek(tokenizer).kind != T_NONE) {
    ast_t *glob = parse_global(tokenizer);
    asti->as.astlist.next = ast_malloc((ast_t){A_GLOBAL, glob->forerror, {0}, {.astlist = {glob, NULL}}});
    asti = asti->as.astlist.next;
  }

  return ast;
}

void typecheck(ast_t *ast, state_t *state);
void typecheck_expect(ast_t *ast, state_t *state, type_t type) {
  assert(ast);
  typecheck(ast, state);

  if (type_cmp(&ast->type, &type)) { return; }
  char *expectstr = type_dump_to_string(&type);
  char *foundstr = type_dump_to_string(&ast->type);
  eprintf(ast->forerror, "expected '%s', found '%s'", expectstr, foundstr);
}

void typecheck_expandable(ast_t *ast, state_t *state, type_t type) {
  assert(ast);
  typecheck(ast, state);

  if (type_cmp(&ast->type, &type)) { return; }
  if (type.kind == TY_INT && ast->type.kind == TY_CHAR) {
    return;
  }

  typecheck_expect(ast, state, type);
}

void typecheck_funcbody(ast_t *ast, state_t *state, type_t ret) {
  assert(ast);
  assert(state);
  assert(ret.kind != TY_FUNC);
  assert(ast->kind == A_BLOCK);

  ast_t *code = ast->as.astlist.ast;
  assert(code);

  if (code->kind == A_RETURN) {
    typecheck_expandable(code, state, ret);
  } else {
    typecheck(code, state);
  } 
  if (ast->as.astlist.next) {
    typecheck_funcbody(ast->as.astlist.next, state, ret);
  }

  ast->type.kind = TY_VOID;
}

void typecheck(ast_t *ast, state_t *state) {
  assert(state);

  if (!ast) { return; }

  switch (ast->kind) {
    case A_NONE:
      assert(0);
    case A_GLOBAL:
    case A_BLOCK:
      assert(ast->as.astlist.ast);
      typecheck(ast->as.astlist.ast, state);
      typecheck(ast->as.astlist.next, state);
      ast->type.kind = TY_VOID;
      break;
    case A_FUNCDECL:
      {
        state_push_scope(state);
        state->param = 0;
        typecheck(ast->as.funcdecl.params, state);
        ast->type = (type_t) {TY_FUNC, {.func = {type_malloc(ast->as.funcdecl.type), ast->as.funcdecl.params ? type_malloc(ast->as.funcdecl.params->type) : NULL}}};
        state_add_global_symbol(state, ast->as.funcdecl.name, ast->type, 0);

        if (ast->as.funcdecl.block) {
          typecheck_funcbody(ast->as.funcdecl.block, state, ast->as.funcdecl.type);
        }
        state_drop_scope(state);
      } break;
    case A_PARAMDEF:
      {
        scope_t *scope = &state->scopes[state->scope_num-1];
        assert(scope->symbol_num + 1 < SYMBOL_MAX);
        ++state->param;
        scope->symbols[scope->symbol_num ++] = (symbol_t) {ast->as.paramdef.name, ast->as.paramdef.type, INFO_LOCAL, {-1-state->param}};

        typecheck(ast->as.paramdef.next, state);
        ast->type = (type_t){TY_PARAM, {.param = {type_malloc(ast->as.paramdef.type), ast->as.paramdef.next ? type_malloc(ast->as.paramdef.next->type) : NULL}}};
      } break;
    case A_RETURN:
      typecheck(ast->as.return_.expr, state);
      if (ast->as.return_.expr) {
        ast->type = type_clone(ast->as.return_.expr->type);
      } else {
        ast->type.kind = TY_VOID;
      }
      break;
    case A_BINARYOP:
      assert(ast->as.binaryop.lhs);
      assert(ast->as.binaryop.rhs);
      typecheck(ast->as.binaryop.lhs, state);
      if (ast->as.binaryop.lhs->type.kind != TY_INT && ast->as.binaryop.lhs->type.kind != TY_PTR) {
        char *typestr = type_dump_to_string(&ast->as.binaryop.lhs->type);
        eprintf(ast->forerror, "expected 'INT' or 'PTR ...' found '%s'", typestr);
      }
      typecheck_expandable(ast->as.binaryop.rhs, state, (type_t){TY_INT, {{0}}});
      ast->type = type_clone(ast->as.binaryop.lhs->type);
      break;
    case A_UNARYOP:
      assert(ast->as.unaryop.arg);
      switch (ast->as.unaryop.op.kind) {
        case T_MINUS:
          typecheck_expandable(ast->as.unaryop.arg, state, (type_t){TY_INT, {{0}}});
          ast->type.kind = TY_INT;
          break;
        case T_AND:
          typecheck(ast->as.unaryop.arg, state);
          ast->type = (type_t){TY_PTR, {.ptr = type_malloc(ast->as.unaryop.arg->type)}};
          break;
        case T_STAR:
          typecheck(ast->as.unaryop.arg, state);
          if (ast->as.unaryop.arg->type.kind != TY_PTR) {
            char *type = type_dump_to_string(&ast->as.unaryop.arg->type);
            eprintf(ast->as.unaryop.arg->forerror, "cannot dereference non PTR type: '%s'", type);
          }
          ast->type = type_clone(*ast->as.unaryop.arg->type.as.ptr);
          break;
        default:
          assert(0);
      }
      break;
    case A_FAC:
      if (ast->as.fac.kind == T_INT) {
        ast->type.kind = TY_INT;
      } else if (ast->as.fac.kind == T_SYM) {
        symbol_t *s = state_find_symbol(state, ast->as.fac);
        ast->type = type_clone(s->type);
      } else if (ast->as.fac.kind == T_STRING) {
        ast->type = (type_t){TY_PTR, {.ptr = type_malloc((type_t){TY_CHAR, {{0}}})}};
      } else if (ast->as.fac.kind == T_HEX) {
        if (ast->as.fac.image.len - 2 == 2) {
          ast->type.kind = TY_CHAR;
        } else {
          ast->type.kind = TY_INT;
        }
      } else {
        assert(0);
      }
      break;
    case A_GLOBDECL:
      if (ast->as.decl.expr) {
        typecheck_expandable(ast->as.decl.expr, state, ast->as.decl.type);
      }
      state_add_global_symbol(state, ast->as.decl.name, ast->as.decl.type, 0);
      ast->type.kind = TY_VOID;
      break;
    case A_DECL:
      if (ast->as.decl.expr) {
        typecheck_expandable(ast->as.decl.expr, state, ast->as.decl.type);
      }
      state_add_local_symbol(state, ast->as.decl.name, ast->as.decl.type);
      ast->type.kind = TY_VOID;
      break;
    case A_ASSIGN:
      assert(ast->as.assign.dest);
      assert(ast->as.assign.expr);
      typecheck(ast->as.assign.dest, state);
      typecheck_expect(ast->as.assign.expr, state, ast->as.assign.dest->type);
      ast->type = type_clone(ast->as.assign.dest->type);
      break;
    case A_FUNCALL:
      {
        symbol_t *s = state_find_symbol(state, ast->as.funcall.name);
        if (s->type.kind != TY_FUNC) {
          char *foundstr = type_dump_to_string(&s->type);
          eprintf(ast->forerror, "expected 'TY_FUNC', found '%s'", foundstr);
        }
        if (s->type.as.func.params) {
          typecheck_expect(ast->as.funcall.params, state, *s->type.as.func.params);
        }
        assert(s->type.as.func.ret);
        ast->type = type_clone(*s->type.as.func.ret);
      } break;
    case A_PARAM:
      typecheck(ast->as.astlist.ast, state);
      typecheck(ast->as.astlist.next, state);
      assert(ast->as.astlist.ast);
      ast->type = (type_t){TY_PARAM, {.param = {type_malloc(ast->as.astlist.ast->type), ast->as.astlist.next ? type_malloc(ast->as.astlist.next->type) : NULL}}};
      break;
    case A_GROUP:
      assert(ast->as.group);
      typecheck(ast->as.group, state);
      ast->type = type_clone(ast->as.group->type);
      break;
  }
}

void data(compiled_t *compiled, bytecode_t b) {
  assert(compiled);
  assert(compiled->data_num + 1 < DATA_MAX);
  compiled->data[compiled->data_num ++] = b;
}

void code(compiled_t *compiled, bytecode_t b) {
  assert(compiled);
  if (compiled->is_init) {
    assert(compiled->init_num + 1 < CODE_MAX);
    compiled->init[compiled->init_num ++] = b;
  } else {
    assert(compiled->code_num + 1 < CODE_MAX);
    compiled->code[compiled->code_num ++] = b;
  }
}

// wants in B the addr
void read(compiled_t *compiled, type_t type) {
  assert(compiled);
  switch (type_size(&type)) {
    case 1:
      code(compiled, (bytecode_t){B_INST, {.inst = rB_AL}});
      break;
    case 2:
      code(compiled, (bytecode_t){B_INST, {.inst = rB_A}});
      break;
    default:
      assert(0);
  }
}

// wants in B the addr
void write(compiled_t *compiled, type_t type) {
  assert(compiled);
  switch (type_size(&type)) {
    case 1:
      code(compiled, (bytecode_t){B_INST, {.inst = AL_rB}});
      break;
    case 2:
      code(compiled, (bytecode_t){B_INST, {.inst = A_rB}});
      break;
    default:
      assert(0);
  }
}

// addr in A
void get_addr(state_t *state, token_t token) {
  assert(state);
  assert(token.kind == T_SYM);

  compiled_t *compiled = &state->compiled;
  symbol_t *s = state_find_symbol(state, token);
  switch (s->kind) {
    case INFO_LOCAL:
      {
        int num = 2*(state->sp - s->info.local);
        assert(num < 256 && num >= 2);
        code(compiled, (bytecode_t){B_INST, {.inst = SP_A}});
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_BL}});
        code(compiled, (bytecode_t){B_HEX, {.num = num}});
        code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
      } break;
    case INFO_GLOBAL:
      {
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
        bytecode_t b = {B_LABEL, {0}};
        sprintf(b.arg.str, "_%03d", s->info.global);
        code(compiled, b);
      } break;
  }
}

void compile(ast_t *ast, state_t *state) {
  assert(state);

  if (!ast) { return; }

  compiled_t *compiled = &state->compiled;

  switch (ast->kind) {
    case A_NONE: 
      assert(0);
    case A_GLOBAL:
    case A_BLOCK: 
      assert(ast->as.astlist.ast);
      compile(ast->as.astlist.ast, state);
      compile(ast->as.astlist.next, state);
      break;
    case A_FUNCDECL: 
      {
        state_push_scope(state);
        state->param = 0;
        compile(ast->as.funcdecl.params, state);
        bytecode_t b = {B_SETLABEL, {0}};
        memcpy(b.arg.str, ast->as.funcdecl.name.image.start, ast->as.funcdecl.name.image.len);
        code(compiled, b);
        compile(ast->as.funcdecl.block, state);
        if (!(compiled->code[compiled->code_num-1].kind == B_INST && 
              compiled->code[compiled->code_num-1].arg.inst == RET)) {
          for (int i = 0; i < state->sp; ++i) { code(compiled, (bytecode_t){B_INST, {.inst = INCSP}}); }
          code(compiled, (bytecode_t){B_INST, {.inst = RET}});
        }
        state_drop_scope(state);
      } break;
    case A_PARAMDEF:
      {
        scope_t *scope = &state->scopes[state->scope_num-1];
        assert(scope->symbol_num + 1 < SYMBOL_MAX);
        ++state->param;
        scope->symbols[scope->symbol_num ++] = (symbol_t) {ast->as.paramdef.name, ast->as.paramdef.type, INFO_LOCAL, {-1-state->param}};

        compile(ast->as.paramdef.next, state);
      } break;  
    case A_RETURN: 
      if (ast->as.return_.expr) { compile(ast->as.return_.expr, state); }
      for (int i = 0; i < state->sp; ++i) { code(compiled, (bytecode_t){B_INST, {.inst = INCSP}}); }
      code(compiled, (bytecode_t){B_INST, {.inst = RET}});
      break;
    case A_BINARYOP:
      assert(ast->as.binaryop.lhs);
      assert(ast->as.binaryop.rhs);
      compile(ast->as.binaryop.lhs, state);
      code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
      compile(ast->as.binaryop.rhs, state);
      switch (ast->as.binaryop.op.kind) {
        case T_PLUS: code(compiled, (bytecode_t){B_INST, {.inst = SUM}}); break;
        case T_MINUS: code(compiled, (bytecode_t){B_INST, {.inst = SUB}}); break;
        case T_STAR: TODO;
        case T_SLASH: TODO;
        default: assert(0);
      }
      break;
    case A_UNARYOP:
      assert(ast->as.unaryop.arg);
      switch (ast->as.unaryop.op.kind) {
        case T_MINUS:
          compile(ast->as.unaryop.arg, state);
          code(compiled, (bytecode_t){B_INST, {.inst = RAM_BL}});
          code(compiled, (bytecode_t){B_HEX, {.num = 0}});
          code(compiled, (bytecode_t){B_INST, {.inst = SUB}});
          break;
        case T_AND:
          {
            assert(ast->as.unaryop.arg->kind == A_FAC);
            get_addr(state, ast->as.unaryop.arg->as.fac);
          } break;
        case T_STAR:
          compile(ast->as.unaryop.arg, state);
          assert(ast->as.unaryop.arg->type.kind == TY_PTR);
          code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
          read(compiled, ast->type);
          break;
        default:
          assert(0);
      }
      break;
    case A_FAC: 
      {
        token_t token = ast->as.fac;
        switch (token.kind) {
          case T_INT:
            {
              int num = atoi(token.image.start);
              if (num >= 256) {
                code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
                code(compiled, (bytecode_t){B_HEX2, {.num = num}});
              } else {
                code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
                code(compiled, (bytecode_t){B_HEX, {.num = num}});

              }
            } break;
          case T_SYM:
            get_addr(state, token);
            code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
            read(compiled, ast->type);
            break;
          case T_STRING:
            {
              code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
              bytecode_t b = {B_SETLABEL, {0}};
              sprintf(b.arg.str, "_%03d", state->uli);
              data(compiled, b);
              b.kind = B_LABEL;
              code(compiled, b);
              b.kind = B_STRING;
              memset(b.arg.str, 0, LABEL_MAX);
              memcpy(b.arg.str, token.image.start, token.image.len);
              data(compiled, b);
              data(compiled, (bytecode_t){B_HEX, {.num = 0}});
              ++ state->uli;
            } break;
          case T_HEX:
            if (token.image.len - 2 == 2) {
              code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
              code(compiled, (bytecode_t){B_HEX, {.num = strtol(token.image.start+2, NULL, 16)}});
            } else {
              code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
              code(compiled, (bytecode_t){B_HEX2, {.num = strtol(token.image.start+2, NULL, 16)}});
            }
            break;
          default:
            assert(0);
        }
      } break;
    case A_GLOBDECL:
      {
        state_add_global_symbol(state, ast->as.decl.name, ast->as.decl.type, state->uli);
        bytecode_t b = {B_SETLABEL, {0}};
        sprintf(b.arg.str, "_%03d", state->uli);
        ++ state->uli;
        data(compiled, b);

        if (ast->as.decl.expr) {
          if (ast->as.decl.expr->kind == A_FAC && ast->as.decl.expr->as.fac.kind == T_STRING) {
            token_t token = ast->as.decl.expr->as.fac;
            b.kind = B_STRING;
            memset(b.arg.str, 0, LABEL_MAX);
            memcpy(b.arg.str, token.image.start, token.image.len);
            data(compiled, b);
            return;
          } 
          compiled->is_init = true;
          compile(ast->as.decl.expr, state);
          code(compiled, (bytecode_t){B_INST, {.inst = RAM_B}});
          b.kind = B_LABEL;
          code(compiled, b);
          write(compiled, ast->as.decl.type);
          compiled->is_init = false;
        }
        for (int i = 0; i < type_size(&ast->as.decl.type); ++i) {
          data(compiled, (bytecode_t){B_HEX, {.num = 0}});
        }
      } break;
    case A_DECL:
      state_add_local_symbol(state, ast->as.decl.name, ast->as.decl.type);
      if (ast->as.decl.expr) {
        compile(ast->as.decl.expr, state);
      }
      code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
      break;
    case A_ASSIGN:
      switch (ast->as.assign.dest->kind) {
        case A_FAC: 
          {
            symbol_t *s = state_find_symbol(state, ast->as.assign.dest->as.fac);
            if (s->kind == INFO_LOCAL) {
              int num = 2*(state->sp - s->info.local);
              assert(num < 256 && num >= 2);
              if (num == 2) {
                compile(ast->as.assign.expr, state);
                code(compiled, (bytecode_t){B_INST, {.inst = INCSP}});
                code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
                return;
              }
            }
            get_addr(state, ast->as.assign.dest->as.fac);
          } break;
        case A_UNARYOP:
          compile(ast->as.assign.dest->as.unaryop.arg, state);
          break;
        default:
          assert(0);
      }
      if (ast->as.assign.expr->kind == A_FAC) {
        code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
        compile(ast->as.assign.expr, state);
      } else {
        code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
        ++ state->sp;
        compile(ast->as.assign.expr, state);
        code(compiled, (bytecode_t){B_INST, {.inst = POPB}});
        -- state->sp;
      }
      write(compiled, ast->type);
      break;
    case A_FUNCALL:
      {
        compile(ast->as.funcall.params, state);
        code(compiled, (bytecode_t){B_INST, {.inst = CALLR}});
        bytecode_t b = {B_RELLABEL, {0}};
        memcpy(b.arg.str, ast->as.funcall.name.image.start, ast->as.funcall.name.image.len);
        code(compiled, b);
      } break;
    case A_PARAM:
      compile(ast->as.astlist.next, state);
      compile(ast->as.astlist.ast, state);
      code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
      ++ state->sp;
      break;
    case A_GROUP:
      compile(ast->as.group, state);
      break;
  }
}

bool compiled_is_inst(compiled_t *compiled, int i, instruction_t inst) {
  assert(compiled);

  if (i >= compiled->code_num) { return false; }
  if (compiled->code[i].kind != B_INST) { return false; }
  if (compiled->code[i].arg.inst != inst) { return false; }
  return true;
}

void compiled_copy(compiled_t *compiled, int i, int a, int n) {
  assert(compiled);
  assert(n >= a+1);
  memcpy(&compiled->code[i+a], &compiled->code[i+n], (compiled->code_num-i-n)*sizeof(bytecode_t));
  compiled->code_num -= n-a;
}

void optimize_compiled(compiled_t *compiled) {
  assert(compiled);

  // TODO: pusha incsp -> nothing

  for (int i = 0; i < compiled->code_num;) {
    if (compiled_is_inst(compiled, i, PEEKA) && compiled_is_inst(compiled, i+1, A_B)) {
      compiled->code[i] = (bytecode_t){B_INST, {.inst = PEEKB}};
      compiled_copy(compiled, i, 1, 2);
      i = 0;
    } else if (compiled_is_inst(compiled, i, RAM_A) && compiled_is_inst(compiled, i+2, A_B)) {
      compiled->code[i] = (bytecode_t){B_INST, {.inst = RAM_B}};
      compiled_copy(compiled, i, 2, 3);
      i = 0;
    } else if (compiled_is_inst(compiled, i, PUSHA) && compiled_is_inst(compiled, i+1, PEEKA)) {
      compiled_copy(compiled, i, 1, 2);
      i = 0;
    } else if (compiled_is_inst(compiled, i, PEEKAR) && compiled->code[i+1].arg.num == 2){
      compiled->code[i] = (bytecode_t){B_INST, {.inst = PEEKA}};
      compiled_copy(compiled, i, 1, 2);
      i = 0;
    } else if (compiled_is_inst(compiled, i, SP_A) && 
        compiled_is_inst(compiled, i+1, RAM_BL) &&
        compiled_is_inst(compiled, i+3, SUM) &&
        compiled_is_inst(compiled, i+4, A_B) &&
        compiled_is_inst(compiled, i+5, rB_A)) {
      compiled->code[i] = (bytecode_t){B_INST, {.inst = PEEKAR}};
      compiled->code[i+1] = compiled->code[i+2];
      compiled_copy(compiled, i, 2, 6);
      i = 0;
    } else {
      ++i;
    }
  } 
}

void help(int errorcode) {
  fprintf(stderr, 
      "Usage: simpleC [options] [files]\n\n"
      "Options:\n"   
      " -d [<module>]  enable debug options for a module\n"
      " -D [<module>]  stop the execution after a module and print the output\n"
      "                (if no module name or module all then it will execute only the tokenizer)\n"
      " -e <string>    compile the string provided\n"
      " -o <file>      write output to the file\n"
      " -O1            optimize the assembly code\n"
      " -O2            optimize pre-compilation\n"
      " -O3            optimize both ways\n"
      " -h | --help    print this page and exit\n\n"
      "Modules:\n"
      "no module name is enables all the modules\n"
      "  all           all the modules\n"
      "  tok           tokenizer\n"
      "  par           parser\n"
      "  typ           typechecker\n"
      "  com           compiler to assembly\n");
  exit(errorcode);
}

#define STR_INPUT_MAX 128
typedef enum {
  M_TOK,
  M_PAR,
  M_TYP,
  M_COM,
  M_COUNT
} module_t;

typedef enum {
  OL_POST,
  OL_PRE,
  OL_COUNT
} optlevel_t;

uint8_t parse_module(char *str) {
  assert(M_COUNT < 8);

  if (strcmp(str, "all") == 0) {
    return (1 << M_COUNT) - 1;
  } else if (strcmp(str, "tok") == 0) {
    return 1 << M_TOK;
  } else if (strcmp(str, "par") == 0) {
    return 1 << M_PAR;
  } else if (strcmp(str, "typ") == 0) {
    return 1 << M_TYP;
  } else if (strcmp(str, "com") == 0) {
    return 1 << M_COM;
  } else {
    fprintf(stderr, "ERROR: unknown module '%s'\n", str);
    help(1);
  }
  assert(0);
}

int main(int argc, char **argv) {
  if (argc == 1) {
    fprintf(stderr, "ERROR: no input\n"); exit(1);
  } 

  assert(atexit(free_all) == 0);

  char *strinput[STR_INPUT_MAX] = {0};
  int strinputi = 0;
  assert(OL_COUNT < 8);
  uint8_t opt = 0;
  assert(M_COUNT < 8);
  uint8_t debug = 0;
  uint8_t exitat = 0;

  char *arg = NULL;
  ++ argv;
  while (*argv) {
    arg = *argv;
    if (arg[0] == '-') {
      switch (arg[1]) {
        case 'd':
          if (!*(argv + 1)) {
            debug = (1 << M_COUNT) - 1;
          } else {
            ++ argv;
            debug = parse_module(*argv);
          }
          ++ argv;
          break;
        case 'D':
          if (!*(argv + 1)) {
            debug = (1 << M_COUNT) - 1;
            exitat = (1 << M_COUNT) - 1;
          } else {
            ++ argv;
            debug = parse_module(*argv);
            exitat = parse_module(*argv);
          }
          ++ argv;
          break;
        case 'e':
          assert(strinputi + 1 < STR_INPUT_MAX);
          ++argv;
          if (!*argv) {
            fprintf(stderr, "ERROR: -e expects a string\n");
            help(1);
          }
          strinput[strinputi ++] = *argv;
          ++argv;
          break;
        case 'o':
          TODO;
        case 'O':
          opt = atoi(arg+2);
          ++ argv;
          break;
        case 'h':
          help(0);
          break;
        case '-':
          if (strcmp(arg + 2, "help") == 0) {
            help(0);
          }
          __attribute__((fallthrough));
        default:
          fprintf(stderr, "ERROR: unknown flag '%s'", arg);
          help(1);
      }
    } else {
      TODO;
    }
  }

  tokenizer_t tokenizer = {0};
  state_t state;
  ast_t *ast;
  for (int i = 0; i < strinputi; ++i) {
    tokenizer_init(&tokenizer, strinput[i], "cmd");
    if ((debug >> M_TOK) & 1) {
      printf("TOKENS:\n");
      token_t token;
      while ((token = token_next(&tokenizer)).kind != T_NONE) {
        token_dump(token);
      }
      tokenizer_init(&tokenizer, strinput[i], "cmd");
    }
    if ((exitat >> M_TOK) & 1) { exit(0); }

    ast = parse(&tokenizer);
    if ((debug >> M_PAR) & 1) {
      printf("AST:\n");
      ast_dump(ast, false);
    }
    if ((exitat >> M_PAR) & 1) { exit(0); }

    state = (state_t){0};
    typecheck(ast, &state);
    if ((debug >> M_TYP) & 1) {
      printf("TYPED AST:\n");
      ast_dump(ast, true);
    }
    if ((exitat >> M_TYP) & 1) { exit(0); }

    // check if main
    bool has_main = false;
    scope_t *scope = &state.scopes[0];
    for (int i = 0; i < scope->symbol_num; ++i) {
      if (sv_eq(scope->symbols[i].name.image, sv_from_cstr("main"))) {
        has_main = true;
        break;
      }
    }
    if (!has_main) {
      fprintf(stderr, "ERROR: no main function found\n");
      exit(1);
    }

    if ((opt >> OL_PRE) & 1) {
      TODO;
    }

    state = (state_t){0};
    compile(ast, &state);

    if ((opt >> OL_POST) & 1) {
      optimize_compiled(&state.compiled);
    }

    if ((debug >> M_COM) & 1) {
      printf("ASSEMBLY:\n");
      code_dump(&state.compiled);
    }
    if ((exitat >> M_COM) & 1) { exit(0); }
  }

  return 0;
}

