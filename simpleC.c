#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SV_IMPLEMENTATION
#include "jaris/instructions.h"

#define TODO                          \
  do {                                \
    printf("TODO at %d\n", __LINE__); \
    exit(1);                          \
  } while (0);

#define ALLOCED_MAX 2048 * 2048
void *alloced[ALLOCED_MAX];
int alloced_num = 0;

void *alloc(int size) {
  assert(alloced_num + 1 < ALLOCED_MAX);
  void *ptr = malloc(size);
  alloced[alloced_num++] = ptr;
  return ptr;
}

void free_ptr(void *ptr) {
  assert(ptr);
  for (int i = alloced_num - 1; i >= 0; --i) {
    if (alloced[i] == ptr) {
      assert(alloced[i] != NULL);
      free(alloced[i]);
      memcpy(&alloced[i], &alloced[i + 1], (alloced_num - i) * sizeof(void *));
      --alloced_num;
      return;
    }
  }
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
  T_SQO,
  T_SQC,
  T_BRO,
  T_BRC,
  T_RETURN,
  T_TYPEDEF,
  T_STRUCT,
  T_SEMICOLON,
  T_PLUS,
  T_MINUS,
  T_STAR,
  T_SLASH,
  T_EQUAL,
  T_AND,
  T_COMMA,
  T_DOT,
  T_VOIDKW,
  T_INTKW,
  T_CHARKW,
  T_CHAR,
  T_ENUM,
  T_ASM,
  T_IF,
  T_ELSE,
  T_EQ,
  T_NEQ,
  T_NOT,
  T_FOR,
  T_WHILE,
  T_EXTERN,
  T_SHL,
  T_SHR,
  T_BREAK,
  T_DEFINE,
} token_kind_t;

typedef struct {
  char *filename;
  sv_t line;
  unsigned int row, col;
  unsigned int len;
} location_t;

#define LOCATION_FMT             "%s:%d:%d"
#define LOCATION_UNPACK(__loc__) (__loc__).filename, (__loc__).row, (__loc__).col

location_t location_union(location_t a, location_t b) {
  location_t c = a;
  if (a.row != b.row) {
    c.len = a.line.len;
  } else {
    c.len = b.col + b.len - a.col;
  }
  return c;
}

typedef struct {
  token_kind_t kind;
  sv_t image;
  location_t loc;
  int asint;
} token_t;

#define MACRO_TOKEN_MAX 16
#define MACRO_MAX       16

typedef struct {
  sv_t name;
  token_t tokens[MACRO_TOKEN_MAX];
  int token_count;
} macro_t;

typedef struct {
  char *buffer;
  location_t loc;
  token_t last_token;
  bool has_last_token;
  macro_t macros[MACRO_MAX];
  int macro_count;
  int current_macro;
  int current_macro_token_index;
} tokenizer_t;

void tokenizer_init(tokenizer_t *tokenizer, char *buffer, char *filename) {
  assert(tokenizer);
  char *row_end = buffer;
  while (*row_end != '\n' && *row_end != '\0') {
    ++row_end;
  }
  *tokenizer = (tokenizer_t){0};
  tokenizer->buffer = buffer;
  tokenizer->loc = (location_t){filename, (sv_t){buffer, row_end - buffer}, 1, 1, 1};
  tokenizer->current_macro = -1;
}

void print_location(location_t location) {
  fprintf(stderr,
          "%*d | %*.*s\n",
          location.row < 1000 ? 3 : 5,
          location.row,
          location.line.len,
          location.line.len,
          location.line.start);
  fprintf(stderr,
          "%s   %*.*s%c%*.*s\n",
          location.row < 1000 ? "   " : "     ",
          location.col - 1,
          location.col - 1,
          "                                                                    "
          "                                                                    "
          "                                                                ",
          '^',
          location.len - 1,
          location.len - 1,
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

static bool dev_flag = false;

static bool catch = false;
static jmp_buf catch_buf;
#define eprintf(__loc, ...) eprintf_impl((__loc), __LINE__, __FUNCTION__, __VA_ARGS__)
void eprintf_impl(location_t location, int line, const char *func, char *fmt, ...) {
  if (catch) {
    catch = false;
    longjmp(catch_buf, 1);
  }
  if (dev_flag) {
    fprintf(stderr, "ERROR throw at %d in %s\n", line, func);
  }
  fprintf(stderr, "ERROR:%s:%d:%d: ", location.filename, location.row, location.col);
  va_list argptr;
  va_start(argptr, fmt);
  vfprintf(stderr, fmt, argptr);
  va_end(argptr);
  fprintf(stderr, "\n");
  print_location(location);
  exit(1);
}

char *token_kind_to_string(token_kind_t kind) {
  // clang-format off
  switch (kind) {
    case T_NONE: return "NONE";
    case T_SYM: return "SYM";
    case T_INT: return "INT";
    case T_HEX: return "HEX";
    case T_STRING: return "STRING";
    case T_PARO: return "PARO";
    case T_PARC: return "PARC";
    case T_SQO: return "SQO";
    case T_SQC: return "SQC";
    case T_BRO: return "BRO";
    case T_BRC: return "BRC";
    case T_RETURN: return "RETURN";
    case T_TYPEDEF: return "TYPEDEF";
    case T_STRUCT: return "STRUCT";
    case T_SEMICOLON: return "SEMICOLON";
    case T_PLUS: return "PLUS";
    case T_MINUS: return "MINUS";
    case T_STAR: return "STAR";
    case T_SLASH: return "SLASH";
    case T_EQUAL: return "EQUAL";
    case T_AND: return "AND";
    case T_COMMA: return "COLON";
    case T_DOT: return "DOT";
    case T_VOIDKW: return "VOIDKW";
    case T_INTKW: return "INTKW";
    case T_CHARKW: return "CHARKW";
    case T_CHAR: return "CHAR";
    case T_ENUM: return "ENUM";
    case T_ASM: return "ASM";
    case T_IF: return "IF";
    case T_ELSE: return "ELSE";
    case T_EQ: return "EQ";
    case T_NEQ: return "NEQ";
    case T_NOT: return "NOT";
    case T_FOR: return "FOR";
    case T_WHILE: return "WHILE";
    case T_EXTERN: return "EXTERN";
    case T_SHL: return "SHL";
    case T_SHR: return "SHR";
    case T_BREAK: return "BREAK";
    case T_DEFINE: return "DEFINE";
  }
  // clang-format on
  assert(0);
}

void token_dump(token_t token) {
  printf("%s '" SV_FMT "' @ %d:%d\n",
         token_kind_to_string(token.kind),
         SV_UNPACK(token.image),
         token.loc.row,
         token.loc.col);
}

token_t token_new_and_consume_from_buffer(token_kind_t kind, int len, tokenizer_t *tok, int asint) {
  assert(len);
  assert(tok);
  location_t loc = tok->loc;
  loc.len = len;
  token_t token = (token_t){kind, {tok->buffer, len}, loc, asint};
  tok->loc.col += len;
  tok->buffer += len;
  return token;
}

token_t token_expect(tokenizer_t *tokenizer, token_kind_t kind);
token_t token_next(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_kind_t table[128] = {0};
  table['('] = T_PARO;
  table[')'] = T_PARC;
  table['['] = T_SQO;
  table[']'] = T_SQC;
  table['{'] = T_BRO;
  table['}'] = T_BRC;
  table[';'] = T_SEMICOLON;
  table['+'] = T_PLUS;
  table['-'] = T_MINUS;
  table['*'] = T_STAR;
  table['/'] = T_SLASH;
  table['&'] = T_AND;
  table[','] = T_COMMA;
  table['.'] = T_DOT;
  table['='] = T_EQUAL;
  table['!'] = T_NOT;

  token_t token = {0};

  if (tokenizer->has_last_token) {
    tokenizer->has_last_token = false;
    token = tokenizer->last_token;

  } else if (tokenizer->current_macro != -1) {
    if (tokenizer->current_macro_token_index >= tokenizer->macros[tokenizer->current_macro].token_count) {
      tokenizer->current_macro_token_index = 0;
      tokenizer->current_macro = -1;
      return token_next(tokenizer);
    } else {
      token = tokenizer->macros[tokenizer->current_macro].tokens[tokenizer->current_macro_token_index++];
    }

  } else {
    switch (*tokenizer->buffer) {
      case '\0':
        break;
      case ' ':
      case '\t':
      case '\r':
        ++tokenizer->buffer;
        ++tokenizer->loc.col;
        return token_next(tokenizer);
      case '\n':
      {
        ++tokenizer->loc.row;
        tokenizer->loc.col = 1;
        ++tokenizer->buffer;
        tokenizer->loc.line.start = tokenizer->buffer;
        char *row_end = tokenizer->buffer;
        while (*row_end != '\n' && *row_end != '\0') {
          ++row_end;
        }
        tokenizer->loc.line.len = row_end - tokenizer->buffer;
        return token_next(tokenizer);
      }
      case '#':
      {
        int len = 1;
        while (isalpha(tokenizer->buffer[len]) || isdigit(tokenizer->buffer[len]) || tokenizer->buffer[len] == '_') {
          ++len;
        }
        token = token_new_and_consume_from_buffer(T_DEFINE, len, tokenizer, 0);
        if (sv_eq(token.image, sv_from_cstr("#define"))) {
          token_t name = token_expect(tokenizer, T_SYM);
          assert(tokenizer->macro_count + 1 < MACRO_MAX);
          macro_t *macro = &tokenizer->macros[tokenizer->macro_count];
          macro->name = name.image;
          while ((token = token_next(tokenizer)).loc.row == name.loc.row) {
            assert(macro->token_count + 1 < MACRO_TOKEN_MAX);
            macro->tokens[macro->token_count++] = token;
            if (token.kind == T_SYM && sv_eq(token.image, name.image)) {
              eprintf(token.loc, "invalid recursive macro");
            }
          }
          tokenizer->macro_count++;
        } else {
          eprintf(token.loc, "invalid directive");
        }
      } break;
      case '<':
      case '>':
        if (tokenizer->buffer[1] == tokenizer->buffer[0]) {
          token = token_new_and_consume_from_buffer(tokenizer->buffer[0] == '<' ? T_SHL : T_SHR, 2, tokenizer, 0);
        } else {
          TODO;
        }
        break;
      case '/':
        if (tokenizer->buffer[1] == '/') {
          while (tokenizer->buffer[0] != '\n') {
            ++tokenizer->buffer;
          }
          return token_next(tokenizer);
        } else if (tokenizer->buffer[1] == '*') {
          while (!(tokenizer->buffer[0] == '*' && tokenizer->buffer[1] == '/')) {
            ++tokenizer->buffer;
            ++tokenizer->loc.col;
          }
          tokenizer->buffer += 2;
          tokenizer->loc.col += 2;
          return token_next(tokenizer);
        }
        __attribute__((fallthrough));
      case '(':
      case ')':
      case '[':
      case ']':
      case '{':
      case '}':
      case '+':
      case '-':
      case '*':
      case '&':
      case ',':
      case ';':
      case '.':
      case '=':
      case '!':
        if (tokenizer->buffer[0] == '=' && tokenizer->buffer[1] == '=') {
          token = token_new_and_consume_from_buffer(T_EQ, 2, tokenizer, 0);
        } else if (tokenizer->buffer[0] == '!' && tokenizer->buffer[1] == '=') {
          token = token_new_and_consume_from_buffer(T_NEQ, 2, tokenizer, 0);
        } else {
          assert(table[(int)*tokenizer->buffer]);
          token = token_new_and_consume_from_buffer(table[(int)*tokenizer->buffer], 1, tokenizer, 0);
        }
        break;
      case '"':
      {
        int len = 1;
        do {
          ++len;
        } while (tokenizer->buffer[len] != '"');
        ++len;
        token = token_new_and_consume_from_buffer(T_STRING, len, tokenizer, 0);
      } break;
      case '\'':
        token = token_new_and_consume_from_buffer(T_CHAR, 3, tokenizer, *(tokenizer->buffer + 1));
        if (token.image.start[2] != '\'') {
          eprintf(token.loc, "CHAR can have only one char");
        }
        break;
      case '0':
        if (tokenizer->buffer[1] == 'x' || tokenizer->buffer[1] == 'X') {
          int len = 2;
          while (isdigit(tokenizer->buffer[len])
                 || ('a' <= tokenizer->buffer[len] && tokenizer->buffer[len] <= 'f')
                 || ('A' <= tokenizer->buffer[len] && tokenizer->buffer[len] <= 'F')) {
            ++len;
          }
          token = token_new_and_consume_from_buffer(T_HEX, len, tokenizer, strtol(tokenizer->buffer + 2, NULL, 16));
          if (len - 2 != 2 && len - 2 != 4) {
            eprintf(token.loc, "HEX can be 1 or 2 bytes");
          }
          break;
        }
        __attribute__((fallthrough));
      default:
        if (isdigit(*tokenizer->buffer)) {
          int len = 1;
          while (isdigit(tokenizer->buffer[len])) {
            ++len;
          }
          if (isalpha(tokenizer->buffer[len]) || tokenizer->buffer[len] == '_') {
            tokenizer->loc.len = len;
            eprintf(tokenizer->loc, "invalid integer");
          }
          token = token_new_and_consume_from_buffer(T_INT, len, tokenizer, atoi(tokenizer->buffer));
        } else if (isalpha(*tokenizer->buffer) || *tokenizer->buffer == '_') {
          int len = 1;
          while (isalpha(tokenizer->buffer[len]) || isdigit(tokenizer->buffer[len]) || tokenizer->buffer[len] == '_') {
            ++len;
          }
          sv_t image = {tokenizer->buffer, len};
          token = token_new_and_consume_from_buffer(
              sv_eq(image, sv_from_cstr("return"))  ? T_RETURN :
              sv_eq(image, sv_from_cstr("typedef")) ? T_TYPEDEF :
              sv_eq(image, sv_from_cstr("struct"))  ? T_STRUCT :
              sv_eq(image, sv_from_cstr("enum"))    ? T_ENUM :
              sv_eq(image, sv_from_cstr("int"))     ? T_INTKW :
              sv_eq(image, sv_from_cstr("char"))    ? T_CHARKW :
              sv_eq(image, sv_from_cstr("void"))    ? T_VOIDKW :
              sv_eq(image, sv_from_cstr("__asm__")) ? T_ASM :
              sv_eq(image, sv_from_cstr("if"))      ? T_IF :
              sv_eq(image, sv_from_cstr("else"))    ? T_ELSE :
              sv_eq(image, sv_from_cstr("for"))     ? T_FOR :
              sv_eq(image, sv_from_cstr("while"))   ? T_WHILE :
              sv_eq(image, sv_from_cstr("extern"))  ? T_EXTERN :
              sv_eq(image, sv_from_cstr("break"))   ? T_BREAK :
                                                      T_SYM,
              len,
              tokenizer,
              0);

        } else {
          eprintf(tokenizer->loc, "unknown char: '%c'", *tokenizer->buffer);
        }
    }
  }

  tokenizer->last_token = token;

  if (token.kind == T_SYM) {
    for (int i = 0; i < tokenizer->macro_count; ++i) {
      if (sv_eq(token.image, tokenizer->macros[i].name)) {
        tokenizer->current_macro = i;
        return token_next(tokenizer);
      }
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

token_t token_expect(tokenizer_t *tokenizer, token_kind_t kind) {
  assert(tokenizer);
  token_t token = token_next(tokenizer);
  if (token.kind != kind) {
    eprintf(token.loc,
            "expected '%s' found '%s'",
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

typedef struct {
  enum {
    ARRAY_LEN_NOTARRAY,
    ARRAY_LEN_UNSET,
    ARRAY_LEN_NUM,
    ARRAY_LEN_EXPR,
  } kind;
  int num;
} array_len_t;

typedef enum {
  TY_NONE,
  TY_VOID,
  TY_CHAR,
  TY_INT,
  TY_FUNC,
  TY_PTR,
  TY_PARAM,
  TY_ARRAY,
  TY_ALIAS,
  TY_FIELDLIST,
  TY_STRUCT,
  TY_ENUM,
} type_kind_t;

typedef struct type_t_ {
  type_kind_t kind;
  int size;
  union {
    struct {
      struct type_t_ *type;
      struct type_t_ *next;
    } list;
    struct {
      struct type_t_ *ret;
      struct type_t_ *params;
    } func;
    struct {
      struct type_t_ *type;
      array_len_t len;
    } array;
    struct {
      token_t name;
      struct type_t_ *type;
      bool is_struct;
    } alias;
    struct {
      token_t name;
      struct type_t_ *fieldlist;
    } struct_;
    struct {
      struct type_t_ *type;
      token_t name;
      struct type_t_ *next;
    } fieldlist;
    struct {
      token_t name;
      struct type_t_ *next;
    } enum_;
    struct type_t_ *ptr;
  } as;
} type_t;

char *array_len_dump_to_string(array_len_t array_len) {
  char *str = alloc(20);
  switch (array_len.kind) {
    case ARRAY_LEN_NOTARRAY:
      break;
    case ARRAY_LEN_UNSET:
      snprintf(str, 20, "[]");
      break;
    case ARRAY_LEN_NUM:
      snprintf(str, 20, "[%d]", array_len.num);
      break;
    case ARRAY_LEN_EXPR:
      snprintf(str, 20, "[...]");
      break;
  }
  return str;
}

type_t *type_malloc(type_t type) {
  type_t *ptr = alloc(sizeof(type_t));
  assert(ptr);
  *ptr = type;
  return ptr;
}

char *type_dump_to_string(type_t *type) {
  assert(type);

  char *string = alloc(128);
  assert(string);
  memset(string, 0, 128);

  switch (type->kind) {
    case TY_NONE:
      strcpy(string, "NONE");
      break;
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
      assert(type->as.func.ret);
      char *str = type_dump_to_string(type->as.func.ret);
      if (type->as.func.params) {
        char *str2 = type_dump_to_string(type->as.func.params);
        snprintf(string, 128, "FUNC {%s %s}", str, str2);
        free_ptr(str2);
      } else {
        snprintf(string, 128, "FUNC {%s}", str);
      }
      free_ptr(str);
    } break;
    case TY_PTR:
      assert(type->as.ptr);
      if (type->as.ptr->kind == TY_ALIAS) {
        snprintf(string, 128, "PTR " SV_FMT, SV_UNPACK(type->as.ptr->as.alias.name.image));
      } else {
        char *str = type_dump_to_string(type->as.ptr);
        snprintf(string, 128, "PTR %s", str);
        free_ptr(str);
      }
      break;
    case TY_PARAM:
    {
      assert(type->as.list.type);
      char *str = type_dump_to_string(type->as.list.type);
      if (type->as.list.next) {
        char *str2 = type_dump_to_string(type->as.list.next);
        snprintf(string, 128, "PARAM {%s %s}", str, str2);
        free_ptr(str2);
      } else {
        snprintf(string, 128, "PARAM {%s}", str);
      }
      free_ptr(str);
    } break;
    case TY_ARRAY:
    {
      assert(type->as.array.type);
      char *str = type_dump_to_string(type->as.array.type);
      char *arr = array_len_dump_to_string(type->as.array.len);
      snprintf(string, 128, "%s%s", str, arr);
      free_ptr(str);
      free_ptr(arr);
    } break;
    case TY_ALIAS:
      snprintf(string,
               128,
               "ALIAS {" SV_FMT " %s%s}",
               SV_UNPACK(type->as.alias.name.image),
               type->as.alias.type ? "..." : "NULL",
               type->as.alias.is_struct ? " struct" : "");
      break;
    case TY_FIELDLIST:
    {
      assert(type->as.fieldlist.type);
      char *str = type_dump_to_string(type->as.fieldlist.type);
      if (type->as.fieldlist.next) {
        char *str2 = type_dump_to_string(type->as.fieldlist.next);
        snprintf(string,
                 128,
                 "FIELDLIST {%s " SV_FMT " %s}",
                 str,
                 SV_UNPACK(type->as.fieldlist.name.image),
                 str2);
        free_ptr(str2);
      } else {
        snprintf(string,
                 128,
                 "FIELDLIST {%s " SV_FMT "}",
                 str,
                 SV_UNPACK(type->as.fieldlist.name.image));
      }
      free_ptr(str);
    } break;
    case TY_STRUCT:
      if (type->as.struct_.fieldlist) {
        char *str = type_dump_to_string(type->as.struct_.fieldlist);
        if (type->as.struct_.name.kind == T_NONE) {
          snprintf(string, 128, "STRUCT {%s}", str);
        } else {
          snprintf(
              string, 128, "STRUCT {" SV_FMT " %s}", SV_UNPACK(type->as.struct_.name.image), str);
        }
        free_ptr(str);
      } else {
        if (type->as.struct_.name.kind == T_NONE) {
          snprintf(string, 128, "STRUCT {}");
        } else {
          snprintf(string, 128, "STRUCT {" SV_FMT "}", SV_UNPACK(type->as.struct_.name.image));
        }
      }
      break;
    case TY_ENUM:
      if (type->as.enum_.next) {
        char *str = type_dump_to_string(type->as.enum_.next);
        snprintf(string, 128, "ENUM {" SV_FMT " %s}", SV_UNPACK(type->as.enum_.name.image), str);
        free_ptr(str);
      } else {
        snprintf(string, 128, "ENUM {" SV_FMT "}", SV_UNPACK(type->as.enum_.name.image));
      }
      break;
  }

#if 0
  int len = strlen(string);
  snprintf(string + len, 128 - len, " <%d>", type->size);
#endif

  return string;
}

bool type_cmp(type_t *a, type_t *b) {
  if (a == NULL && b == NULL) {
    return true;
  }
  if ((a == NULL && b != NULL) || (a != NULL && b == NULL)) {
    return false;
  }

  if (a->kind == TY_ALIAS) {
    assert(a->as.alias.type);
    return type_cmp(a->as.alias.type, b);
  } else if (b->kind == TY_ALIAS) {
    assert(b->as.alias.type);
    return type_cmp(a, b->as.alias.type);
  }
  if (a->kind == b->kind) {
    switch (a->kind) {
      case TY_NONE:
      case TY_VOID:
      case TY_CHAR:
      case TY_INT:
        return true;
      case TY_FUNC:
        assert(a->as.func.ret);
        if ((a->as.func.params ? 1 : 0) ^ (b->as.func.params ? 1 : 0)) {
          return false;
        }
        return type_cmp(a->as.func.ret, b->as.func.ret)
               && (a->as.func.params ? type_cmp(a->as.func.params, b->as.func.params) : true);
      case TY_PTR:
        assert(a->as.ptr);
        assert(b->as.ptr);
        return type_cmp(a->as.ptr, b->as.ptr);
      case TY_PARAM:
        assert(a->as.list.type);
        assert(b->as.list.type);
        if ((a->as.list.next ? 1 : 0) ^ (b->as.list.next ? 1 : 0)) {
          return false;
        }
        return type_cmp(a->as.list.type, b->as.list.type)
               && (a->as.list.next ? type_cmp(a->as.list.next, b->as.list.next) : true);
      case TY_ARRAY:
        return type_cmp(a->as.array.type, b->as.array.type)
               && a->as.array.len.kind == b->as.array.len.kind;
      case TY_ALIAS:
        assert(0);
      case TY_STRUCT:
        return (a->as.struct_.name.kind != T_NONE && b->as.struct_.name.kind != T_NONE
                && sv_eq(a->as.struct_.name.image, b->as.struct_.name.image))
               || type_cmp(a->as.struct_.fieldlist, b->as.struct_.fieldlist);
      case TY_FIELDLIST:
        return type_cmp(a->as.fieldlist.type, b->as.fieldlist.type)
               && sv_eq(a->as.fieldlist.name.image, b->as.fieldlist.name.image)
               && type_cmp(a->as.fieldlist.next, b->as.fieldlist.next);
      case TY_ENUM:
        return sv_eq(a->as.enum_.name.image, b->as.enum_.name.image)
               && type_cmp(a->as.enum_.next, b->as.enum_.next);
    }
  }
  return false;
}

bool type_greaterthan(type_t *a, type_t *b) { // a >= b
  assert(a);
  assert(b);

  if (a->kind == TY_ALIAS) {
    assert(a->as.alias.type);
    return type_cmp(a->as.alias.type, b);
  } else if (b->kind == TY_ALIAS) {
    assert(b->as.alias.type);
    return type_cmp(a, b->as.alias.type);
  }

  if (a->kind == TY_INT && b->kind == TY_CHAR) {
    return true;
  } else if (a->kind == TY_ARRAY && b->kind == TY_ARRAY
             && type_cmp(a->as.array.type, b->as.array.type)) {
    assert(b->as.array.len.kind == ARRAY_LEN_NUM);
    if (a->as.array.len.kind == ARRAY_LEN_NUM) {
      return a->as.array.len.num >= b->as.array.len.num;
    } else {
      return a->as.array.len.kind == ARRAY_LEN_UNSET;
    }
  } else if (a->kind == TY_PTR && b->kind == TY_ARRAY) {
    return type_cmp(a->as.ptr, b->as.array.type);
  } else if (a->kind == TY_INT && b->kind == TY_ENUM) {
    return true;
  }
  return type_cmp(a, b);
}

int type_size_aligned(type_t *type) {
  return type->size + type->size % 2;
}

void type_expect(location_t loc, type_t *found, type_t *expect) {
  if (type_cmp(found, expect)) {
    return;
  }
  char *expectstr = type_dump_to_string(expect);
  char *foundstr = type_dump_to_string(found);
  eprintf(loc, "expected '%s', found '%s'", expectstr, foundstr);
}

bool type_is_kind(type_t *type, type_kind_t kind) {
  assert(type);
  assert(kind != TY_ALIAS);

  if (type->kind == TY_ALIAS) {
    assert(type->as.alias.type);
    return type_is_kind(type->as.alias.type, kind);
  }

  return type->kind == kind;
}

type_t *type_pass_alias(type_t *type) {
  assert(type);
  if (type->kind == TY_ALIAS) {
    assert(type->as.alias.type);
    return type_pass_alias(type->as.alias.type);
  }
  return type;
}

typedef enum {
  A_NONE,
  A_LIST,      // binary
  A_FUNCDECL,  // funcdecl
  A_FUNCDEF,   // funcdef
  A_PARAMDEF,  // paramdef
  A_BLOCK,     // ast
  A_STATEMENT, // ast
  A_RETURN,    // ast
  A_BINARYOP,  // binaryop
  A_UNARYOP,   // unaryop
  A_INT,       // fac
  A_STRING,    // fac
  A_SYM,       // fac
  A_DECL,      // decl
  A_GLOBDECL,  // decl
  A_ASSIGN,    // binary
  A_FUNCALL,   // funcall
  A_PARAM,     // binary
  A_ARRAY,     // binary
  A_TYPEDEF,   // typedef_
  A_CAST,      // cast
  A_ASM,       // fac
  A_IF,        // if_
  A_WHILE,     // while_
  A_EXTERN,    // ast
  A_BREAK,     //
} ast_kind_t;

typedef struct ast_t_ {
  ast_kind_t kind;
  location_t loc;
  type_t type;
  union {
    struct {
      struct ast_t_ *left;
      struct ast_t_ *right;
    } binary;
    struct {
      type_t type;
      token_t name;
      struct ast_t_ *params;
      struct ast_t_ *block;
    } funcdecl;
    struct {
      type_t type;
      token_t name;
      struct ast_t_ *params;
    } funcdef;
    struct {
      type_t type;
      token_t name;
      struct ast_t_ *next;
    } paramdef;
    struct {
      token_kind_t op;
      struct ast_t_ *lhs;
      struct ast_t_ *rhs;
    } binaryop;
    struct {
      token_kind_t op;
      struct ast_t_ *arg;
    } unaryop;
    struct {
      type_t type;
      token_t name;
      struct ast_t_ *expr;
      struct ast_t_ *array_len;
    } decl;
    struct {
      token_t name;
      struct ast_t_ *params;
    } funcall;
    struct {
      type_t type;
      token_t name;
    } typedef_;
    struct {
      type_t target;
      struct ast_t_ *ast;
    } cast;
    struct {
      struct ast_t_ *cond;
      struct ast_t_ *then;
      struct ast_t_ *else_;
    } if_;
    token_t fac;
    struct ast_t_ *ast;
  } as;
} ast_t;

ast_t *ast_malloc(ast_t ast) {
  ast_t *ptr = alloc(sizeof(ast_t));
  assert(ptr);
  *ptr = ast;
  return ptr;
}

char *ast_kind_to_string(ast_kind_t kind) {
  // clang-format off
  switch (kind) {
    case A_NONE: return "NONE";
    case A_LIST: return "LIST";
    case A_FUNCDECL: return "FUNCDECL";
    case A_FUNCDEF: return "FUNCDEF";
    case A_PARAMDEF: return "PARAMDEF";
    case A_BLOCK: return "BLOCK";
    case A_STATEMENT: return "STATEMENT";
    case A_RETURN: return "RETURN";
    case A_BINARYOP: return "BINARYOP";
    case A_UNARYOP: return "UNARYOP";
    case A_INT: return "INT";
    case A_STRING: return "STRING";
    case A_SYM: return "SYM";
    case A_DECL: return "DECL";
    case A_GLOBDECL: return "GLOBDECL";
    case A_ASSIGN: return "ASSIGN";
    case A_FUNCALL: return "FUNCALL";
    case A_PARAM: return "PARAM";
    case A_ARRAY: return "ARRAY";
    case A_TYPEDEF: return "TYPEDEF";
    case A_CAST: return "CAST";
    case A_ASM: return "ASM";
    case A_IF: return "IF";
    case A_WHILE: return "WHILE";
    case A_EXTERN: return "EXTERN";
    case A_BREAK: return "BREAK";
  }
  // clang-format on
  assert(0);
}

void ast_dump(ast_t *ast, bool dumptype) {
  if (!ast) {
    printf("NULL");
    return;
  }

  printf("%s(", ast_kind_to_string(ast->kind));
  switch (ast->kind) {
    case A_NONE:
      assert(0);
    case A_BREAK:
      break;
    case A_LIST:
    case A_ASSIGN:
    case A_PARAM:
    case A_ARRAY:
    case A_WHILE:
      ast_dump(ast->as.binary.left, dumptype);
      printf(", ");
      ast_dump(ast->as.binary.right, dumptype);
      break;
    case A_FUNCDECL:
    {
      char *str = type_dump_to_string(&ast->as.funcdecl.type);
      printf("%s, " SV_FMT ", ", str, SV_UNPACK(ast->as.funcdecl.name.image));
      free_ptr(str);
      ast_dump(ast->as.funcdecl.params, dumptype);
      printf(", ");
      ast_dump(ast->as.funcdecl.block, dumptype);
    } break;
    case A_FUNCDEF:
    {
      char *str = type_dump_to_string(&ast->as.funcdef.type);
      printf("%s, " SV_FMT ", ", str, SV_UNPACK(ast->as.funcdef.name.image));
      free_ptr(str);
      ast_dump(ast->as.funcdef.params, dumptype);
    } break;
    case A_PARAMDEF:
    {
      char *str = type_dump_to_string(&ast->as.paramdef.type);
      printf("%s, " SV_FMT ", ", str, SV_UNPACK(ast->as.paramdef.name.image));
      free_ptr(str);
      ast_dump(ast->as.paramdef.next, dumptype);
    } break;
    case A_BLOCK:
    case A_STATEMENT:
    case A_RETURN:
    case A_EXTERN:
      ast_dump(ast->as.ast, dumptype);
      break;
    case A_BINARYOP:
      printf("%s, ", token_kind_to_string(ast->as.binaryop.op));
      ast_dump(ast->as.binaryop.lhs, dumptype);
      printf(", ");
      ast_dump(ast->as.binaryop.rhs, dumptype);
      break;
    case A_UNARYOP:
      printf("%s, ", token_kind_to_string(ast->as.unaryop.op));
      ast_dump(ast->as.unaryop.arg, dumptype);
      break;
    case A_INT:
    case A_STRING:
    case A_SYM:
    case A_ASM:
      printf(SV_FMT, SV_UNPACK(ast->as.fac.image));
      break;
    case A_DECL:
    case A_GLOBDECL:
    {
      char *str = type_dump_to_string(&ast->as.decl.type);
      printf("%s, " SV_FMT ", ", str, SV_UNPACK(ast->as.decl.name.image));
      free_ptr(str);
      ast_dump(ast->as.decl.expr, dumptype);
      printf(", ");
      ast_dump(ast->as.decl.array_len, dumptype);
    } break;
    case A_FUNCALL:
      printf(SV_FMT ", ", SV_UNPACK(ast->as.funcall.name.image));
      ast_dump(ast->as.funcall.params, dumptype);
      break;
    case A_TYPEDEF:
    {
      char *str = type_dump_to_string(&ast->as.typedef_.type);
      printf("%s, " SV_FMT ", ", str, SV_UNPACK(ast->as.typedef_.name.image));
      free_ptr(str);
    } break;
    case A_CAST:
    {
      char *str = type_dump_to_string(&ast->as.cast.target);
      printf("%s, ", str);
      free_ptr(str);
      ast_dump(ast->as.cast.ast, dumptype);
    } break;
    case A_IF:
      ast_dump(ast->as.if_.cond, dumptype);
      printf(", ");
      ast_dump(ast->as.if_.then, dumptype);
      printf(", ");
      ast_dump(ast->as.if_.else_, dumptype);
      break;
  }
  printf(")");

  if (dumptype && ast->type.kind != TY_VOID) {
    char *str = type_dump_to_string(&ast->type);
    printf(" {%s <%d>}", str, ast->type.size);
    free_ptr(str);
  }
}

void ast_dump_tree(ast_t *ast, bool dumptype, int indent) {
  if (indent > 0) {
    assert(indent < 400 / 4);
    printf("%*.*s",
           indent * 4,
           indent * 4,
           "                                                                   "
           "                                                                   "
           "                                                                   "
           "                                                                   "
           "                                                                   "
           "                                                                   "
           "                                                                   "
           "                                                                   "
           "                                                                   ");
  }

  if (!ast) {
    printf("NULL\n");
    return;
  }

  printf("%s", ast_kind_to_string(ast->kind));

#define dump_type                                   \
  if (dumptype && ast->type.kind != TY_VOID) {      \
    char *str = type_dump_to_string(&ast->type);    \
    printf(" :: {%s <%d>}\n", str, ast->type.size); \
    free_ptr(str);                                  \
  } else {                                          \
    printf("\n");                                   \
  }

  switch (ast->kind) {
    case A_NONE:
    case A_BREAK:
      dump_type;
      break;
    case A_LIST:
    case A_PARAM:
    case A_ARRAY:
    {
      dump_type;
      ast_dump_tree(ast->as.binary.left, dumptype, indent + 1);
      ast_t *next = ast->as.binary.right;
      while (next) {
        assert(next->kind == ast->kind);
        ast_dump_tree(next->as.binary.left, dumptype, indent + 1);
        next = next->as.binary.right;
      }
    } break;
    case A_FUNCDECL:
    {
      char *str = type_dump_to_string(&ast->as.funcdecl.type);
      printf(" {%s} " SV_FMT, str, SV_UNPACK(ast->as.funcdecl.name.image));
      free_ptr(str);
      dump_type;
      ast_dump_tree(ast->as.funcdecl.params, dumptype, indent + 1);
      ast_dump_tree(ast->as.funcdecl.block, dumptype, indent + 1);
    } break;
    case A_FUNCDEF:
    {
      char *str = type_dump_to_string(&ast->as.funcdef.type);
      printf(" {%s} " SV_FMT, str, SV_UNPACK(ast->as.funcdef.name.image));
      free_ptr(str);
      dump_type;
      ast_dump_tree(ast->as.funcdef.params, dumptype, indent + 1);
    } break;
    case A_PARAMDEF:
    {
      char *str = type_dump_to_string(&ast->as.paramdef.type);
      printf(" {%s} " SV_FMT, str, SV_UNPACK(ast->as.paramdef.name.image));
      free_ptr(str);
      dump_type;
      ast_dump_tree(ast->as.paramdef.next, dumptype, indent);
    } break;
    case A_STATEMENT:
    case A_BLOCK:
    case A_RETURN:
    case A_EXTERN:
      dump_type;
      ast_dump_tree(ast->as.ast, dumptype, indent + 1);
      break;
    case A_BINARYOP:
      printf(" %s", token_kind_to_string(ast->as.binaryop.op));
      dump_type;
      ast_dump_tree(ast->as.binaryop.lhs, dumptype, indent + 1);
      ast_dump_tree(ast->as.binaryop.rhs, dumptype, indent + 1);
      break;
    case A_UNARYOP:
      printf(" %s", token_kind_to_string(ast->as.unaryop.op));
      dump_type;
      ast_dump_tree(ast->as.unaryop.arg, dumptype, indent + 1);
      break;
    case A_INT:
      printf(" %d", ast->as.fac.asint);
      dump_type;
      break;
    case A_STRING:
    case A_SYM:
    case A_ASM:
      printf(" " SV_FMT, SV_UNPACK(ast->as.fac.image));
      dump_type;
      break;
    case A_GLOBDECL:
    case A_DECL:
    {
      char *str = type_dump_to_string(&ast->as.decl.type);
      printf(" {%s} " SV_FMT, str, SV_UNPACK(ast->as.decl.name.image));
      free_ptr(str);
      dump_type;
      ast_dump_tree(ast->as.decl.expr, dumptype, indent + 1);
    } break;
    case A_ASSIGN:
    case A_WHILE:
      dump_type;
      ast_dump_tree(ast->as.binary.left, dumptype, indent + 1);
      ast_dump_tree(ast->as.binary.right, dumptype, indent + 1);
      break;
    case A_FUNCALL:
      printf(" " SV_FMT, SV_UNPACK(ast->as.funcall.name.image));
      dump_type;
      ast_dump_tree(ast->as.funcall.params, dumptype, indent + 1);
      break;
    case A_TYPEDEF:
    {
      char *str = type_dump_to_string(&ast->as.typedef_.type);
      printf(" {%s} " SV_FMT, str, SV_UNPACK(ast->as.typedef_.name.image));
      free_ptr(str);
      dump_type;
    } break;
    case A_CAST:
    {
      char *str = type_dump_to_string(&ast->as.cast.target);
      printf(" {%s}\n", str);
      free_ptr(str);
      ast_dump_tree(ast->as.cast.ast, dumptype, indent + 1);
    } break;
    case A_IF:
      dump_type;
      ast_dump_tree(ast->as.if_.cond, dumptype, indent + 1);
      ast_dump_tree(ast->as.if_.then, dumptype, indent + 1);
      ast_dump_tree(ast->as.if_.else_, dumptype, indent + 1);
      break;
  }

#undef dump_type
}

#define SYMBOL_MAX       256
#define SCOPE_MAX        32
#define DATA_MAX         256
#define CODE_MAX         512
#define IR_MAX           256
#define BREAK_TARGET_MAX 8

typedef struct {
  token_t name;
  type_t *type;
  enum {
    INFO_NONE,
    INFO_LOCAL,
    INFO_GLOBAL,
    INFO_TYPE,
    INFO_TYPEINCOMPLETE,
    INFO_CONSTANT,
  } kind;
  union {
    int local;
    int global;
    int num;
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

typedef enum {
  IR_NONE,
  IR_SETLABEL,    // + sv
  IR_SETULI,      // + num
  IR_JMPZ,        // + num
  IR_JMPNZ,       // + num
  IR_JMP,         // + num
  IR_FUNCEND,     //
  IR_ADDR_LOCAL,  // + num
  IR_ADDR_GLOBAL, // + loc
  IR_READ,        // + num
  IR_WRITE,       // + num
  IR_CHANGE_SP,   // + num
  IR_INT,         // + num
  IR_STRING,      // + num
  IR_OPERATION,   // + inst
  IR_MUL,         // + num
  IR_DIV,         // + num
  IR_CALL,        // + sv
  IR_EXTERN,      // + sv
} ir_kind_t;

typedef struct {
  ir_kind_t kind;
  union {
    struct {
      uint16_t base;
      uint16_t offset;
    } loc;
    sv_t sv;
    int num;
    instruction_t inst;
  } arg;
} ir_t;

char *ir_kind_to_string(ir_kind_t kind) {
  switch (kind) {
    case IR_NONE:
      return "NONE";
    case IR_SETLABEL:
      return "SETLABEL";
    case IR_SETULI:
      return "SETULI";
    case IR_JMPZ:
      return "JMPZ";
    case IR_JMPNZ:
      return "JMPNZ";
    case IR_JMP:
      return "JMP";
    case IR_FUNCEND:
      return "FUNCEND";
    case IR_ADDR_LOCAL:
      return "ADDR_LOCAL";
    case IR_ADDR_GLOBAL:
      return "ADDR_GLOBAL";
    case IR_READ:
      return "READ";
    case IR_WRITE:
      return "WRITE";
    case IR_CHANGE_SP:
      return "CHANGE_SP";
    case IR_INT:
      return "INT";
    case IR_STRING:
      return "STRING";
    case IR_OPERATION:
      return "OPERATION";
    case IR_MUL:
      return "MUL";
    case IR_DIV:
      return "DIV";
    case IR_CALL:
      return "CALL";
    case IR_EXTERN:
      return "EXTERN";
  }
  assert(0);
}

void ir_dump(ir_t ir) {
  printf("%s", ir_kind_to_string(ir.kind));
  switch (ir.kind) {
    case IR_NONE:
    case IR_FUNCEND:
      break;
    case IR_SETLABEL:
    case IR_CALL:
    case IR_EXTERN:
      printf(" " SV_FMT, SV_UNPACK(ir.arg.sv));
      break;
    case IR_SETULI:
    case IR_JMPZ:
    case IR_JMPNZ:
    case IR_JMP:
    case IR_READ:
    case IR_WRITE:
    case IR_CHANGE_SP:
    case IR_INT:
    case IR_STRING:
    case IR_MUL:
    case IR_DIV:
    case IR_ADDR_LOCAL:
      printf(" %d", ir.arg.num);
      break;
    case IR_OPERATION:
      printf(" %s", instruction_to_string(ir.arg.inst));
      break;
    case IR_ADDR_GLOBAL:
      printf(" {%d+%d}", ir.arg.loc.base, ir.arg.loc.offset);
      break;
  }
  printf("\n");
}

typedef struct {
  int uli; // uli to jump to if break
  int sp;  // sp before the loop, to correctly out of scope the variables
} break_target_info_t;

typedef struct {
  scope_t scopes[SCOPE_MAX];
  int scope_num;
  scope_t *scope;
  int sp;
  int param;
  int uli; // unique label id
  compiled_t compiled;
  ir_t irs[IR_MAX];
  int ir_num;
  ir_t irs_init[IR_MAX];
  int ir_init_num;
  bool is_init;
  break_target_info_t break_target[BREAK_TARGET_MAX];
  int break_target_num;
} state_t;

void state_init(state_t *state) {
  assert(state);
  *state = (state_t){0};
  ++state->scope_num;
  state->scope = &state->scopes[0];
}

void state_add_ir(state_t *state, ir_t ir) {
  assert(state);
  if (state->is_init) {
    // if (ir.kind == IR_CHANGE_SP && state->irs_init[state->ir_init_num].kind == IR_CHANGE_SP) {
    //   state->irs_init[state->ir_init_num].arg.num += ir.arg.num;
    //   state->sp += ir.arg.num;
    //   return;
    // } else if (ir.kind == IR_CHANGE_SP && ir.arg.num == 0) {
    //   return;
    // }
    assert(state->ir_init_num + 1 < IR_MAX);
    state->irs_init[state->ir_init_num++] = ir;
  } else {
    // if (ir.kind == IR_CHANGE_SP && state->irs[state->ir_num].kind == IR_CHANGE_SP) {
    //   state->irs[state->ir_num].arg.num += ir.arg.num;
    //   state->sp += ir.arg.num;
    //   return;
    // } else if (ir.kind == IR_CHANGE_SP && ir.arg.num == 0) {
    //   return;
    // }
    assert(state->ir_num + 1 < IR_MAX);
    state->irs[state->ir_num++] = ir;
  }
  switch (ir.kind) {
    case IR_NONE:
      assert(0);
      break;
    case IR_SETLABEL:
    case IR_SETULI:
    case IR_JMP:
    case IR_FUNCEND:
    case IR_MUL:
    case IR_DIV:
    case IR_CALL:
    case IR_EXTERN:
      break;
    case IR_JMPZ:
    case IR_JMPNZ:
      state->sp -= 2;
      break;
    case IR_ADDR_LOCAL:
    case IR_ADDR_GLOBAL:
      state->sp += 2;
      break;
    case IR_READ:
      state->sp -= 2;
      state->sp += ir.arg.num + ir.arg.num % 2;
      break;
    case IR_WRITE:
      state->sp -= 2;
      state->sp -= ir.arg.num + ir.arg.num % 2;
      break;
    case IR_CHANGE_SP:
      state->sp += ir.arg.num;
      break;
    case IR_INT:
    case IR_STRING:
      state->sp += 2;
      break;
    case IR_OPERATION:
      switch (ir.arg.inst) {
        case SUM:
        case SUB:
        case B_AH:
          state->sp -= 2;
          break;
        case SHL:
        case SHR:
          break;
        default:
          TODO;
      }
      break;
  }
  assert(state->sp % 2 == 0);
}

void state_add_addr_offset(state_t *state, int offset) {
  assert(state);
  if (state->is_init) {
    assert(state->ir_init_num > 0);
    if (state->irs_init[state->ir_init_num - 1].kind == IR_ADDR_LOCAL) {
      state->irs_init[state->ir_init_num - 1].arg.num += offset;
    } else if (state->irs_init[state->ir_init_num - 1].kind == IR_ADDR_GLOBAL) {
      state->irs_init[state->ir_init_num - 1].arg.loc.offset += offset;
    } else {
      assert(0 && "expected ADDR");
    }
  } else {
    assert(state->ir_num > 0);
    if (state->irs[state->ir_num - 1].kind == IR_ADDR_LOCAL) {
      state->irs[state->ir_num - 1].arg.num += offset;
    } else if (state->irs[state->ir_num - 1].kind == IR_ADDR_GLOBAL) {
      state->irs[state->ir_num - 1].arg.loc.offset += offset;
    } else {
      assert(0 && "expected ADDR");
    }
  }
}

void state_push_break_target(state_t *state, int target) {
  assert(state);
  assert(state->break_target_num + 1 < BREAK_TARGET_MAX);
  state->break_target[state->break_target_num++] = (break_target_info_t){target, state->sp};
}

void state_drop_break_target(state_t *state) {
  assert(state);
  assert(state->break_target_num > 0);
  state->break_target_num--;
}

void data(compiled_t *, bytecode_t);
void code(compiled_t *, bytecode_t);
void state_init_with_compiled(state_t *state) {
  state_init(state);
  compiled_t *compiled = &state->compiled;
  data(compiled, bytecode_with_string(BEXTERN, 0, "exit"));
  data(compiled, bytecode_with_string(BGLOBAL, 0, "_start"));
  compiled->is_init = true;
  code(compiled, bytecode_with_string(BSETLABEL, 0, "_start"));
  compiled->is_init = false;
}

void state_push_scope(state_t *state) {
  assert(state);
  assert(state->scope_num + 1 < SCOPE_MAX);
  memset(&state->scopes[state->scope_num], 0, sizeof(scope_t));
  ++state->scope_num;
  state->scope = &state->scopes[state->scope_num - 1];
}

void state_drop_scope(state_t *state) {
  assert(state);
  assert(state->scope_num > 0);
  --state->scope_num;
  state->scope = &state->scopes[state->scope_num - 1];
}

symbol_t *state_find_symbol(state_t *state, token_t name) {
  assert(state);
  assert(state->scope_num > 0);
  for (int j = state->scope_num - 1; j >= 0; --j) {
    scope_t *scope = &state->scopes[j];
    for (int i = 0; i < scope->symbol_num; ++i) {
      if (sv_eq(scope->symbols[i].name.image, name.image)) {
        return &scope->symbols[i];
      }
    }
  }
  eprintf(name.loc, "symbol not declared: " SV_FMT, SV_UNPACK(name.image));
  assert(0);
}

void state_add_symbol(state_t *state, symbol_t symbol) {
  assert(state);

  for (int i = 0; i < state->scope->symbol_num; ++i) {
    if (sv_eq(state->scope->symbols[i].name.image, symbol.name.image)) {
      eprintf(symbol.name.loc,
              "redefinition of symbol '" SV_FMT "', defined at " LOCATION_FMT,
              SV_UNPACK(symbol.name.image),
              LOCATION_UNPACK(state->scope->symbols[i].name.loc));
    }
  }

  scope_t *scope = &state->scopes[state->scope_num - 1];
  assert(scope->symbol_num + 1 < SYMBOL_MAX);
  scope->symbols[scope->symbol_num++] = symbol;
}

void state_solve_type_alias(state_t *state, type_t *type) {
  assert(state);
  assert(type);

  switch (type->kind) {
    case TY_NONE:
      assert(0);
    case TY_VOID:
    case TY_CHAR:
    case TY_INT:
      break;
    case TY_FUNC:
      assert(0);
    case TY_PTR:
      assert(type->as.ptr);
      state_solve_type_alias(state, type->as.ptr);
      break;
    case TY_PARAM:
      assert(type->as.list.type);
      state_solve_type_alias(state, type->as.list.type);
      if (type->as.list.next) {
        state_solve_type_alias(state, type->as.list.next);
      }
      break;
    case TY_ARRAY:
      assert(type->as.array.type);
      state_solve_type_alias(state, type->as.array.type);
      if (type->as.array.len.kind == ARRAY_LEN_NUM) {
        type->size = type->as.array.type->size * type->as.array.len.num;
      }
      break;
    case TY_ALIAS:
    {
      symbol_t *s = state_find_symbol(state, type->as.alias.name);
      if (s->kind == INFO_TYPEINCOMPLETE && !type->as.alias.is_struct) {
        eprintf(type->as.alias.name.loc,
                "must use 'struct " SV_FMT "'",
                SV_UNPACK(type->as.alias.name.image));
      }
      if (s->kind != INFO_TYPE && s->kind != INFO_TYPEINCOMPLETE) {
        eprintf(type->as.alias.name.loc, "expected to be a type");
      }
      assert(s->type);
      type->as.alias.type = s->type;
      type->size = type->as.alias.type->size;
    } break;
    case TY_STRUCT:
      if (type->as.struct_.fieldlist) {
        state_solve_type_alias(state, type->as.struct_.fieldlist);
        type->size = type->as.struct_.fieldlist->size;
      }
      break;
    case TY_FIELDLIST:
      assert(type->as.fieldlist.type);
      state_solve_type_alias(state, type->as.fieldlist.type);
      if (type->as.fieldlist.type->size == 0) {
        eprintf(type->as.fieldlist.name.loc, "field has incomplete type");
      }
      if (type->as.fieldlist.next) {
        state_solve_type_alias(state, type->as.fieldlist.next);
      }
      type->size = type->as.fieldlist.type->size + (type->as.fieldlist.next ? type->as.fieldlist.next->size : 0);
      break;
    case TY_ENUM:
      break;
  }
}

void dump_code(compiled_t *compiled) {
  assert(compiled);
  for (int i = 0; i < compiled->data_num; ++i) {
    bytecode_dump(compiled->data[i]);
  }
  for (int i = 0; i < compiled->init_num; ++i) {
    bytecode_dump(compiled->init[i]);
  }
  for (int i = 0; i < compiled->code_num; ++i) {
    bytecode_dump(compiled->code[i]);
  }
}

void bytecode_to_file(FILE *stream, bytecode_t bc) {
  assert(stream);
  switch (bc.kind) {
    case BNONE:
      assert(0);
      break;
    case BINST:
      fprintf(stream, "%s", instruction_to_string(bc.inst));
      break;
    case BINSTHEX:
      fprintf(stream, "%s 0x%02X", instruction_to_string(bc.inst), bc.arg.num);
      break;
    case BINSTHEX2:
      fprintf(stream, "%s 0x%04X", instruction_to_string(bc.inst), bc.arg.num);
      break;
    case BINSTLABEL:
      fprintf(stream, "%s %s", instruction_to_string(bc.inst), bc.arg.string);
      break;
    case BINSTRELLABEL:
      fprintf(stream, "%s $%s", instruction_to_string(bc.inst), bc.arg.string);
      break;
    case BHEX:
      fprintf(stream, "0x%02X", bc.arg.num);
      break;
    case BHEX2:
      fprintf(stream, "0x%04X", bc.arg.num);
      break;
    case BSTRING:
      fprintf(stream, "\"%s\"", bc.arg.string);
      break;
    case BSETLABEL:
      fprintf(stream, "%s:", bc.arg.string);
      break;
    case BGLOBAL:
      fprintf(stream, "GLOBAL %s", bc.arg.string);
      break;
    case BEXTERN:
      fprintf(stream, "EXTERN %s", bc.arg.string);
      break;
    case BALIGN:
      fprintf(stream, "ALIGN");
      break;
    case BDB:
      fprintf(stream, "db %d", bc.arg.num);
      break;
  }
}

type_t parse_type(tokenizer_t *tokenizer) {
  assert(tokenizer);

  bool is_struct = token_next_if_kind(tokenizer, T_STRUCT);

  type_t type = {0};

  token_t token = token_peek(tokenizer);
  if (token_next_if_kind(tokenizer, T_VOIDKW)) {
    type = (type_t){TY_VOID, 0, {}};
  } else if (token_next_if_kind(tokenizer, T_INTKW)) {
    type = (type_t){TY_INT, 2, {}};
  } else if (token_next_if_kind(tokenizer, T_CHARKW)) {
    type = (type_t){TY_CHAR, 1, {}};
  } else {
    token = token_expect(tokenizer, T_SYM);
    type = (type_t){TY_ALIAS, 0, {.alias = {token, NULL, is_struct}}};
  }

  if (is_struct && type.kind != TY_ALIAS) {
    eprintf(token.loc, "expected the name of an incomplete struct");
  }

  if (token_next_if_kind(tokenizer, T_STAR)) {
    type = (type_t){TY_PTR, 2, {.ptr = type_malloc(type)}};
  }

  return type;
}

void add_field(ast_t *ast, type_t *type, type_t **typei) {
  assert(ast);
  assert(ast->kind == A_DECL);
  assert(type);
  assert(type->kind == TY_FIELDLIST);
  assert(typei);
  assert(*typei);
  assert((*typei)->kind == TY_FIELDLIST);
  assert((*typei)->as.fieldlist.next == NULL);

  type_t ftype = ast->as.decl.type;
  token_t fname = ast->as.decl.name;

  for (type_t *t = type; t; t = t->as.fieldlist.next) {
    assert(t->kind == TY_FIELDLIST);
    if (sv_eq(t->as.fieldlist.name.image, fname.image)) {
      eprintf(fname.loc, "redefinition of field");
    }
  }

  if (!(*typei)->as.fieldlist.name.kind) {
    (*typei)->as.fieldlist.type = type_malloc(ftype);
    (*typei)->as.fieldlist.name = fname;
  } else {
    (*typei)->as.fieldlist.next = type_malloc((type_t){TY_FIELDLIST, 0, {.fieldlist = {type_malloc(ftype), fname, NULL}}});
    *typei = (*typei)->as.fieldlist.next;
  }
}

ast_t *parse_decl(tokenizer_t *tokenizer);
type_t parse_structdef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = token_expect(tokenizer, T_STRUCT).loc;
  token_t name = {0};
  if (token_peek(tokenizer).kind == T_SYM) {
    name = token_next(tokenizer);
  }
  token_expect(tokenizer, T_BRO);
  if (token_next_if_kind(tokenizer, T_BRC)) {
    catch = false;
    eprintf(location_union(start, tokenizer->last_token.loc), "invalid empty struct");
  }

  type_t type = {TY_FIELDLIST, 0, {}};
  type_t *typei = &type;

  do {
    ast_t *ast = parse_decl(tokenizer);
    token_expect(tokenizer, T_SEMICOLON);
    if (ast->kind == A_DECL) {
      if (ast->as.decl.expr) {
        eprintf(ast->as.decl.expr->loc, "expected SEMICOLON");
      }
      add_field(ast, &type, &typei);
    } else if (ast->kind == A_LIST) {
      for (ast_t *l = ast; l; l = l->as.binary.right) {
        assert(l->kind == A_LIST);
        assert(l->as.binary.left);
        assert(l->as.binary.left->kind == A_DECL);
        if (l->as.binary.left->as.decl.expr) {
          eprintf(ast->as.decl.expr->loc, "expected SEMICOLON");
        }
        add_field(l->as.binary.left, &type, &typei);
      }
    }
  } while (!token_next_if_kind(tokenizer, T_BRC));

  return (type_t){TY_STRUCT, 0, {.struct_ = {name, type_malloc(type)}}};
}

type_t parse_enumdef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_expect(tokenizer, T_ENUM);
  token_expect(tokenizer, T_BRO);

  token_t name = token_expect(tokenizer, T_SYM);
  token_expect(tokenizer, T_COMMA);

  type_t type = {TY_ENUM, 2, {.enum_ = {name, NULL}}};
  type_t *typei = &type;

  token_t names[128] = {name};
  int i = 1;

  while (!token_next_if_kind(tokenizer, T_BRC)) {
    token_t name = token_expect(tokenizer, T_SYM);
    token_expect(tokenizer, T_COMMA);

    for (int j = 0; j < i; ++j) {
      if (sv_eq(name.image, names[j].image)) {
        eprintf(name.loc, "redefinition of enumerator");
      }
    }

    names[i++] = name;

    typei->as.enum_.next = type_malloc((type_t){TY_ENUM, 2, {.enum_ = {name, NULL}}});
    typei = typei->as.enum_.next;
  }

  return type;
}

ast_t *parse_expr(tokenizer_t *tokenizer);
ast_t *parse_param(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_expect(tokenizer, T_PARO);
  if (token_next_if_kind(tokenizer, T_PARC)) {
    return NULL;
  }

  ast_t *expr = parse_expr(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_PARAM, expr->loc, {0}, {.binary = {expr, NULL}}});
  ast_t *asti = ast;

  while (token_next_if_kind(tokenizer, T_COMMA)) {
    expr = parse_expr(tokenizer);
    asti->as.binary.right = ast_malloc((ast_t){A_PARAM, expr->loc, {}, {.binary = {expr, NULL}}});
    asti = asti->as.binary.right;
  }

  token_expect(tokenizer, T_PARC);

  return ast;
}

ast_t *parse_funcall(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *param = parse_param(tokenizer);

  return ast_malloc((ast_t){A_FUNCALL, param ? location_union(name.loc, param->loc) : name.loc, {}, {.funcall = {name, param}}});
}

ast_t *parse_array(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t bro = token_expect(tokenizer, T_BRO);
  if (token_next_if_kind(tokenizer, T_BRC)) {
    catch = false;
    eprintf(location_union(bro.loc, tokenizer->last_token.loc), "invalid empty array");
  }

  ast_t *expr = parse_expr(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_ARRAY, expr->loc, {}, {.binary = {expr, NULL}}});
  ast_t *asti = ast;
  while (token_next_if_kind(tokenizer, T_COMMA)) {
    expr = parse_expr(tokenizer);
    asti->as.binary.right = ast_malloc((ast_t){A_ARRAY, expr->loc, {}, {.binary = {expr, NULL}}});
    asti = asti->as.binary.right;
  }
  token_t brc = token_expect(tokenizer, T_BRC);
  ast->loc = location_union(bro.loc, brc.loc);
  return ast;
}

ast_t *parse_fac(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);

  tokenizer_t savetok = *tokenizer;

  if (token_next_if_kind(tokenizer, T_PARO)) {
    savetok = *tokenizer;
    catch = true;
    if (setjmp(catch_buf) == 0) {
      type_t type = parse_type(tokenizer);
      token_expect(tokenizer, T_PARC);
      catch = false;
      ast_t *fac = parse_fac(tokenizer);
      return ast_malloc((ast_t){A_CAST, location_union(token.loc, fac->loc), {}, {.cast = {type, fac}}});
    }
    *tokenizer = savetok;

    ast_t *expr = parse_expr(tokenizer);
    token_expect(tokenizer, T_PARC);
    return expr;
  }

  if (token.kind == T_BRO) {
    return parse_array(tokenizer);
  }

  savetok = *tokenizer;
  if (token.kind == T_SYM) {
    token_next(tokenizer);
    if (token_peek(tokenizer).kind == T_PARO) {
      *tokenizer = savetok;
      return parse_funcall(tokenizer);
    }
    *tokenizer = savetok;
  }

  token_next(tokenizer);
  if (token.kind == T_INT || token.kind == T_HEX || token.kind == T_CHAR) {
    return ast_malloc((ast_t){A_INT, token.loc, {}, {.fac = token}});
  } else if (token.kind == T_STRING) {
    return ast_malloc((ast_t){A_STRING, token.loc, {}, {.fac = token}});
  } else if (token.kind == T_SYM) {
    return ast_malloc((ast_t){A_SYM, token.loc, {}, {.fac = token}});
  } else {
    eprintf(token.loc, "invalid token for fac: '%s'", token_kind_to_string(token.kind));
  }
  assert(0);
}

ast_t *parse_access(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *fac = parse_fac(tokenizer);

  if (token_peek(tokenizer).kind == T_DOT) {
    token_t dot = token_next(tokenizer);
    token_t name = token_expect(tokenizer, T_SYM);

    return ast_malloc((ast_t){A_BINARYOP, location_union(fac->loc, name.loc), {}, {.binaryop = {dot.kind, fac, ast_malloc((ast_t){A_SYM, name.loc, {}, {.fac = name}})}}});
  }

  return fac;
}

ast_t *parse_unary(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);
  switch (token.kind) {
    case T_PLUS:
    case T_MINUS:
    case T_AND:
    case T_STAR:
      token_next(tokenizer);
      ast_t *arg = parse_unary(tokenizer);

      if (arg->kind == A_INT) {
        if (token.kind == T_MINUS) {
          arg->as.fac.asint = 0xFFFF - arg->as.fac.asint + 1;
          return arg;
        } else if (token.kind == T_PLUS) {
          return arg;
        }
      }

      return ast_malloc((ast_t){
          A_UNARYOP, location_union(token.loc, arg->loc), {}, {.unaryop = {token.kind, arg}}});
    default:
      break;
  }

  ast_t *ast = parse_access(tokenizer);

  if (token_next_if_kind(tokenizer, T_SQO)) {
    ast_t *expr = parse_expr(tokenizer);
    token_t sqc = token_expect(tokenizer, T_SQC);

    ast->loc = location_union(ast->loc, sqc.loc);
    ast = ast_malloc((ast_t){A_UNARYOP, ast->loc, {}, {.unaryop = {T_STAR, ast_malloc((ast_t){A_BINARYOP, ast->loc, {}, {.binaryop = {T_PLUS, ast, expr}}})}}});
  }

  return ast;
}

ast_t *parse_term(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *a = parse_unary(tokenizer);
  token_t token;
  while (token = token_peek(tokenizer), token.kind == T_STAR || token.kind == T_SLASH) {
    token_next(tokenizer);
    ast_t *b = parse_unary(tokenizer);
    a = ast_malloc((ast_t){A_BINARYOP, location_union(a->loc, b->loc), {}, {.binaryop = {token.kind, a, b}}});
  }

  return a;
}

ast_t *parse_atom1(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *a = parse_term(tokenizer);

  token_t token;
  while (token = token_peek(tokenizer), token.kind == T_PLUS || token.kind == T_MINUS) {
    token_next(tokenizer);
    ast_t *b = parse_term(tokenizer);
    a = ast_malloc((ast_t){A_BINARYOP, location_union(a->loc, b->loc), {}, {.binaryop = {token.kind, a, b}}});
  }

  return a;
}

ast_t *parse_atom(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *a = parse_atom1(tokenizer);

  token_t t = token_peek(tokenizer);
  if (token_next_if_kind(tokenizer, T_SHL) || token_next_if_kind(tokenizer, T_SHR)) {
    ast_t *b = parse_atom1(tokenizer);
    a = ast_malloc((ast_t){A_BINARYOP, location_union(a->loc, b->loc), {}, {.binaryop = {t.kind, a, b}}});
  }

  return a;
}

ast_t *parse_comp(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *ast = parse_atom(tokenizer);

  token_t t = token_peek(tokenizer);
  if (token_next_if_kind(tokenizer, T_EQ) || token_next_if_kind(tokenizer, T_NEQ)) {
    ast_t *b = parse_atom(tokenizer);
    ast = ast_malloc((ast_t){A_BINARYOP, location_union(ast->loc, b->loc), {}, {.binaryop = {t.kind, ast, b}}});
  }

  return ast;
}

ast_t *parse_expr(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;
  int not= token_next_if_kind(tokenizer, T_NOT);
  ast_t *ast = parse_comp(tokenizer);

  if (not) {
    ast = ast_malloc((ast_t){A_UNARYOP, location_union(start, ast->loc), {}, {.unaryop = {T_NOT, ast}}});
  }

  return ast;
}

ast_t *parse_decl(tokenizer_t *tokenizer) {
  assert(tokenizer);
  location_t start = tokenizer->loc;
  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *array_len_expr = NULL;
  array_len_t array_len = {ARRAY_LEN_NOTARRAY, 0};
  if (token_next_if_kind(tokenizer, T_SQO)) {
    if (token_next_if_kind(tokenizer, T_SQC)) {
      array_len = (array_len_t){ARRAY_LEN_UNSET, 0};
    } else {
      array_len_expr = parse_expr(tokenizer);
      token_expect(tokenizer, T_SQC);
      array_len = (array_len_t){ARRAY_LEN_EXPR, 0};
      if (array_len_expr->kind == A_INT) {
        array_len = (array_len_t){ARRAY_LEN_NUM, array_len_expr->as.fac.asint};
        array_len_expr = NULL;
      }
    }
  }
  ast_t *expr = NULL;
  if (token_next_if_kind(tokenizer, T_EQUAL)) {
    expr = parse_expr(tokenizer);
  }
  if (array_len.kind != ARRAY_LEN_NOTARRAY) {
    type = (type_t){TY_ARRAY, 0, {.array = {type_malloc(type), array_len}}};
  }
  if (expr && array_len.kind != ARRAY_LEN_UNSET) {
    expr = ast_malloc((ast_t){A_CAST, expr->loc, {}, {.cast = {type, expr}}});
  }
  ast_t *ast = ast_malloc((ast_t){A_DECL, location_union(start, expr ? expr->loc : name.loc), {}, {.decl = {type, name, expr, array_len_expr}}});
  if (array_len.kind == ARRAY_LEN_NOTARRAY) {
    if (token_next_if_kind(tokenizer, T_COMMA)) {
      ast = ast_malloc((ast_t){A_LIST, ast->loc, {}, {.binary = {ast, NULL}}});
      ast_t **asti = &ast->as.binary.right;

      do {
        start = tokenizer->loc;
        bool ptr = token_next_if_kind(tokenizer, T_STAR);
        name = token_expect(tokenizer, T_SYM);
        expr = NULL;
        if (token_next_if_kind(tokenizer, T_EQUAL)) {
          expr = parse_expr(tokenizer);
        }
        type_t _type = type;
        if (ptr) {
          _type = (type_t){TY_PTR, 2, {.ptr = type_malloc(type)}};
        }
        ast_t *_ast = ast_malloc((ast_t){A_DECL, location_union(start, expr ? expr->loc : name.loc), {}, {.decl = {_type, name, expr, array_len_expr}}});
        *asti = ast_malloc((ast_t){A_LIST, _ast->loc, {}, {.binary = {_ast, NULL}}});
        asti = &(*asti)->as.binary.right;
      } while (token_next_if_kind(tokenizer, T_COMMA));
    }
  }
  return ast;
}

ast_t *parse_asm(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;
  token_expect(tokenizer, T_ASM);
  token_expect(tokenizer, T_PARO);
  token_t str = token_expect(tokenizer, T_STRING);
  token_expect(tokenizer, T_PARC);

  return ast_malloc((ast_t){A_ASM, location_union(start, tokenizer->loc), {}, {.fac = str}});
}

ast_t *parse_statement(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;

  if (token_next_if_kind(tokenizer, T_SEMICOLON)) {
    return NULL;
  }

  if (token_next_if_kind(tokenizer, T_BREAK)) {
    location_t loc = tokenizer->last_token.loc;
    token_expect(tokenizer, T_SEMICOLON);
    return ast_malloc((ast_t){A_BREAK, loc, {}, {}});
  }

  if (token_next_if_kind(tokenizer, T_RETURN)) {
    ast_t *expr = NULL;
    if (token_peek(tokenizer).kind != T_SEMICOLON) {
      expr = parse_expr(tokenizer);
    }
    token_expect(tokenizer, T_SEMICOLON);
    return ast_malloc((ast_t){A_RETURN, location_union(start, expr ? expr->loc : start), {}, {.ast = expr}});
  }

  tokenizer_t savetok = *tokenizer;
  catch = true;
  if (setjmp(catch_buf) == 0) {
    if (token_peek(tokenizer).kind == T_STRUCT) {
      catch = false;
    }
    ast_t *ast = parse_decl(tokenizer);
    token_expect(tokenizer, T_SEMICOLON);
    catch = false;
    return ast;
  }

  *tokenizer = savetok;
  catch = true;
  if (setjmp(catch_buf) == 0) {
    ast_t *ast = parse_asm(tokenizer);
    token_expect(tokenizer, T_SEMICOLON);
    catch = false;
    return ast;
  }

  *tokenizer = savetok;
  ast_t *a = parse_expr(tokenizer);
  if (token_next_if_kind(tokenizer, T_EQUAL)) {
    ast_t *b = parse_expr(tokenizer);
    token_expect(tokenizer, T_SEMICOLON);
    a = ast_malloc((ast_t){A_ASSIGN, location_union(a->loc, b->loc), {}, {.binary = {a, b}}});
    return ast_malloc((ast_t){A_STATEMENT, a->loc, {}, {.ast = a}});
  }

  token_expect(tokenizer, T_SEMICOLON);

  a = ast_malloc((ast_t){A_STATEMENT, a->loc, {}, {.ast = a}});

  return a;
}

ast_t *parse_block(tokenizer_t *tokenizer);
ast_t *parse_if(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;

  token_expect(tokenizer, T_IF);
  token_expect(tokenizer, T_PARO);
  ast_t *cond = parse_expr(tokenizer);
  token_expect(tokenizer, T_PARC);
  ast_t *then = parse_block(tokenizer);
  ast_t *else_ = NULL;
  if (token_next_if_kind(tokenizer, T_ELSE)) {
    if (token_peek(tokenizer).kind == T_IF) {
      else_ = parse_if(tokenizer);
    } else {
      else_ = parse_block(tokenizer);
    }
  }

  location_t end = tokenizer->loc;

  return ast_malloc((ast_t){A_IF, location_union(start, end), {}, {.if_ = {cond, then, else_}}});
}

ast_t *parse_for(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;

  token_expect(tokenizer, T_FOR);
  token_expect(tokenizer, T_PARO);
  ast_t *init = parse_statement(tokenizer);
  ast_t *cond = parse_expr(tokenizer);
  token_expect(tokenizer, T_SEMICOLON);

  ast_t *inc = NULL;
  if (token_peek(tokenizer).kind != T_PARC) {
    inc = parse_expr(tokenizer);
    if (token_peek(tokenizer).kind != T_PARC) {
      token_expect(tokenizer, T_EQUAL);
      ast_t *b = parse_expr(tokenizer);

      inc = ast_malloc((ast_t){A_ASSIGN, location_union(inc->loc, b->loc), {}, {.binary = {inc, b}}});
    }
  }
  if (inc) {
    inc = ast_malloc((ast_t){A_STATEMENT, inc->loc, {}, {.ast = inc}});
  }

  token_expect(tokenizer, T_PARC);

  location_t end = tokenizer->loc;
  ast_t *body = parse_block(tokenizer);

  if (inc && body) {
    assert(body->kind == A_BLOCK);
    ast_t *a = body->as.ast;
    assert(a->kind == A_LIST);
    while (a->as.binary.right) {
      a = a->as.binary.right;
      assert(a->kind == A_LIST);
    }

    a->as.binary.right = ast_malloc((ast_t){A_LIST, inc->loc, {}, {.binary = {inc, NULL}}});
  } else if (inc) {
    body = ast_malloc((ast_t){A_BLOCK, inc->loc, {}, {.ast = inc}});
  }

  location_t loc = location_union(start, end);

  ast_t *ast = ast_malloc((ast_t){A_WHILE, loc, {}, {.binary = {cond, body}}});

  if (init) {
    ast = ast_malloc((ast_t){A_BLOCK, loc, {}, {.ast = ast_malloc((ast_t){A_LIST, loc, {}, {.binary = {init, ast_malloc((ast_t){A_LIST, loc, {}, {.binary = {ast, NULL}}})}}})}});
  }

  return ast;
}

ast_t *parse_while(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;

  token_expect(tokenizer, T_WHILE);
  token_expect(tokenizer, T_PARO);
  ast_t *cond = parse_expr(tokenizer);
  token_expect(tokenizer, T_PARC);
  ast_t *body = parse_block(tokenizer);

  return ast_malloc((ast_t){A_WHILE, location_union(start, tokenizer->loc), {}, {.binary = {cond, body}}});
}

ast_t *parse_code(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);

  if (token.kind == T_BRO) {
    return parse_block(tokenizer);
  }

  if (token.kind == T_IF) {
    return parse_if(tokenizer);
  }

  if (token.kind == T_FOR) {
    return parse_for(tokenizer);
  }

  if (token.kind == T_WHILE) {
    return parse_while(tokenizer);
  }

  return parse_statement(tokenizer);
}

ast_t *parse_block(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_expect(tokenizer, T_BRO);

  if (token_next_if_kind(tokenizer, T_BRC)) {
    return NULL;
  }

  ast_t *code = parse_code(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_LIST, location_union(token.loc, code->loc), {}, {.binary = {code, NULL}}});
  ast_t *block = ast;

  while (token_peek(tokenizer).kind != T_BRC) {
    code = parse_code(tokenizer);
    if (code) {
      block->as.binary.right = ast_malloc((ast_t){A_LIST, location_union(ast->loc, code->loc), {}, {.binary = {code, NULL}}});
      block = block->as.binary.right;
    }
  }

  token_t end = token_expect(tokenizer, T_BRC);

  return ast_malloc((ast_t){A_BLOCK, location_union(token.loc, end.loc), {}, {.ast = ast}});
}

ast_t *parse_paramdef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t par = token_expect(tokenizer, T_PARO);
  if (token_next_if_kind(tokenizer, T_PARC)) {
    return NULL;
  }

  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *ast = ast_malloc((ast_t){A_PARAMDEF, location_union(par.loc, name.loc), {}, {.paramdef = {type, name, NULL}}});
  ast_t *asti = ast;

  while (token_next_if_kind(tokenizer, T_COMMA)) {
    type = parse_type(tokenizer);
    name = token_expect(tokenizer, T_SYM);
    asti->as.paramdef.next = ast_malloc((ast_t){A_PARAMDEF, location_union(ast->loc, name.loc), {}, {.paramdef = {type, name, NULL}}});
    asti = asti->as.paramdef.next;
  }

  token_expect(tokenizer, T_PARC);

  return ast;
}

ast_t *parse_funcdecl(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;

  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);

  ast_t *param = parse_paramdef(tokenizer);

  ast_t *block = parse_block(tokenizer);

  return ast_malloc((ast_t){A_FUNCDECL, location_union(start, block ? block->loc : (param ? param->loc : name.loc)), {}, {.funcdecl = {type, name, param, block}}});
}

ast_t *parse_funcdef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;

  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);

  ast_t *param = parse_paramdef(tokenizer);

  token_expect(tokenizer, T_SEMICOLON);

  return ast_malloc((ast_t){A_FUNCDEF, location_union(start, param ? param->loc : name.loc), {}, {.funcdef = {type, name, param}}});
}

ast_t *parse_typedef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t td = token_expect(tokenizer, T_TYPEDEF);

  type_t type = {0};

  switch (token_peek(tokenizer).kind) {
    case T_STRUCT:
      type = parse_structdef(tokenizer);
      break;
    case T_ENUM:
      type = parse_enumdef(tokenizer);
      break;
    default:
      type = parse_type(tokenizer);
  }

  token_t name = token_expect(tokenizer, T_SYM);
  token_expect(tokenizer, T_SEMICOLON);

  return ast_malloc((ast_t){A_TYPEDEF, td.loc, {}, {.typedef_ = {type, name}}});
}

ast_t *parse_extern(tokenizer_t *tokenizer) {
  assert(tokenizer);

  location_t start = tokenizer->loc;

  token_expect(tokenizer, T_EXTERN);
  ast_t *funcdef = parse_funcdef(tokenizer);

  return ast_malloc((ast_t){A_EXTERN, location_union(start, funcdef->loc), {}, {.ast = funcdef}});
}

ast_t *parse_global(tokenizer_t *tokenizer) {
  assert(tokenizer);

  if (token_peek(tokenizer).kind == T_TYPEDEF) {
    return parse_typedef(tokenizer);
  }

  if (token_peek(tokenizer).kind == T_EXTERN) {
    return parse_extern(tokenizer);
  }

  tokenizer_t savetok = *tokenizer;
  catch = true;
  if (setjmp(catch_buf) == 0) {
    ast_t *decl = parse_decl(tokenizer);
    token_expect(tokenizer, T_SEMICOLON);
    decl->kind = A_GLOBDECL;
    catch = false;
    return decl;
  }
  *tokenizer = savetok;

  return parse_funcdecl(tokenizer);
}

ast_t *parse(tokenizer_t *tokenizer) {
  assert(tokenizer);

  if (token_peek(tokenizer).kind == T_NONE) {
    return NULL;
  }

  ast_t *glob = parse_global(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_LIST, glob->loc, {}, {.binary = {glob, NULL}}});
  ast_t *asti = ast;

  while (token_peek(tokenizer).kind != T_NONE) {
    ast_t *glob = parse_global(tokenizer);
    asti->as.binary.right = ast_malloc((ast_t){A_LIST, glob->loc, {}, {.binary = {glob, NULL}}});
    asti = asti->as.binary.right;
  }

  return ast;
}

void typecheck(ast_t *ast, state_t *state);
void typecheck_expect(ast_t *ast, state_t *state, type_t type) {
  assert(ast);

  if (ast->type.kind == TY_NONE) {
    typecheck(ast, state);
  }

  type_expect(ast->loc, &ast->type, &type);
}

void typecheck_expandable(ast_t *ast, state_t *state, type_t type) {
  assert(ast);

  if (ast->type.kind == TY_NONE) {
    typecheck(ast, state);
  }

  if (type_greaterthan(&type, &ast->type)) {
    return;
  }

  type_expect(ast->loc, &ast->type, &type);
}

void typecheck_funcbody(ast_t *ast, state_t *state, type_t ret) {
  assert(ast);
  assert(state);
  assert(ret.kind != TY_FUNC);

  switch (ast->kind) {
    case A_BLOCK:
      state_push_scope(state);
      assert(ast->as.ast);
      typecheck_funcbody(ast->as.ast, state, ret);
      state_drop_scope(state);
      break;
    case A_LIST:
      for (ast_t *asti = ast; asti; asti = asti->as.binary.right) {
        typecheck_funcbody(asti->as.binary.left, state, ret);
        asti->type = (type_t){TY_VOID, 0, {}};
      }
      break;
    case A_RETURN:
      if (ast->as.ast) {
        typecheck_expect(ast->as.ast, state, ret);
      }
      ast->type = (type_t){TY_VOID, 0, {}};
      break;
    case A_IF:
      assert(ast->as.if_.cond);
      typecheck(ast->as.if_.cond, state);
      if (ast->as.if_.cond->type.kind != TY_INT && ast->as.if_.cond->type.kind != TY_PTR) {
        eprintf(ast->as.if_.cond->loc, "expected INT or PTR, found '%s'", type_dump_to_string(&ast->as.if_.cond->type));
      }
      if (ast->as.if_.then) {
        typecheck_funcbody(ast->as.if_.then, state, ret);
      }
      if (ast->as.if_.else_) {
        typecheck_funcbody(ast->as.if_.else_, state, ret);
      }
      break;
    case A_WHILE:
      assert(ast->as.binary.left);
      typecheck_expandable(ast->as.binary.left, state, (type_t){TY_INT, 2, {}});
      if (ast->as.binary.right) {
        typecheck_funcbody(ast->as.binary.right, state, ret);
      }
      break;
    default:
      typecheck(ast, state);
  }
  ast->type = (type_t){TY_VOID, 0, {}};
}

void typecheck(ast_t *ast, state_t *state) {
  assert(state);

  if (!ast) {
    return;
  }

  switch (ast->kind) {
    case A_NONE:
    case A_IF:
    case A_BLOCK:
    case A_RETURN:
    case A_WHILE:
      assert(0);
    case A_LIST:
      assert(ast->as.binary.left);
      typecheck(ast->as.binary.left, state);
      typecheck(ast->as.binary.right, state);
      ast->type = (type_t){TY_VOID, 0, {}};
      break;
    case A_STATEMENT:
      assert(ast->as.ast);
      typecheck(ast->as.ast, state);
      ast->type = (type_t){TY_VOID, 0, {}};
      break;
    case A_FUNCDECL:
      // TODO: why not solve_type_alias there?
      state_add_symbol(state, (symbol_t){ast->as.funcdecl.name, &ast->type, 0, {}});
      state_push_scope(state);
      state->param = 0;
      typecheck(ast->as.funcdecl.params, state);
      state_solve_type_alias(state, &ast->as.funcdecl.type);
      ast->type = (type_t){TY_FUNC, ast->as.funcdecl.type.size, {.func = {&ast->as.funcdecl.type, ast->as.funcdecl.params ? &ast->as.funcdecl.params->type : NULL}}};
      if (ast->as.funcdecl.block) {
        assert(ast->as.funcdecl.block->kind == A_BLOCK);
        typecheck_funcbody(ast->as.funcdecl.block->as.ast, state, ast->as.funcdecl.type);
      }
      state_drop_scope(state);
      break;
    case A_FUNCDEF:
      state_add_symbol(state, (symbol_t){ast->as.funcdef.name, &ast->type, 0, {}});
      state_push_scope(state);
      state->param = 0;
      typecheck(ast->as.funcdef.params, state);
      state_solve_type_alias(state, &ast->as.funcdef.type);
      ast->type =
          (type_t){TY_FUNC,
                   ast->as.funcdef.type.size,
                   {.func = {&ast->as.funcdef.type,
                             ast->as.funcdef.params ? &ast->as.funcdef.params->type : NULL}}};
      state_drop_scope(state);
      break;
    case A_PARAMDEF:
    {
      scope_t *scope = &state->scopes[state->scope_num - 1];
      assert(scope->symbol_num + 1 < SYMBOL_MAX);
      ++state->param;
      state_solve_type_alias(state, &ast->as.paramdef.type);
      scope->symbols[scope->symbol_num++] = (symbol_t){
          ast->as.paramdef.name, &ast->as.paramdef.type, INFO_LOCAL, {-1 - state->param}};

      typecheck(ast->as.paramdef.next, state);
      ast->type = (type_t){TY_PARAM,
                           ast->as.paramdef.type.size,
                           {.list = {&ast->as.paramdef.type,
                                     ast->as.paramdef.next ? &ast->as.paramdef.next->type : NULL}}};
    } break;
    case A_BINARYOP:
    {
      assert(ast->as.binaryop.lhs);
      assert(ast->as.binaryop.rhs);
      typecheck(ast->as.binaryop.lhs, state);
      type_t *type = &ast->as.binaryop.lhs->type;

      switch (ast->as.binaryop.op) {
        case T_DOT:
        {
          assert(ast->as.binaryop.rhs->kind == A_SYM);

          if (!type_is_kind(type, TY_STRUCT)) {
            eprintf(ast->as.binaryop.lhs->loc,
                    "expected a 'STRUCT', found '%s'",
                    type_dump_to_string(type));
          }

          type_t *f = type->as.alias.type->as.struct_.fieldlist;
          while (!sv_eq(f->as.fieldlist.name.image, ast->as.binaryop.rhs->as.fac.image)) {
            f = f->as.fieldlist.next;
          };
          if (!f) {
            eprintf(ast->as.binaryop.rhs->loc,
                    "member not found in '%s'",
                    type_dump_to_string(type->as.alias.type));
          }

          ast->type = *f->as.fieldlist.type;

        } break;
        case T_EQ:
        case T_NEQ:
          typecheck_expandable(ast->as.binaryop.rhs, state, ast->as.binaryop.lhs->type);
          if (!type_is_kind(type, TY_INT) && !type_is_kind(type, TY_PTR)) {
            eprintf(ast->loc,
                    "invalid operation '%s' between '%s' and '%s'",
                    token_kind_to_string(ast->as.binaryop.op),
                    type_dump_to_string(&ast->as.binaryop.lhs->type),
                    type_dump_to_string(&ast->as.binaryop.rhs->type));
          }
          ast->type = ast->as.binaryop.lhs->type;
          break;
        case T_PLUS:
        case T_MINUS:
          if (type_is_kind(type, TY_PTR) && ast->as.binaryop.op == T_MINUS) {
            typecheck(ast->as.binaryop.rhs, state);
            if (type_is_kind(&ast->as.binaryop.rhs->type, TY_INT)) {
              ast->type = *type;
            } else if (type_is_kind(&ast->as.binaryop.rhs->type, TY_PTR)) {
              type_expect(ast->as.binaryop.lhs->loc, &ast->as.binaryop.lhs->type, type);
              ast->type = (type_t){TY_INT, 2, {}};
            } else {
              eprintf(ast->loc,
                      "invalid operation '%s' between '%s' and '%s'",
                      token_kind_to_string(ast->as.binaryop.op),
                      type_dump_to_string(&ast->as.binaryop.lhs->type),
                      type_dump_to_string(&ast->as.binaryop.rhs->type));
            }
          } else {
            typecheck_expandable(ast->as.binaryop.rhs, state, (type_t){TY_INT, 2, {}});
            if (type_is_kind(type, TY_INT) || type_is_kind(type, TY_PTR)) {
              ast->type = *type;
            } else if (type->kind == TY_ARRAY) {
              ast->type = (type_t){TY_PTR, 2, {.ptr = type->as.array.type}};
              ast_t *lhs = ast->as.binaryop.lhs;
              ast->as.binaryop.lhs =
                  ast_malloc((ast_t){A_CAST, lhs->loc, {}, {.cast = {ast->type, lhs}}});
            } else {
              eprintf(ast->loc,
                      "invalid operation '%s' between '%s' and '%s'",
                      token_kind_to_string(ast->as.binaryop.op),
                      type_dump_to_string(&ast->as.binaryop.lhs->type),
                      type_dump_to_string(&ast->as.binaryop.rhs->type));
            }
          }
          break;
        case T_SHR:
        case T_SHL:
          typecheck_expandable(ast->as.binaryop.lhs, state, (type_t){TY_INT, 2, {}});
          typecheck_expandable(ast->as.binaryop.rhs, state, (type_t){TY_INT, 2, {}});
          ast->type = (type_t){TY_INT, 2, {}};
          break;
        default:
          printf("todo op: %s\n", token_kind_to_string(ast->as.binaryop.op));
          TODO;
      }
    } break;
    case A_UNARYOP:
      assert(ast->as.unaryop.arg);
      switch (ast->as.unaryop.op) {
        case T_MINUS:
          typecheck_expandable(ast->as.unaryop.arg, state, (type_t){TY_INT, 2, {}});
          ast->type.kind = TY_INT;
          break;
        case T_AND:
          typecheck(ast->as.unaryop.arg, state);
          ast->type = (type_t){TY_PTR, 2, {.ptr = &ast->as.unaryop.arg->type}};
          break;
        case T_STAR:
          typecheck(ast->as.unaryop.arg, state);
          if (type_is_kind(&ast->as.unaryop.arg->type, TY_PTR)) {
            ast->type = *ast->as.unaryop.arg->type.as.ptr;
          } else if (type_is_kind(&ast->as.unaryop.arg->type, TY_ARRAY)) {
            ast->type = *ast->as.unaryop.arg->type.as.array.type;
          } else {
            char *type = type_dump_to_string(&ast->as.unaryop.arg->type);
            eprintf(ast->as.unaryop.arg->loc, "cannot dereference non PTR type: '%s'", type);
          }
          break;
        case T_NOT:
          typecheck_expect(ast->as.unaryop.arg, state, (type_t){TY_INT, 2, {}});
          ast->type = (type_t){TY_INT, 2, {}};
          break;
        default:
          TODO;
      }
      break;
    case A_INT:
      if (ast->as.fac.kind == T_CHAR || (ast->as.fac.kind == T_HEX && ast->as.fac.image.len == 4)) {
        ast->type = (type_t){TY_CHAR, 1, {}};
      } else {
        ast->type = (type_t){TY_INT, 2, {}};
      }
      break;
    case A_STRING:
      ast->type = (type_t){TY_PTR, 2, {.ptr = type_malloc((type_t){TY_CHAR, 1, {}})}};
      break;
    case A_SYM:
      ast->type = *state_find_symbol(state, ast->as.fac)->type;
      break;
    case A_GLOBDECL:
    case A_DECL:
      state_solve_type_alias(state, &ast->as.decl.type);

      if (type_is_kind(&ast->as.decl.type, TY_ARRAY)
          && ast->as.decl.type.as.array.len.kind == ARRAY_LEN_UNSET && ast->as.decl.expr == NULL) {
        eprintf(ast->loc, "array without length uninitialized");
      }
      if (type_is_kind(&ast->as.decl.type, TY_ARRAY)
          && ast->as.decl.type.as.array.len.kind == ARRAY_LEN_EXPR) {
        if (ast->as.decl.expr != NULL) {
          eprintf(ast->loc, "array with variable length cannot be initialized");
        } else {
          typecheck_expect(ast->as.decl.array_len, state, (type_t){TY_INT, 2, {}});
        }
      }
      if (ast->as.decl.expr) {
        typecheck_expandable(ast->as.decl.expr, state, ast->as.decl.type);
        if (type_is_kind(&ast->as.decl.type, TY_ARRAY)
            && ast->as.decl.type.as.array.len.kind == ARRAY_LEN_UNSET) {
          typecheck(ast->as.decl.expr, state);
          ast->as.decl.type = ast->as.decl.expr->type;
        }
        ast->as.decl.expr->type = ast->as.decl.type;
      }

      if (ast->as.decl.type.size == 0) {
        eprintf(ast->loc, "variable has incomplete type: %s", type_dump_to_string(&ast->as.decl.type));
      }

      state_add_symbol(state, (symbol_t){ast->as.decl.name, &ast->as.decl.type, 0, {}});
      ast->type = (type_t){TY_VOID, 0, {}};
      break;
    case A_ASSIGN:
      assert(ast->as.binary.left);
      assert(ast->as.binary.right);
      typecheck(ast->as.binary.left, state);
      if (type_is_kind(&ast->as.binary.left->type, TY_ARRAY)) {
        eprintf(ast->loc, "assign to array");
      }
      if (type_is_kind(&ast->as.binary.left->type, TY_ENUM)) {
        eprintf(ast->loc, "assign to enumerator");
      }
      typecheck_expect(ast->as.binary.right, state, ast->as.binary.left->type);
      ast->type = ast->as.binary.left->type;
      break;
    case A_FUNCALL:
    {
      symbol_t *s = state_find_symbol(state, ast->as.funcall.name);
      if (s->type->kind != TY_FUNC) {
        char *foundstr = type_dump_to_string(s->type);
        eprintf(ast->loc, "expected 'TY_FUNC', found '%s'", foundstr);
      }
      if (s->type->as.func.params == NULL && ast->as.funcall.params != NULL) {
        eprintf(ast->loc, "too many parameters");
      }
      if (s->type->as.func.params != NULL && ast->as.funcall.params == NULL) {
        eprintf(ast->loc, "too few parameters");
      }
      if (s->type->as.func.params) {
        typecheck_expect(ast->as.funcall.params, state, *s->type->as.func.params);
      }
      assert(s->type->as.func.ret);
      ast->type = *s->type->as.func.ret;
    } break;
    case A_PARAM:
      assert(ast->as.binary.left);
      typecheck(ast->as.binary.left, state);
      typecheck(ast->as.binary.right, state);
      ast->type = (type_t){TY_PARAM,
                           ast->as.binary.left->type.size,
                           {.list = {&ast->as.binary.left->type,
                                     ast->as.binary.right ? &ast->as.binary.right->type : NULL}}};
      break;
    case A_ARRAY:
      assert(ast->as.binary.left);
      if (ast->as.binary.right) {
        typecheck(ast->as.binary.left, state);
        typecheck(ast->as.binary.right, state);
        assert(type_is_kind(&ast->as.binary.right->type, TY_ARRAY));
        type_cmp(&ast->as.binary.left->type, ast->as.binary.right->type.as.array.type);

        type_t type = ast->as.binary.right->type;
        type.as.array.len.num += 1;
        type.size += ast->as.binary.left->type.size;
        ast->type = type;
      } else {
        typecheck(ast->as.binary.left, state);
        ast->type = (type_t){TY_ARRAY,
                             ast->as.binary.left->type.size,
                             {.array = {&ast->as.binary.left->type, .len = {ARRAY_LEN_NUM, 1}}}};
      }
      break;
    case A_TYPEDEF:
    {
      type_t *type = &ast->as.typedef_.type;
      if (type->kind == TY_STRUCT && type->as.struct_.name.kind != T_NONE) {
        state_add_symbol(state, (symbol_t){type->as.struct_.name, type, INFO_TYPEINCOMPLETE, {}});
      }
      if (type->kind == TY_ENUM) {
        for (type_t *typei = type; typei; typei = typei->as.enum_.next) {
          state_add_symbol(state, (symbol_t){typei->as.enum_.name, type, 0, {}});
        }
      }

      state_solve_type_alias(state, type);
      state_add_symbol(state,
                       (symbol_t){ast->as.typedef_.name, &ast->as.typedef_.type, INFO_TYPE, {}});
      ast->type = (type_t){TY_VOID, 0, {}};
    } break;
    case A_CAST:
      state_solve_type_alias(state, &ast->as.cast.target);

      if (type_is_kind(&ast->as.cast.target, TY_STRUCT) && ast->as.cast.ast->kind == A_ARRAY) {
        type_t *target = type_pass_alias(&ast->as.cast.target);
        type_t *f = target->as.struct_.fieldlist;
        ast_t *expr = ast->as.cast.ast;

        while (f && expr) {
          if (type_is_kind(f->as.fieldlist.type, TY_PTR) && expr->as.binary.left->kind == A_INT
              && expr->as.binary.left->as.fac.asint == 0) {
            expr->as.binary.left->type = *f->as.fieldlist.type;
          } else {
            typecheck_expect(expr->as.binary.left, state, *f->as.fieldlist.type);
          }
          expr->type = *target;
          expr->type.as.struct_.fieldlist = f;

          expr = expr->as.binary.right;
          f = f->as.fieldlist.next;
        }

        if (f != NULL
            && !(ast->as.cast.ast->kind == A_ARRAY
                 && ast->as.cast.ast->as.binary.left
                 && ast->as.cast.ast->as.binary.left->kind == A_INT
                 && ast->as.cast.ast->as.binary.left->as.fac.asint == 0
                 && ast->as.cast.ast->as.binary.right == NULL)) {
          eprintf(ast->loc, "missing fields");
        }
        if (expr != NULL) {
          eprintf(ast->loc, "too many fields");
        }
      } else {
        typecheck_expandable(ast->as.cast.ast, state, ast->as.cast.target);
      }
      ast->type = ast->as.cast.target;
      break;
    case A_ASM:
      ast->type = (type_t){TY_VOID, 0, {}};
      break;
    case A_EXTERN:
      typecheck(ast->as.ast, state);
      ast->type = (type_t){TY_VOID, 0, {}};
      break;
    case A_BREAK:
      ast->type = (type_t){TY_VOID, 0, {}};
      break;
  }
}

void optimize_ast(ast_t **astp, bool debug_opt) {
  assert(astp);
  ast_t *ast = *astp;
  if (!ast) {
    return;
  }

  switch (ast->kind) {
    case A_NONE:
      assert(0);
    case A_PARAMDEF:
    case A_INT:
    case A_SYM:
    case A_STRING:
    case A_TYPEDEF:
    case A_ASM:
    case A_FUNCDEF:
    case A_EXTERN:
    case A_BREAK:
      break;
    case A_LIST:
    case A_ASSIGN:
    case A_ARRAY:
    case A_PARAM:
      optimize_ast(&ast->as.binary.left, debug_opt);
      optimize_ast(&ast->as.binary.right, debug_opt);
      break;
    case A_FUNCDECL:
      optimize_ast(&ast->as.funcdecl.block, debug_opt);
      break;
    case A_STATEMENT:
    case A_RETURN:
    case A_BLOCK:
      optimize_ast(&ast->as.ast, debug_opt);
      break;
    case A_BINARYOP:
    {
      ast_t *a = ast->as.binaryop.lhs;
      ast_t *b = ast->as.binaryop.rhs;
      optimize_ast(&a, debug_opt);
      optimize_ast(&b, debug_opt);
    } break;
    case A_UNARYOP:
      optimize_ast(&ast->as.unaryop.arg, debug_opt);
      break;
    case A_DECL:
    case A_GLOBDECL:
      optimize_ast(&ast->as.decl.expr, debug_opt);
      break;
    case A_FUNCALL:
      optimize_ast(&ast->as.funcall.params, debug_opt);
      break;
    case A_CAST:
      optimize_ast(&ast->as.cast.ast, debug_opt);
      break;
    case A_IF:
    {
      if (debug_opt) {
        ast_dump(ast, 0);
        printf(" -> ");
      }

      ast_t *cond = ast->as.if_.cond;
      if (cond->kind == A_BINARYOP && (cond->as.binaryop.op == T_EQ || cond->as.binaryop.op == T_NEQ)) {

        cond->as.binaryop.op = T_MINUS;
        if (cond->as.binaryop.op == T_EQ) {
          ast_t *then = ast->as.if_.then;
          ast->as.if_.then = ast->as.if_.else_;
          ast->as.if_.else_ = then;
        }

      } else if (cond->kind == A_UNARYOP && cond->as.unaryop.op == T_NOT) {

        ast->as.if_.cond = cond->as.unaryop.arg;
        free_ptr(cond);

        ast_t *then = ast->as.if_.then;
        ast->as.if_.then = ast->as.if_.else_;
        ast->as.if_.else_ = then;
      }

      if (debug_opt) {
        ast_dump(ast, 0);
        printf("\n");
      }

      optimize_ast(&cond, debug_opt);
      optimize_ast(&ast->as.if_.then, debug_opt);
      if (ast->as.if_.else_) {
        optimize_ast(&ast->as.if_.else_, debug_opt);
      }
    } break;
    case A_WHILE:
    {
      ast_t *cond = ast->as.binary.left;
      if (cond->kind == A_BINARYOP
          && (cond->as.binaryop.op == T_EQ || cond->as.binaryop.op == T_NEQ)) {
        if (debug_opt) {
          ast_dump(ast, 0);
          printf(" -> ");
        }

        cond->as.binaryop.op = T_MINUS;
        if (cond->as.binaryop.op == T_EQ) {
          ast->as.binary.left =
              ast_malloc((ast_t){A_UNARYOP, cond->loc, cond->type, {.unaryop = {T_NOT, cond}}});
        }

        if (debug_opt) {
          ast_dump(ast, 0);
          printf("\n");
        }
      }

      optimize_ast(&cond, debug_opt);
      optimize_ast(&ast->as.binary.right, debug_opt);
    } break;
  }
}

void data(compiled_t *compiled, bytecode_t b) {
  assert(compiled);
  assert(compiled->data_num + 1 < DATA_MAX);
  if (compiled->data_num > 0 && compiled->data[compiled->data_num - 1].kind == BDB
      && b.kind == BDB) {
    compiled->data[compiled->data_num - 1].arg.num += b.arg.num;
    return;
  }
  compiled->data[compiled->data_num++] = b;
}

void code(compiled_t *compiled, bytecode_t b) {
  assert(compiled);
  if (compiled->is_init) {
    assert(compiled->init_num + 1 < CODE_MAX);
    compiled->init[compiled->init_num++] = b;
  } else {
    assert(compiled->code_num + 1 < CODE_MAX);
    compiled->code[compiled->code_num++] = b;
  }
}

bytecode_t bytecode_uli(bytecode_kind_t kind, instruction_t inst, int uli) {
  bytecode_t b = {kind, inst, {}};
  sprintf(b.arg.string, "_%03d", uli);
  return b;
}

int datauli(state_t *state) {
  assert(state);
  int uli = state->uli++;
  data(&state->compiled, (bytecode_t){BALIGN, 0, {}});
  data(&state->compiled, bytecode_uli(BSETLABEL, 0, uli));
  return uli;
}

bytecode_t bytecode_with_sv(bytecode_kind_t kind, instruction_t inst, sv_t sv) {
  char label[sv.len + 1];
  memset(label, 0, sv.len + 1);
  sprintf(label, SV_FMT, SV_UNPACK(sv));
  return bytecode_with_string(kind, inst, label);
}

void compile(ast_t *ast, state_t *state);
void get_addr_ast(state_t *state, ast_t *ast) {
  assert(state);
  assert(ast);

  switch (ast->kind) {
    case A_SYM:
    {
      symbol_t *s = state_find_symbol(state, ast->as.fac);
      if (s->kind == INFO_LOCAL) {
        state_add_ir(state, (ir_t){IR_ADDR_LOCAL, {.num = state->sp - s->info.local}});
      } else if (s->kind == INFO_GLOBAL) {
        state_add_ir(state, (ir_t){IR_ADDR_GLOBAL, {.loc = {s->info.global, 0}}});
      } else {
        assert(0);
      }
    } break;
    case A_UNARYOP:
      assert(ast->as.unaryop.op == T_STAR);
      get_addr_ast(state, ast->as.unaryop.arg);
      break;
    case A_BINARYOP:
      switch (ast->as.binaryop.op) {
        case T_DOT:
        {
          get_addr_ast(state, ast->as.binaryop.lhs);
          unsigned int offset = 0;
          type_t *f = ast->as.binaryop.lhs->type.as.alias.type->as.struct_.fieldlist;
          while (!sv_eq(f->as.fieldlist.name.image, ast->as.binaryop.rhs->as.fac.image)) {
            if (type_is_kind(f->as.fieldlist.type, TY_CHAR) && f->as.fieldlist.next
                && !type_is_kind(f->as.fieldlist.next->as.fieldlist.type, TY_CHAR)) {
              offset += 2;
            } else {
              offset += f->as.fieldlist.type->size;
            }
            f = f->as.fieldlist.next;
          };
          assert(f);
          assert(offset < 256);

          if (offset > 0) {
            state_add_addr_offset(state, offset);
          }
        } break;
        case T_PLUS:
        case T_MINUS:
        {
          assert(type_is_kind(&ast->type, TY_PTR));
          get_addr_ast(state, ast->as.binaryop.lhs);
          int size = ast->type.as.ptr->size;
          if (ast->as.binaryop.rhs->kind == A_INT) {
            state_add_addr_offset(state, ast->as.binaryop.rhs->as.fac.asint * size);
          } else {
            compile(ast->as.binaryop.rhs, state);
            if (size != 1) {
              state_add_ir(state, (ir_t){IR_MUL, {.num = size}});
            }
            state_add_ir(state, (ir_t){IR_OPERATION, {.inst = ast->as.binaryop.op == T_PLUS ? SUM : SUB}});
          }
        } break;
        default:
          assert(0);
      }
      break;
    case A_CAST:
      get_addr_ast(state, ast->as.cast.ast);
      break;
    default:
      printf("TODO: %s at %d of ", __FUNCTION__, __LINE__);
      ast_dump(ast, false);
      printf("\n");
      exit(1);
  }
}

void compile_data(ast_t *ast, state_t *state, int uli, int offset) {
  assert(ast);
  assert(state);
  compiled_t *compiled = &state->compiled;

  switch (ast->kind) {
    case A_CAST:
    {
      compile_data(ast->as.cast.ast, state, uli, 0);
      int delta = type_size_aligned(&ast->as.cast.target) - type_size_aligned(&ast->as.cast.ast->type);
      assert(delta >= 0);
      if (delta > 0) {
        data(compiled, (bytecode_t){BDB, 0, {.num = delta}});
      }
    } break;
    case A_ARRAY:
      if (type_is_kind(&ast->type, TY_ARRAY) && type_is_kind(ast->type.as.array.type, TY_PTR)) {
        TODO;
        // ast_t *node = ast;
        // int i = 0;
        // while (node) {
        //   i++;
        // ;
        // data(compiled,bytecode_uli() );
        //   ast_dump_tree(node->as.binary.left, 0, 0);
        //   compile_data(node->as.binary.left, state, element_uli, 0);
        //   node = node->as.binary.right;
        // }
        ast_t *node = ast;
        while (node) {
          int element_uli = datauli(state);
          ast_dump_tree(node->as.binary.left, 0, 0);
          compile_data(node->as.binary.left, state, element_uli, 0);
          node = node->as.binary.right;
        }
        break;
      }
      compile_data(ast->as.binary.left, state, uli, offset);
      if (ast->as.binary.right) {
        compile_data(ast->as.binary.right, state, uli, offset + ast->as.binary.left->type.size);
      }
      break;
    case A_INT:
      data(compiled,
           (bytecode_t){ast->type.size == 2 ? BHEX2 : BHEX, 0, {.num = ast->as.fac.asint}});
      break;
    case A_STRING:
      data(compiled, bytecode_with_sv(BSTRING, 0, (sv_t){ast->as.fac.image.start + 1, ast->as.fac.image.len - 2}));
      data(compiled, (bytecode_t){BHEX, 0, {.num = 0}});
      break;
    default:
    {
      int size = ast->type.size;
      data(compiled, (bytecode_t){BDB, 0, {.num = size}});
      state->is_init = true;
      compile(ast, state);
      state_add_ir(state, (ir_t){IR_ADDR_GLOBAL, {.loc = {uli, offset}}});
      state_add_ir(state, (ir_t){IR_WRITE, {.num = size}});
      state->is_init = false;
    }
  }
}

void compile(ast_t *ast, state_t *state) {
  assert(state);
  assert(ast);

  compiled_t *compiled = &state->compiled;

  switch (ast->kind) {
    case A_NONE:
      assert(0);
    case A_LIST:
      compile(ast->as.binary.left, state);
      if (ast->as.binary.right) {
        compile(ast->as.binary.right, state);
      }
      break;
    case A_BLOCK:
    {
      int start_sp = state->sp;
      state_push_scope(state);
      compile(ast->as.ast, state);
      state_drop_scope(state);
      state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = -(state->sp - start_sp)}});
    } break;
    case A_PARAM:
      if (ast->as.binary.right) {
        compile(ast->as.binary.right, state);
      }
      compile(ast->as.binary.left, state);
      break;
    case A_FUNCDECL:
      state_add_symbol(state, (symbol_t){ast->as.funcdecl.name, &ast->type, 0, {}});
      state_push_scope(state);
      state->param = 4 + type_size_aligned(&ast->as.funcdecl.type);
      if (ast->as.funcdecl.params) {
        compile(ast->as.funcdecl.params, state);
      }
      state_add_ir(state, (ir_t){IR_SETLABEL, {.sv = ast->as.funcdecl.name.image}});
      state->sp = 0;
      if (ast->as.funcdecl.block) {
        compile(ast->as.funcdecl.block->as.ast, state);
      }
      if (state->irs[state->ir_num - 1].kind != IR_FUNCEND) {
        state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = -state->sp}});
        state_add_ir(state, (ir_t){IR_FUNCEND, {}});
      }
      state_drop_scope(state);
      break;
    case A_FUNCDEF:
      state_add_symbol(state, (symbol_t){ast->as.funcdecl.name, &ast->type, 0, {}});
      break;
    case A_PARAMDEF:
      state_add_symbol(state, (symbol_t){ast->as.paramdef.name, &ast->as.paramdef.type, INFO_LOCAL, {-state->param}});
      state->param += type_size_aligned(&ast->as.paramdef.type);
      if (ast->as.paramdef.next) {
        compile(ast->as.paramdef.next, state);
      }
      break;
    case A_STATEMENT:
    {
      int sp_start = state->sp;
      compile(ast->as.ast, state);
      assert(ast->as.ast->kind != A_RETURN); // TODO: needed?
      assert(ast->as.ast->kind != A_DECL);
      int delta = state->sp - sp_start;
      if (delta != 0) {
        state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = -delta}});
      }
    } break;
    case A_RETURN:
    {
      compile(ast->as.ast, state);
      int size = type_size_aligned(&ast->as.ast->type);
      state_add_ir(state, (ir_t){IR_ADDR_LOCAL, {.num = state->sp + 2}});
      state_add_ir(state, (ir_t){IR_WRITE, {.num = size}});
      state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = -state->sp}});
      state_add_ir(state, (ir_t){IR_FUNCEND, {}});
    } break;
    case A_BINARYOP:
      switch (ast->as.binaryop.op) {
        case T_DOT:
          get_addr_ast(state, ast);
          state_add_ir(state, (ir_t){IR_READ, {.num = ast->type.size}});
          break;
        case T_PLUS:
        case T_MINUS:
          compile(ast->as.binaryop.lhs, state);
          compile(ast->as.binaryop.rhs, state);
          if (type_is_kind(&ast->type, TY_PTR)) {
            state_add_ir(state, (ir_t){IR_MUL, {.num = ast->type.as.ptr->size}});
            state_add_ir(state, (ir_t){IR_OPERATION, {.inst = ast->as.binaryop.op == T_PLUS ? SUM : SUB}});
          } else if (ast->as.binaryop.op == T_MINUS && ast->type.kind == TY_INT && type_is_kind(&ast->as.binaryop.lhs->type, TY_PTR) && type_is_kind(&ast->as.binaryop.rhs->type, TY_PTR)) {
            state_add_ir(state, (ir_t){IR_OPERATION, {.inst = SUB}});
            state_add_ir(state, (ir_t){IR_DIV, {.num = ast->as.binaryop.lhs->type.as.ptr->size}});
            break;
          } else {
            state_add_ir(state, (ir_t){IR_OPERATION, {.inst = ast->as.binaryop.op == T_PLUS ? SUM : SUB}});
          }
          break;
        case T_EQ:
        case T_NEQ:
        {
          state_add_ir(state, (ir_t){IR_INT, {.num = ast->as.binaryop.op == T_EQ}});
          compile(ast->as.binaryop.rhs, state);
          compile(ast->as.binaryop.lhs, state);
          state_add_ir(state, (ir_t){IR_OPERATION, {.inst = SUB}});

          int a = state->uli++;
          state_add_ir(state, (ir_t){IR_JMPZ, {.num = a}});
          state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = -2}});
          state_add_ir(state, (ir_t){IR_INT, {.num = ast->as.binaryop.op != T_EQ}});
          state_add_ir(state, (ir_t){IR_SETULI, {.num = a}});
        } break;
        case T_SHL:
        case T_SHR:
        {
          assert(ast->as.binaryop.rhs->kind == A_INT);
          compile(ast->as.binaryop.lhs, state);
          ir_t ir = {IR_OPERATION, {.inst = ast->as.binaryop.op == T_SHL ? SHL : SHR}};
          for (int i = 0; i < ast->as.binaryop.rhs->as.fac.asint; ++i) {
            state_add_ir(state, ir);
          }
        } break;
        default:
          printf("BINARYOP %s\n", token_kind_to_string(ast->as.binaryop.op));
          TODO;
      }
      break;
    case A_UNARYOP:
      switch (ast->as.unaryop.op) {
        case T_STAR:
          compile(ast->as.unaryop.arg, state);
          state_add_ir(state, (ir_t){IR_READ, {.num = ast->type.size}});
          break;
        case T_AND:
          get_addr_ast(state, ast->as.unaryop.arg);
          break;
        case T_NOT:
        {
          state_add_ir(state, (ir_t){IR_INT, {.num = 0}});
          compile(ast->as.unaryop.arg, state);
          int a = state->uli++;
          state_add_ir(state, (ir_t){IR_JMPZ, {.num = a}});
          state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = -2}});
          state_add_ir(state, (ir_t){IR_INT, {.num = 1}});
          state_add_ir(state, (ir_t){IR_SETULI, {.num = a}});
        } break;
        default:
          printf("UNARYOP: %s\n", token_kind_to_string(ast->as.unaryop.op));
          TODO;
      }
      break;
    case A_INT:
      state_add_ir(state, (ir_t){IR_INT, {.num = ast->as.fac.asint}});
      break;
    case A_SYM:
    {
      symbol_t *s = state_find_symbol(state, ast->as.fac);
      if (s->kind == INFO_CONSTANT) {
        state_add_ir(state, (ir_t){IR_INT, {.num = s->info.num}});
      } else {
        if (type_is_kind(&ast->type, TY_ARRAY)) {
          eprintf(ast->loc, "cannot access ARRAY, maybe wanna cast it to PTR");
        }
        get_addr_ast(state, ast);
        assert(s->type);
        state_add_ir(state, (ir_t){IR_READ, {.num = s->type->size}});
      }
    } break;
    case A_STRING:
    {
      int uli = datauli(state);
      compile_data(ast, state, 0, 0); // TODO: maybe compile_data(ast, state, uli, 0);
      state_add_ir(state, (ir_t){IR_ADDR_GLOBAL, {.loc = {uli, 0}}});
    } break;
    case A_DECL:
    {
      int size = type_size_aligned(&ast->as.decl.type);
      if (ast->as.decl.expr) {
        int start_sp = state->sp;
        compile(ast->as.decl.expr, state);
        assert(state->sp > start_sp);
        if (state->sp - start_sp > size) {
          state_add_ir(state, (ir_t){IR_ADDR_LOCAL, {.num = state->sp - start_sp - size}});
          state_add_ir(state, (ir_t){IR_WRITE, {.num = size}});
          state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = state->sp - start_sp - 2 * size}});
        }
      } else {
        state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = size}});
      }
      state_add_symbol(state, (symbol_t){ast->as.decl.name, &ast->as.decl.type, INFO_LOCAL, {state->sp - 2}});
    } break;
    case A_GLOBDECL:
    {
      int uli = datauli(state);
      if (ast->as.decl.expr) {
        compile_data(ast->as.decl.expr, state, uli, 0);
      } else {
        data(compiled, (bytecode_t){BDB, 0, {.num = ast->as.decl.type.size}});
      }
      state_add_symbol(state,
                       (symbol_t){ast->as.decl.name, &ast->as.decl.type, INFO_GLOBAL, {uli}});
    } break;
    case A_ASSIGN:
    {
      compile(ast->as.binary.right, state);
      get_addr_ast(state, ast->as.binary.left);
      int size = type_size_aligned(&ast->type);
      state_add_ir(state, (ir_t){IR_WRITE, {.num = size}});
    } break;
    case A_FUNCALL:
      if (ast->as.funcall.params) {
        compile(ast->as.funcall.params, state);
      }
      state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = type_size_aligned(&ast->type)}});
      state_add_ir(state, (ir_t){IR_CALL, {.sv = ast->as.funcall.name.image}});
      break;
    case A_ARRAY:
      assert(ast->type.as.array.len.kind != ARRAY_LEN_EXPR);
      if (type_is_kind(ast->type.as.array.type, TY_CHAR)) {
        if (ast->as.binary.right) {
          if (ast->as.binary.right->as.binary.right) {
            compile(ast->as.binary.right->as.binary.right, state);
          }
          compile(ast->as.binary.left, state);
          compile(ast->as.binary.right->as.binary.left, state);
          state_add_ir(state, (ir_t){IR_OPERATION, {.inst = B_AH}});
        } else {
          compile(ast->as.binary.left, state);
        }
        break;
      }
      if (ast->as.binary.right) {
        compile(ast->as.binary.right, state);
      }
      compile(ast->as.binary.left, state);
      break;
    case A_TYPEDEF:
      if (ast->as.typedef_.type.kind == TY_ENUM) {
        int i = 0;
        for (type_t *typei = &ast->as.typedef_.type; typei; typei = typei->as.enum_.next) {
          state_add_symbol(state, (symbol_t){typei->as.enum_.name, type_malloc((type_t){TY_INT, 2, {}}), INFO_CONSTANT, {.num = i}});
          i++;
        }
      }
      break;
    case A_CAST:
      if (ast->as.cast.ast->kind == A_ARRAY
          && ast->as.cast.ast->as.binary.left
          && ast->as.cast.ast->as.binary.left->kind == A_INT
          && ast->as.cast.ast->as.binary.left->as.fac.asint == 0
          && ast->as.cast.ast->as.binary.right == NULL) {
        for (int i = 0; i < type_size_aligned(&ast->as.cast.target); i += 2) {
          state_add_ir(state, (ir_t){IR_INT, {.num = 0}});
        }
      } else if (type_is_kind(&ast->as.cast.target, TY_STRUCT) && ast->as.cast.ast->kind == A_ARRAY) {
        ast_t *asts[128] = {0};
        int ast_num = 0;
        ast_t *asti = ast->as.cast.ast;
        while (asti) {
          assert(ast_num + 1 < 128);
          asts[ast_num++] = asti->as.binary.left;
          asti = asti->as.binary.right;
        }
        for (--ast_num; ast_num >= 0; --ast_num) {
          if (ast_num > 1 && type_is_kind(&asts[ast_num]->type, TY_CHAR)
              && type_is_kind(&asts[ast_num - 1]->type, TY_CHAR)) {
            compile(asts[ast_num - 1], state);
            compile(asts[ast_num], state);
            state_add_ir(state, (ir_t){IR_OPERATION, {.inst = B_AH}});
            --ast_num;
          } else {
            compile(asts[ast_num], state);
          }
        }
      } else if (type_is_kind(&ast->as.cast.target, TY_PTR) && type_is_kind(&ast->as.cast.ast->type, TY_ARRAY)) {
        get_addr_ast(state, ast->as.cast.ast);
      } else {
        int tsize = type_size_aligned(&ast->as.cast.target);
        int size = type_size_aligned(&ast->as.cast.ast->type);
        if (tsize - size < 0) {
          eprintf(ast->loc,
                  "cannot cast '%s' to smaller size type '%s' ",
                  type_dump_to_string(&ast->as.cast.ast->type),
                  type_dump_to_string(&ast->as.cast.target));
        }
        state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = tsize - size}});
        compile(ast->as.cast.ast, state);
      }
      break;
    case A_ASM:
      TODO;
      break;
    case A_IF:
      compile(ast->as.if_.cond, state);
      if (ast->as.if_.else_) {
        int a = state->uli++;
        int b = state->uli++;
        state_add_ir(state, (ir_t){IR_JMPZ, {.num = a}});
        if (ast->as.if_.then) {
          compile(ast->as.if_.then, state);
        }
        state_add_ir(state, (ir_t){IR_JMP, {.num = b}});
        state_add_ir(state, (ir_t){IR_SETULI, {.num = a}});
        compile(ast->as.if_.else_, state);
        state_add_ir(state, (ir_t){IR_SETULI, {.num = b}});
      } else if (ast->as.if_.then) {
        int a = state->uli++;
        state_add_ir(state, (ir_t){IR_JMPZ, {.num = a}});
        compile(ast->as.if_.then, state);
        state_add_ir(state, (ir_t){IR_SETULI, {.num = a}});
      }
      break;
    case A_WHILE:
    {
      int a = state->uli++;
      int b = state->uli++;
      state_push_break_target(state, b);
      state_add_ir(state, (ir_t){IR_SETULI, {.num = a}});
      if (ast->as.binary.left->kind == A_UNARYOP && ast->as.binary.left->as.unaryop.op == T_NOT) {
        compile(ast->as.binary.left->as.unaryop.arg, state);
        state_add_ir(state, (ir_t){IR_JMPNZ, {.num = b}});
      } else {
        compile(ast->as.binary.left, state);
        state_add_ir(state, (ir_t){IR_JMPZ, {.num = b}});
      }
      if (ast->as.binary.right) {
        compile(ast->as.binary.right, state);
      }
      state_add_ir(state, (ir_t){IR_JMP, {.num = a}});
      state_add_ir(state, (ir_t){IR_SETULI, {.num = b}});
      state_drop_break_target(state);
    } break;
    case A_EXTERN:
      assert(ast->as.ast->kind == A_FUNCDEF);
      state_add_ir(state, (ir_t){IR_EXTERN, {.sv = ast->as.ast->as.funcdef.name.image}});
      compile(ast->as.ast, state);
      break;
    case A_BREAK:
      if (state->break_target_num == 0) {
        eprintf(ast->loc, "break statement not within loop or switch");
      }
      break_target_info_t info = state->break_target[state->break_target_num - 1];
      state_add_ir(state, (ir_t){IR_CHANGE_SP, {.num = -(state->sp - info.sp)}});
      state_add_ir(state, (ir_t){IR_JMP, {.num = info.uli}});
      break;
  }
}

bool is_ir_kind(ir_t *irs, int *ir_count, int i, ir_kind_t kind) {
  assert(irs);
  return i < *ir_count && irs[i].kind == kind;
}

void optimize_ir(ir_t *irs, int *ir_count, bool debug_opt) {
  assert(irs);
  assert(ir_count);

  for (int i = 0; i < *ir_count; ++i) {
    if (is_ir_kind(irs, ir_count, i, IR_CHANGE_SP) && irs[i].arg.num == 0) {
      if (debug_opt) {
        printf("  %03d | CHANGE_SP(0) -> nothing\n", i);
      }

      *ir_count -= 1;
      memcpy(irs + i, irs + i + 1, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_CHANGE_SP)
               && is_ir_kind(irs, ir_count, i + 1, IR_CHANGE_SP)) {
      if (debug_opt) {
        printf("  %03d | CHANGE_SP(x) CHANGE_SP(y) -> CHANGE_SP(x+y)\n", i);
      }

      irs[i + 1].arg.num += irs[i].arg.num;
      *ir_count -= 1;
      memcpy(irs + i, irs + i + 1, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_ADDR_LOCAL)
               && is_ir_kind(irs, ir_count, i + 1, IR_READ)
               && is_ir_kind(irs, ir_count, i + 2, IR_CHANGE_SP)
               && irs[i + 1].arg.num <= -irs[i + 2].arg.num) {
      if (debug_opt) {
        printf("  %03d | ADDR_LOCAL READ(x) CHANGE_SP(y) if x <= -y -> CHANGE_SP(x+y)\n", i);
      }

      irs[i + 2].arg.num += irs[i + 1].arg.num;
      *ir_count -= 2;
      memcpy(irs + i, irs + i + 2, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_ADDR_LOCAL)
               && is_ir_kind(irs, ir_count, i + 1, IR_READ)
               && is_ir_kind(irs, ir_count, i + 2, IR_ADDR_LOCAL)
               && is_ir_kind(irs, ir_count, i + 3, IR_READ)
               && irs[i].arg.num - irs[i + 3].arg.num == irs[i + 2].arg.num - irs[i + 1].arg.num) {
      if (debug_opt) {
        printf("  %03d | ADDR_LOCAL(x+z) READ(y) ADDR_LOCAL(y+z) READ(z) -> ADDR_LOCAL(z) READ(x+y)\n", i);
      }

      irs[i + 2].arg.num = irs[i].arg.num - irs[i + 3].arg.num;
      irs[i + 3].arg.num += irs[i + 1].arg.num;
      *ir_count -= 2;
      memcpy(irs + i, irs + i + 2, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_INT) && is_ir_kind(irs, ir_count, i + 1, IR_INT)
               && is_ir_kind(irs, ir_count, i + 2, IR_OPERATION) && irs[i + 2].arg.inst == B_AH) {
      if (debug_opt) {
        printf("  %03d | INT(x) INT(y) OPERATION(B_AH) -> INT((x << 8) | y)\n", i);
      }

      irs[i].arg.num = (irs[i].arg.num << 8) | irs[i + 1].arg.num;
      *ir_count -= 2;
      memcpy(irs + i + 1, irs + i + 3, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_INT) && is_ir_kind(irs, ir_count, i + 1, IR_INT)
               && is_ir_kind(irs, ir_count, i + 2, IR_OPERATION)
               && (irs[i + 2].arg.inst == SUM || irs[i + 2].arg.inst == SUB)) {
      if (debug_opt) {
        printf("  %03d | INT(x) INT(y) OPERATION(SUM|SUB) -> INT(x+y | x-y)\n", i);
      }

      irs[i].arg.num += (irs[i + 2].arg.inst == SUB ? -1 : 1) * irs[i + 1].arg.num;
      *ir_count -= 2;
      memcpy(irs + i + 1, irs + i + 3, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_ADDR_LOCAL)
               && is_ir_kind(irs, ir_count, i + 1, IR_READ)
               && is_ir_kind(irs, ir_count, i + 2, IR_ADDR_LOCAL)
               && is_ir_kind(irs, ir_count, i + 3, IR_WRITE)
               && is_ir_kind(irs, ir_count, i + 4, IR_CHANGE_SP) && irs[i].arg.num == 2
               && irs[i + 1].arg.num == irs[i + 3].arg.num
               && -irs[i + 4].arg.num >= irs[i + 1].arg.num) {
      if (debug_opt) {
        printf("  %03d | ADDR_LOCAL(2) READ(x) ADDR_LOCAL(y) WRITE(x) CHANGE_SP(z) if -z>=x -> ADDR_LOCAL(y-x) WRITE(x) CHANGE_SP(z+x)\n", i);
      }

      irs[i + 2].arg.num -= irs[i + 1].arg.num;
      irs[i + 4].arg.num += irs[i + 1].arg.num;
      *ir_count -= 2;
      memcpy(irs + i, irs + i + 2, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_INT)
               && is_ir_kind(irs, ir_count, i + 1, IR_MUL)) {
      if (debug_opt) {
        printf("  %03d | INT(x) MUL(y) -> INT(x * y)\n", i);
      }

      irs[i].arg.num *= irs[i + 1].arg.num;
      *ir_count -= 1;
      memcpy(irs + i + 1, irs + i + 2, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    } else if (is_ir_kind(irs, ir_count, i, IR_ADDR_LOCAL)
               && is_ir_kind(irs, ir_count, i + 1, IR_INT)
               && is_ir_kind(irs, ir_count, i + 2, IR_OPERATION)
               && irs[i + 2].arg.inst == SUM) {
      if (debug_opt) {
        printf("  %03d | ADDR_LOCAL(x) INT(y) OPERATION(SUM) -> ADDR_LOCAL(x + y)\n", i);
      }

      irs[i].arg.num += irs[i + 1].arg.num;
      *ir_count -= 2;
      memcpy(irs + i + 1, irs + i + 3, (*ir_count - i) * sizeof(ir_t));
      i = 0;
    }
  }
}

void compile_change_sp(state_t *state, int delta) {
  assert(state);
  compiled_t *compiled = &state->compiled;
  if (delta == 0) {
    return;
  }

  assert(delta % 2 == 0);
  if (abs(delta) <= 10) {
    for (int i = 0; i < abs(delta); i += 2) {
      code(compiled, (bytecode_t){BINST, delta > 0 ? DECSP : INCSP, {}});
    }
  } else {
    code(compiled, (bytecode_t){BINST, SP_A, {}});
    if (delta > 0) {
      code(compiled, (bytecode_t){BINST, A_B, {}});
      code(compiled, (bytecode_t){BINSTHEX2, RAM_A, {.num = delta}});
      code(compiled, (bytecode_t){BINST, SUB, {}});
    } else {
      code(compiled, (bytecode_t){BINSTHEX2, RAM_B, {.num = -delta}});
      code(compiled, (bytecode_t){BINST, SUM, {}});
    }
    code(compiled, (bytecode_t){BINST, A_SP, {}});
  }
}

void compile_ir_list(state_t *state, ir_t *irs, int ir_count) {
  assert(state);
  assert(irs);
  compiled_t *compiled = &state->compiled;

  for (int iri = 0; iri < ir_count; ++iri) {
    ir_t ir = irs[iri];

    switch (ir.kind) {
      case IR_NONE:
        assert(0);
      case IR_SETLABEL:
        code(compiled, bytecode_with_sv(BSETLABEL, 0, ir.arg.sv));
        break;
      case IR_SETULI:
        code(compiled, bytecode_uli(BSETLABEL, 0, ir.arg.num));
        break;
      case IR_JMPZ:
      case IR_JMPNZ:
        code(compiled, (bytecode_t){BINST, POPA, {}});
        code(compiled, (bytecode_t){BINST, CMPA, {}});
        code(compiled, bytecode_uli(BINSTRELLABEL, ir.kind == IR_JMPZ ? JMPRZ : JMPRNZ, ir.arg.num));
        break;
      case IR_JMP:
        code(compiled, bytecode_uli(BINSTRELLABEL, JMPR, ir.arg.num));
        break;
      case IR_FUNCEND:
        code(compiled, (bytecode_t){BINST, RET, {}});
        break;
      case IR_ADDR_LOCAL:
        assert(ir.arg.loc.offset == 0);
        if (iri + 1 < ir_count && irs[iri + 1].kind == IR_READ && irs[iri + 1].arg.num > 1) {
          int size = irs[iri + 1].arg.num;
          assert(size % 2 == 0);
          assert(ir.arg.loc.base % 2 == 0);
          for (int i = 0; i < size; i += 2) {
            code(compiled, (bytecode_t){BINSTHEX, PEEKAR, {.num = ir.arg.loc.base + size - 2}});
            code(compiled, (bytecode_t){BINST, PUSHA, {}});
          }
          iri++;
        } else if (iri + 1 < ir_count && irs[iri + 1].kind == IR_WRITE && irs[iri + 1].arg.num > 1) {
          int size = irs[iri + 1].arg.num;
          assert(0 < size && size < 256);
          assert(ir.arg.loc.base % 2 == 0);
          for (int i = 0; i < size; i += 2) {
            code(compiled, (bytecode_t){BINST, POPA, {}});
            code(compiled, (bytecode_t){BINSTHEX, PUSHAR, {.num = ir.arg.loc.base}});
          }
          iri++;
        } else {
          code(compiled, (bytecode_t){BINST, SP_A, {}});
          code(compiled, (bytecode_t){BINSTHEX2, RAM_B, {.num = ir.arg.loc.base}});
          code(compiled, (bytecode_t){BINST, SUM, {}});
          code(compiled, (bytecode_t){BINST, PUSHA, {}});
        }
        break;
      case IR_ADDR_GLOBAL:
        code(compiled, bytecode_uli(BINSTLABEL, RAM_A, ir.arg.loc.base));
        if (ir.arg.loc.offset != 0) {
          code(compiled, (bytecode_t){BINSTHEX2, RAM_B, {.num = ir.arg.loc.offset}});
          code(compiled, (bytecode_t){BINST, SUM, {}});
        }
        code(compiled, (bytecode_t){BINST, PUSHA, {}});
        break;
      case IR_WRITE:
        code(compiled, (bytecode_t){BINST, POPB, {}});
        code(compiled, (bytecode_t){BINST, POPA, {}});
        if (ir.arg.num == 1) {
          code(compiled, (bytecode_t){BINST, AL_rB, {}});
        } else {
          assert(ir.arg.num % 2 == 0);
          code(compiled, (bytecode_t){BINST, A_rB, {}});
          for (int i = 2; i < ir.arg.num; i += 2) {
            code(compiled, (bytecode_t){BINSTHEX, RAM_AL, {.num = 2}});
            code(compiled, (bytecode_t){BINST, SUM, {}});
            code(compiled, (bytecode_t){BINST, A_B, {}});
            code(compiled, (bytecode_t){BINST, POPA, {}});
            code(compiled, (bytecode_t){BINST, A_rB, {}});
          }
        }
        break;
      case IR_READ:
        code(compiled, (bytecode_t){BINST, POPB, {}});
        if (ir.arg.num == 1) {
          code(compiled, (bytecode_t){BINST, rB_AL, {}});
        } else {
          assert(ir.arg.num % 2 == 0);
          if (ir.arg.num > 2) {
            code(compiled, (bytecode_t){BINSTHEX2, RAM_A, {.num = ir.arg.num - 2}});
            code(compiled, (bytecode_t){BINST, SUM, {}});
            code(compiled, (bytecode_t){BINST, A_B, {}});
          }
          code(compiled, (bytecode_t){BINST, rB_A, {}});
          for (int i = 2; i < ir.arg.num; i += 2) {
            code(compiled, (bytecode_t){BINST, PUSHA, {}});
            code(compiled, (bytecode_t){BINSTHEX, RAM_AL, {.num = 2}});
            code(compiled, (bytecode_t){BINST, SUB, {}});
            code(compiled, (bytecode_t){BINST, A_B, {}});
            code(compiled, (bytecode_t){BINST, rB_A, {}});
          }
        }
        code(compiled, (bytecode_t){BINST, PUSHA, {}});
        break;
      case IR_CHANGE_SP:
        compile_change_sp(state, ir.arg.num);
        break;
      case IR_INT:
      {
        int num = ir.arg.num;
        if (iri + 1 < ir_count && irs[iri + 1].kind == IR_MUL) {
          num *= irs[iri + 1].arg.num;
          ++iri;
        }
        // TODO: INTs in sequence
        // if (!(iri > 0 && irs[iri - 1].kind == IR_INT && irs[iri - 1].arg.num == num)) {
        if (0 <= num && num < 256) {
          code(compiled, (bytecode_t){BINSTHEX, RAM_AL, {.num = num}});
        } else {
          code(compiled, (bytecode_t){BINSTHEX2, RAM_A, {.num = num}});
        }
        //}
        code(compiled, (bytecode_t){BINST, PUSHA, {}});
      } break;
      case IR_STRING:
        code(compiled, bytecode_uli(BINSTLABEL, RAM_A, ir.arg.num));
        code(compiled, (bytecode_t){BINST, PUSHA, {}});
        break;
      case IR_OPERATION:
        switch (ir.arg.inst) {
          case SUM:
          case SUB:
          case B_AH:
            code(compiled, (bytecode_t){BINST, POPA, {}});
            code(compiled, (bytecode_t){BINST, POPB, {}});
            code(compiled, (bytecode_t){BINST, ir.arg.inst, {}});
            code(compiled, (bytecode_t){BINST, PUSHA, {}});
            break;
          case SHL:
          case SHR:
            code(compiled, (bytecode_t){BINST, POPA, {}});
            code(compiled, (bytecode_t){BINST, ir.arg.inst, {}});
            code(compiled, (bytecode_t){BINST, PUSHA, {}});
            break;
          default:
            printf("%s\n", instruction_to_string(ir.arg.inst));
            TODO;
        }
        break;
      case IR_MUL:
      case IR_DIV:
        if (ir.arg.num != 1) {
          code(compiled, (bytecode_t){BINST, POPA, {}});
          assert(ir.arg.num % 2 == 0);
          for (int i = 0; i < ir.arg.num; i += 2) {
            code(compiled, (bytecode_t){BINST, ir.kind == IR_MUL ? SHL : SHR, {}});
          }
          code(compiled, (bytecode_t){BINST, PUSHA, {}});
        }
        break;
      case IR_CALL:
        code(compiled, bytecode_with_sv(BINSTLABEL, CALL, ir.arg.sv));
        break;
      case IR_EXTERN:
        code(compiled, bytecode_with_sv(BEXTERN, 0, ir.arg.sv));
        break;
    }
  }
}

bool is_inst(bytecode_t *bs, int *b_count, int i, instruction_t inst) {
  assert(bs);
  return i >= 0 && i < *b_count && bs[i].inst == inst;
}

void optimize_asm(bytecode_t *bs, int *b_count, bool debug_opt) {
  assert(bs);
  assert(b_count);

  for (int i = 0; i < *b_count; ++i) {
    // if ((is_inst(bs, b_count, i, PEEKAR) || is_inst(bs, b_count, i, PEEKA))
    // && is_inst(bs, b_count, i + 1, A_B)) {
    //   bs[i].inst = PEEKB;
    //   memcpy(bs + i + 1, bs + i + 2, (b_count - i - 1) * sizeof(bs[0]));
    // }
    if (is_inst(bs, b_count, i, PEEKAR) && bs[i].arg.num == 2) {
      if (debug_opt) {
        printf("  %03d | PEEKAR 2 -> PEEKA\n", i);
      }

      bs[i] = (bytecode_t){BINST, PEEKA, {}};
    } else if (is_inst(bs, b_count, i, PUSHA) && is_inst(bs, b_count, i + 1, POPA)) {
      if (debug_opt) {
        printf("  %03d | PUSHA POPA -> nothing\n", i);
      }

      memcpy(bs + i, bs + i + 2, (*b_count - i - 2) * sizeof(bytecode_t));
      *b_count -= 2;
      i = 0;
    } else if (is_inst(bs, b_count, i, PUSHA) && is_inst(bs, b_count, i + 1, POPB)) {
      if (debug_opt) {
        printf("  %03d | PUHSA POPB -> A_B\n", i);
      }

      bs[i].inst = A_B;
      memcpy(bs + i + 1, bs + i + 2, (*b_count - i - 2) * sizeof(bs[0]));
      *b_count -= 1;
      i = 0;
    } else if ((is_inst(bs, b_count, i, RAM_A) || is_inst(bs, b_count, i, RAM_B))
               && bs[i].arg.num < 256) {
      if (debug_opt) {
        printf("  %03d | RAM_A x | RAM_B x if x<256 -> RAM_AL x | RAM_BL x\n", i);
      }

      bs[i].inst = bs[i].inst == RAM_A ? RAM_AL : RAM_BL;
      bs[i].kind = BINSTHEX;
    } else if ((is_inst(bs, b_count, i, SUM) || is_inst(bs, b_count, i, SUB))
               && is_inst(bs, b_count, i + 1, CMPA)) {
      if (debug_opt) {
        printf("  %03d | SUM|SUB CMPA -> SUM|SUB\n", i);
      }

      *b_count -= 1;
      memcpy(bs + i + 1, bs + i + 2, (*b_count - i) * sizeof(bytecode_t));
      i = 0;
    } else if (is_inst(bs, b_count, i, PUSHA)
               && (is_inst(bs, b_count, i + 1, RAM_A) || is_inst(bs, b_count, i + 1, RAM_AL)
                   || is_inst(bs, b_count, i + 1, PEEKAR))
               && is_inst(bs, b_count, i + 2, POPB)) {
      if (debug_opt) {
        printf("  %03d | PUSHA RAM_A|RAM_AL|PEEKAR(x) POPB -> A_B RAM_A|RAM_AL|PEEKAR(x-2)\n", i);
      }

      *b_count -= 1;
      bs[i] = (bytecode_t){BINST, A_B, {}};
      if (bs[i + 1].inst == PEEKAR) {
        bs[i + 1].arg.num -= 2;
      }
      memcpy(bs + i + 2, bs + i + 3, (*b_count - i) * sizeof(bytecode_t));
      i = 0;
    } else if ((is_inst(bs, b_count, i, RAM_A) || is_inst(bs, b_count, i, RAM_AL)) && is_inst(bs, b_count, i + 1, A_B)) {
      if (debug_opt) {
        printf("  %03d | RAM_A|RAM_AL A_B -> RAM_B|RAM_BL\n", i);
      }

      *b_count -= 1;
      bs[i].inst = bs[i].inst == RAM_A ? RAM_B : RAM_BL;
      memcpy(bs + i + 1, bs + i + 2, (*b_count - i) * sizeof(bytecode_t));
      i = 0;
    }
  }
}

void help(int errorcode) {
  fprintf(stderr,
          "Usage: simpleC [options] [files]\n\n"
          "Options:\n"
          " -d <module> | opt    enable debug options for a module or optimizers\n"
          " -D <module>          stop the execution after a module and print the output\n"
          "                          if module 'all' then it will execute only the tokenizer\n"
          " -e <string>          compile the string provided\n"
          " -o <file>            write output to the file [default is 'out.asm']\n"
          " -O0 | -O             no optimization\n"
          " -O1                  enable ASM bytecode optimization\n"
          " -O2                  enable IR optimization (and ASM)\n"
          " -O3                  enable AST optimization (and ASM and IR)\n"
          " -O4                  enable all optimization\n"
          " --dev                print the source code loc where the error is thrown\n"
          " -h | --help          print this page and exit\n\n"
          "Modules:\n"
          "no module name is enables all the modules\n"
          "  all           all the modules\n"
          "  tok           tokenizer\n"
          "  par           parser\n"
          "  typ           typechecker\n"
          "  ir            ir\n"
          "  com           compiler to assembly\n");
  exit(errorcode);
}

typedef enum {
  M_TOK,
  M_PAR,
  M_TYP,
  M_IR,
  M_COM,
  M_COUNT,
} module_t;

typedef enum {
  OL_NONE,
  OL_ASM,
  OL_IR,
  OL_AST,
  OL_COUNT,
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
  } else if (strcmp(str, "ir") == 0) {
    return 1 << M_IR;
  } else if (strcmp(str, "com") == 0) {
    return 1 << M_COM;
  } else {
    fprintf(stderr, "ERROR: unknown module '%s'\n", str);
    help(1);
  }
  assert(0);
}

typedef struct {
  enum {
    INPUT_FILE,
    INPUT_STRING,
  } kind;
  char *str;
} input_t;

#define INPUTS_MAX 128

int main(int argc, char **argv) {
  (void)argc;
  assert(atexit(free_all) == 0);

  char *output = NULL;
  input_t inputs[INPUTS_MAX] = {0};
  int input_num = 0;

  uint8_t opt = 0;
  uint8_t debug_opt = 0;
  assert(M_COUNT < 8);
  uint8_t debug = 0;
  uint8_t exitat = 0;

  char *arg = NULL;
  ++argv;
  while (*argv) {
    arg = *argv;
    if (arg[0] == '-') {
      switch (arg[1]) {
        case 'd':
          ++argv;
          if (!*argv) {
            fprintf(stderr, "ERROR: -d expects a module name or 'opt'\n");
            help(1);
          }
          if (strcmp(*argv, "opt") == 0) {
            debug_opt = 1;
          } else {
            debug |= parse_module(*argv);
          }
          ++argv;
          break;
        case 'D':
          if (!*(argv + 1)) {
            debug = (1 << M_COUNT) - 1;
            exitat = (1 << M_COUNT) - 1;
          } else {
            ++argv;
            debug |= parse_module(*argv);
            exitat |= parse_module(*argv);
          }
          ++argv;
          break;
        case 'e':
          assert(input_num + 1 < INPUTS_MAX);
          ++argv;
          if (!*argv) {
            fprintf(stderr, "ERROR: -e expects a string\n");
            help(1);
          }
          inputs[input_num++] = (input_t){INPUT_STRING, *argv};
          ++argv;
          break;
        case 'o':
          ++argv;
          if (!*argv) {
            fprintf(stderr, "ERROR: -o expects a string\n");
            help(1);
          }
          output = *argv;
          ++argv;
          break;
        case 'O':
          opt = atoi(arg + 2);
          if (opt > OL_COUNT) {
            fprintf(stderr, "ERROR: invalid optimize level: %d, max: %d\n", opt, OL_COUNT);
            help(1);
          }
          ++argv;
          break;
        case 'h':
          help(0);
          break;
        case '-':
          if (strcmp(arg + 2, "help") == 0) {
            help(0);
          } else if (strcmp(arg + 2, "dev") == 0) {
            dev_flag = true;
            ++argv;
            break;
          }
          __attribute__((fallthrough));
        default:
          fprintf(stderr, "ERROR: unknown flag '%s'\n", arg);
          help(1);
      }
    } else {
      assert(input_num + 1 < INPUTS_MAX);
      inputs[input_num++] = (input_t){INPUT_FILE, *argv};
      ++argv;
    }
  }

  tokenizer_t tokenizer = {0};
  state_t state;
  state_init(&state);
  ast_t *ast;
  for (int i = 0; i < input_num; ++i) {
    char *buffer = NULL;
    char *name = NULL;
    switch (inputs[i].kind) {
      case INPUT_FILE:
      {
        name = inputs[i].str;
        FILE *file = fopen(inputs[i].str, "r");
        if (!file) {
          fprintf(stderr, "error: cannot open file '%s': %s", inputs[i].str, strerror(errno));
          exit(1);
        }
        fseek(file, 0, SEEK_END);
        int size = ftell(file);
        fseek(file, 0, SEEK_SET);
        buffer = alloc(size + 1);
        fread(buffer, 1, size, file);
        buffer[size] = 0;
        assert(fclose(file) == 0);
      } break;
      case INPUT_STRING:
        name = "cmd";
        buffer = inputs[i].str;
        break;
    };

    tokenizer_init(&tokenizer, buffer, name);
    if ((debug >> M_TOK) & 1) {
      printf("TOKENS:\n");
      token_t token;
      while ((token = token_next(&tokenizer)).kind != T_NONE) {
        token_dump(token);
      }
      tokenizer_init(&tokenizer, buffer, name);
    }
    if ((exitat >> M_TOK) & 1) {
      exit(0);
    }

    ast = parse(&tokenizer);
    if ((debug >> M_PAR) & 1) {
      printf("AST:\n");
      ast_dump_tree(ast, false, 0);
      printf("\n");
    }
    if ((exitat >> M_PAR) & 1) {
      exit(0);
    }

    state_init(&state);
    typecheck(ast, &state);
    if (opt >= OL_AST) {
      if (debug_opt) {
        printf("OPTIMIZE AST:\n");
      }
      optimize_ast(&ast, debug_opt);
    }
    if ((debug >> M_TYP) & 1) {
      printf("TYPED AST:\n");
      ast_dump_tree(ast, true, 0);
      printf("\n");
    }
    if ((exitat >> M_TYP) & 1) {
      exit(0);
    }

    // check if main
    scope_t *scope = &state.scopes[0];
    symbol_t *main_symbol = NULL;
    for (int i = 0; i < scope->symbol_num; ++i) {
      if (sv_eq(scope->symbols[i].name.image, sv_from_cstr("main"))) {
        main_symbol = &scope->symbols[i];
        break;
      }
    }
    if (!main_symbol) {
      fprintf(stderr, "ERROR: no main function found\n");
      exit(1);
    }
    if (main_symbol->type->kind != TY_FUNC || main_symbol->type->as.func.ret->kind != TY_INT) {
      fprintf(stderr, "ERROR: expected main to be FUNC with return type INT\n");
      exit(1);
    }

    state_init_with_compiled(&state);
    compile(ast, &state);

    if (opt >= OL_IR) {
      if (debug_opt) {
        printf("OPTIMIZE IR:\n");
      }
      optimize_ir(state.irs_init, &state.ir_init_num, debug_opt);
      optimize_ir(state.irs, &state.ir_num, debug_opt);
    }
    if ((debug >> M_IR) & 1) {
      printf("IR INIT:\n");
      for (int i = 0; i < state.ir_init_num; i++) {
        printf("\t");
        ir_dump(state.irs_init[i]);
      }
      printf("IR:\n");
      for (int i = 0; i < state.ir_num; i++) {
        printf("\t");
        ir_dump(state.irs[i]);
      }
      printf("\n");
    }
    if ((exitat >> M_IR) & 1) {
      exit(0);
    }

    state.compiled.is_init = true;
    compile_ir_list(&state, state.irs_init, state.ir_init_num);
    state.compiled.is_init = false;
    compile_ir_list(&state, state.irs, state.ir_num);

    state.compiled.is_init = true;
    code(&state.compiled, (bytecode_t){BINSTHEX, RAM_AL, {.num = 0}});
    code(&state.compiled, (bytecode_t){BINST, PUSHA, {}});
    code(&state.compiled, bytecode_with_string(BINSTRELLABEL, CALLR, "main"));
    code(&state.compiled, (bytecode_t){BINST, POPA, {}});
    code(&state.compiled, bytecode_with_string(BINSTLABEL, CALL, "exit"));
    state.compiled.is_init = false;

    if (opt >= OL_ASM) {
      if (debug_opt) {
        printf("OPTIMIZE ASM:\n");
      }
      optimize_asm(state.compiled.code, &state.compiled.code_num, debug_opt);
      optimize_asm(state.compiled.init, &state.compiled.init_num, debug_opt);
    }
    if ((debug >> M_COM) & 1) {
      printf("ASSEMBLY:\n");
      dump_code(&state.compiled);
    }
    if ((exitat >> M_COM) & 1) {
      exit(0);
    }

    if (inputs[i].kind == INPUT_FILE) {
      free_ptr(buffer);
    }
  }

  if (!output) {
    output = "out.asm";
  }

  FILE *file = fopen(output, "w");
  if (!file) {
    fprintf(stderr, "cannot open file '%s': '%s'", output, strerror(errno));
    exit(1);
  }

  for (int i = 0; i < state.compiled.data_num; ++i) {
    bytecode_to_file(file, state.compiled.data[i]);
    putc(' ', file);
  }
  putc('\n', file);
  for (int i = 0; i < state.compiled.init_num; ++i) {
    bytecode_to_file(file, state.compiled.init[i]);
    putc(' ', file);
  }
  putc('\n', file);
  for (int i = 0; i < state.compiled.code_num; ++i) {
    bytecode_to_file(file, state.compiled.code[i]);
    putc(' ', file);
  }

  /*
  for (int i = 0; i < state.compiled.data_num; ++i) {
    bytecode_t bc = state.compiled.data[i];
    bytecode_kind_t bbckind = i > 0 ? state.compiled.data[i - 1].kind : BNONE;
    bytecode_kind_t abckind =
        i + 1 < state.compiled.data_num ? state.compiled.data[i + 1].kind : BNONE;
    if ((bc.kind == BSETLABEL && bbckind != BALIGN) || (bc.kind == BALIGN && abckind == BSETLABEL)
        || (i > 0 && (bc.kind == BEXTERN || bc.kind == BGLOBAL))) {
      putc('\n', file);
    }
    bytecode_to_asm(file, bc);
    putc(' ', file);
  }
  putc('\n', file);
  for (int i = 0; i < state.compiled.init_num; ++i) {
    bytecode_to_asm(file, state.compiled.init[i]);
    putc(' ', file);
  }
  for (int i = 0; i < state.compiled.code_num; ++i) {
    if (state.compiled.code[i].kind == BSETLABEL) {
      putc('\n', file);
    }
    bytecode_to_asm(file, state.compiled.code[i]);
    putc(' ', file);
  }
  */

  assert(fclose(file) == 0);

  return 0;
}
