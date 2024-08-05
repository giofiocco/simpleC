#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../defs.h"

#define TODO assert(0 && "TODO")

typedef struct {
  char *start;
  unsigned int len;
} sv_t;

#define SV_FMT            "%*.*s"
#define SV_UNPACK(__sv__) (__sv__).len, (__sv__).len, (__sv__).start

bool sv_eq(sv_t a, sv_t b) {
  if (a.len != b.len) {
    return false;
  }
  for (unsigned int i = 0; i < a.len; ++i) {
    if (a.start[i] != b.start[i]) {
      return false;
    }
  }
  return true;
}

sv_t sv_from_cstr(char *str) {
  return (sv_t){str, strlen(str)};
}

sv_t sv_union(sv_t a, sv_t b) {
  return (sv_t){a.start, b.start + b.len - a.start};
}

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
  T_COLON,
  T_DOT,
  T_VOIDKW,
  T_INTKW,
  T_CHARKW,
} token_kind_t;

typedef struct {
  char *filename;
  char *row_start;
  unsigned int row, col;
  unsigned int len;
} location_t;

location_t location_union(location_t a, location_t b) {
  location_t c = a;
  c.len = b.col + b.len - a.col;
  return c;
}

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
  tokenizer->loc = (location_t){
    filename, buffer, 1, 1, 0,
  };
}

void print_location(location_t location) {
  char *row_end = location.row_start;
  while (*row_end != '\n' && *row_end != '\0') {
    ++row_end;
  }
  int row_len = row_end - location.row_start;
  fprintf(stderr, "%*d | %*.*s\n", location.row < 1000 ? 3 : 5, location.row, row_len, row_len, location.row_start);
  fprintf(stderr, "%s   %*.*s%c%*.*s\n", location.row < 1000 ? "   " : "     ", location.col - 1, location.col - 1,
          "                                                                    "
          "                                                                    "
          "                                                                ",
          '^', location.len - 1, location.len - 1,
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

static bool catch = false;
static jmp_buf catch_buf;
#define eprintf(__loc, ...) eprintf_impl((__loc), __LINE__, __FUNCTION__, __VA_ARGS__)
void eprintf_impl(location_t location, int line, const char *func, char *fmt, ...) {
  if (catch) {
    catch = false;
    longjmp(catch_buf, 1);
  }
  if (true) {
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

token_t token_next(tokenizer_t *tokenizer) {
  assert(tokenizer);

  if (tokenizer->has_last_token) {
    tokenizer->has_last_token = false;
    return tokenizer->last_token;
  }

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
  table['='] = T_EQUAL;
  table['&'] = T_AND;
  table[','] = T_COLON;
  table['.'] = T_DOT;

  token_t token = {0};

  switch (*tokenizer->buffer) {
    case '\0': break;
    case ' ':
    case '\t':
    case '\r':
      ++tokenizer->buffer;
      ++tokenizer->loc.col;
      return token_next(tokenizer);
    case '\n':
      ++tokenizer->loc.row;
      tokenizer->loc.col = 1;
      ++tokenizer->buffer;
      tokenizer->loc.row_start = tokenizer->buffer;
      return token_next(tokenizer);
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
    case '=':
    case '&':
    case ',':
    case ';':
    case '.':
      assert(table[(int)*tokenizer->buffer]);
      tokenizer->loc.len = 1;
      token = (token_t){
        table[(int)*tokenizer->buffer],
        {tokenizer->buffer, 1},
        tokenizer->loc
      };
      ++tokenizer->buffer;
      ++tokenizer->loc.col;
      break;
    case '"':
      {
        char *start = tokenizer->buffer;
        do {
          ++tokenizer->buffer;
        } while (*tokenizer->buffer != '"');
        ++tokenizer->buffer;
        sv_t str = {start, tokenizer->buffer - start};
        tokenizer->loc.len = str.len;
        token = (token_t){T_STRING, str, tokenizer->loc};
        tokenizer->loc.col += str.len;
      }
      break;
    case '0':
      if (*(tokenizer->buffer + 1) == 'x' || *(tokenizer->buffer + 1) == 'X') {
        char *image_start = tokenizer->buffer;
        tokenizer->buffer += 2;
        while (isdigit(*tokenizer->buffer) || ('a' <= *tokenizer->buffer && *tokenizer->buffer <= 'f') ||
               ('A' <= *tokenizer->buffer && *tokenizer->buffer <= 'F')) {
          ++tokenizer->buffer;
        }
        tokenizer->loc.len = tokenizer->buffer - image_start;
        token = (token_t){
          T_HEX,
          {image_start, tokenizer->loc.len},
          tokenizer->loc
        };
        if (token.image.len - 2 != 2 && token.image.len - 2 != 4) {
          eprintf(token.loc, "HEX can be 1 or 2 bytes\n");
        }
        tokenizer->loc.col += token.image.len;
        break;
      }
      __attribute__((fallthrough));
    default:
      {
        if (isalpha(*tokenizer->buffer) || isdigit(*tokenizer->buffer) || *tokenizer->buffer == '_') {
          char *image_start = tokenizer->buffer;
          bool is_int = isdigit(*tokenizer->buffer) ? true : false;
          int len = 0;
          do {
            if (!isdigit(*tokenizer->buffer)) {
              is_int = false;
            }
            ++len;
            ++tokenizer->buffer;
          } while (isalpha(*tokenizer->buffer) || isdigit(*tokenizer->buffer) || *tokenizer->buffer == '_');
          sv_t image = {image_start, len};
          tokenizer->loc.len = len;
          token = (token_t){is_int                                  ? T_INT
                            : sv_eq(image, sv_from_cstr("return"))  ? T_RETURN
                            : sv_eq(image, sv_from_cstr("typedef")) ? T_TYPEDEF
                            : sv_eq(image, sv_from_cstr("struct"))  ? T_STRUCT
                            : sv_eq(image, sv_from_cstr("int"))     ? T_INTKW
                            : sv_eq(image, sv_from_cstr("char"))    ? T_CHARKW
                            : sv_eq(image, sv_from_cstr("void"))    ? T_VOIDKW
                                                                    : T_SYM,
                            image, tokenizer->loc};
          if (!is_int && isdigit(image_start[0])) {
            eprintf(token.loc, "SYM cannot start with digit");
          }
          tokenizer->loc.col += len;
        } else {
          eprintf(tokenizer->loc, "unknown char: '%c'", *tokenizer->buffer);
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

char *token_kind_to_string(token_kind_t kind) {
  switch (kind) {
    case T_NONE:      return "NONE";
    case T_SYM:       return "SYM";
    case T_INT:       return "INT";
    case T_HEX:       return "HEX";
    case T_STRING:    return "STRING";
    case T_PARO:      return "PARO";
    case T_PARC:      return "PARC";
    case T_SQO:       return "SQO";
    case T_SQC:       return "SQC";
    case T_BRO:       return "BRO";
    case T_BRC:       return "BRC";
    case T_RETURN:    return "RETURN";
    case T_TYPEDEF:   return "TYPEDEF";
    case T_STRUCT:    return "STRUCT";
    case T_SEMICOLON: return "SEMICOLON";
    case T_PLUS:      return "PLUS";
    case T_MINUS:     return "MINUS";
    case T_STAR:      return "STAR";
    case T_SLASH:     return "SLASH";
    case T_EQUAL:     return "EQUAL";
    case T_AND:       return "AND";
    case T_COLON:     return "COLON";
    case T_DOT:       return "DOT";
    case T_VOIDKW:    return "VOIDKW";
    case T_INTKW:     return "INTKW";
    case T_CHARKW:    return "CHARKW";
  }
  assert(0);
}

void token_dump(token_t token) {
  printf("%s '" SV_FMT "' @ %d:%d\n", token_kind_to_string(token.kind), SV_UNPACK(token.image), token.loc.row,
         token.loc.col);
}

token_t token_expect(tokenizer_t *tokenizer, token_kind_t kind) {
  assert(tokenizer);
  token_t token = token_next(tokenizer);
  if (token.kind != kind) {
    eprintf(token.loc, "expected '%s' found '%s'", token_kind_to_string(kind), token_kind_to_string(token.kind));
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

typedef struct type_t_ {
  enum {
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
  } kind;
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
    struct type_t_ *ptr;
  } as;
} type_t;

char *array_len_dump_to_string(array_len_t array_len) {
  char *str = alloc(20);
  switch (array_len.kind) {
    case ARRAY_LEN_NOTARRAY: break;
    case ARRAY_LEN_UNSET:    snprintf(str, 20, "[]"); break;
    case ARRAY_LEN_NUM:      snprintf(str, 20, "[%d]", array_len.num); break;
    case ARRAY_LEN_EXPR:     snprintf(str, 20, "[...]"); break;
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
    case TY_VOID: strcpy(string, "VOID"); break;
    case TY_CHAR: strcpy(string, "CHAR"); break;
    case TY_INT:  strcpy(string, "INT"); break;
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
      }
      break;
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
      }
      break;
    case TY_ARRAY:
      {
        assert(type->as.array.type);
        char *str = type_dump_to_string(type->as.array.type);
        char *arr = array_len_dump_to_string(type->as.array.len);
        snprintf(string, 128, "%s%s", str, arr);
        free_ptr(str);
        free_ptr(arr);
      }
      break;
    case TY_ALIAS:
      snprintf(string, 128, "ALIAS {" SV_FMT " %s%s}", SV_UNPACK(type->as.alias.name.image),
               type->as.alias.type ? "..." : "NULL", type->as.alias.is_struct ? " struct" : "");
      break;
    case TY_FIELDLIST:
      {
        assert(type->as.fieldlist.type);
        char *str = type_dump_to_string(type->as.fieldlist.type);
        if (type->as.fieldlist.next) {
          char *str2 = type_dump_to_string(type->as.fieldlist.next);
          snprintf(string, 128, "FIELDLIST {%s " SV_FMT " %s}", str, SV_UNPACK(type->as.fieldlist.name.image), str2);
          free_ptr(str2);
        } else {
          snprintf(string, 128, "FIELDLIST {%s " SV_FMT "}", str, SV_UNPACK(type->as.fieldlist.name.image));
        }
        free_ptr(str);
      }
      break;
    case TY_STRUCT:
      if (type->as.struct_.fieldlist) {
        char *str = type_dump_to_string(type->as.struct_.fieldlist);
        if (type->as.struct_.name.kind == T_NONE) {
          snprintf(string, 128, "STRUCT {%s}", str);
        } else {
          snprintf(string, 128, "STRUCT {" SV_FMT " %s}", SV_UNPACK(type->as.struct_.name.image), str);
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
  }

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
      case TY_INT:  return true;
      case TY_FUNC:
        assert(a->as.func.ret);
        if ((a->as.func.params ? 1 : 0) ^ (b->as.func.params ? 1 : 0)) {
          return false;
        }
        return type_cmp(a->as.func.ret, b->as.func.ret) &&
               (a->as.func.params ? type_cmp(a->as.func.params, b->as.func.params) : true);
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
        return type_cmp(a->as.list.type, b->as.list.type) &&
               (a->as.list.next ? type_cmp(a->as.list.next, b->as.list.next) : true);
      case TY_ARRAY:
        return type_cmp(a->as.array.type, b->as.array.type) && a->as.array.len.kind == b->as.array.len.kind;
      case TY_ALIAS: assert(0);
      case TY_STRUCT:
        return (a->as.struct_.name.kind != T_NONE && b->as.struct_.name.kind != T_NONE &&
                sv_eq(a->as.struct_.name.image, b->as.struct_.name.image)) ||
               type_cmp(a->as.struct_.fieldlist, b->as.struct_.fieldlist);
      case TY_FIELDLIST:
        return type_cmp(a->as.fieldlist.type, b->as.fieldlist.type) &&
               sv_eq(a->as.fieldlist.name.image, b->as.fieldlist.name.image) &&
               type_cmp(a->as.fieldlist.next, b->as.fieldlist.next);
    }
  }
  return false;
}

bool type_greaterthan(type_t *a, type_t *b) {  // a >= b
  assert(a);
  assert(b);

  if (a->kind == TY_INT && b->kind == TY_CHAR) {
    return true;
  } else if (a->kind == TY_ARRAY && b->kind == TY_ARRAY && type_cmp(a->as.array.type, b->as.array.type)) {
    assert(b->as.array.len.kind == ARRAY_LEN_NUM);
    if (a->as.array.len.kind == ARRAY_LEN_NUM) {
      return a->as.array.len.num >= b->as.array.len.num;
    } else {
      return a->as.array.len.kind == ARRAY_LEN_UNSET;
    }
  } else if (a->kind == TY_PTR && b->kind == TY_ARRAY) {
    return type_cmp(a->as.ptr, b->as.array.type);
  }
  return type_cmp(a, b);
}

int type_size(type_t *type) {
  assert(type);
  switch (type->kind) {
    case TY_NONE:  assert(0);
    case TY_VOID:  return 0;
    case TY_CHAR:  return 1;
    case TY_INT:   return 2;
    case TY_FUNC:  assert(0);
    case TY_PTR:   return 2;
    case TY_PARAM: assert(0);
    case TY_ARRAY:
      assert(type->as.array.type);
      assert(type->as.array.len.kind == ARRAY_LEN_NUM);
      return type_size(type->as.array.type) * type->as.array.len.num;
    case TY_ALIAS:  assert(type->as.alias.type); return type_size(type->as.alias.type);
    case TY_STRUCT: assert(type->as.struct_.fieldlist); return type_size(type->as.struct_.fieldlist);
    case TY_FIELDLIST:
      assert(type->as.fieldlist.type);
      if (type->as.fieldlist.type->kind == TY_CHAR && type->as.fieldlist.next &&
          type->as.fieldlist.next->as.fieldlist.type->kind != TY_CHAR) {
        return 2 + type_size(type->as.fieldlist.next);
      } else {
        return type_size(type->as.fieldlist.type) + (type->as.fieldlist.next ? type_size(type->as.fieldlist.next) : 0);
      }
  }
  assert(0);
}

int type_size_aligned(type_t *type) {
  int size = type_size(type);
  return size + size % 2;
}

void type_expect(location_t loc, type_t *found, type_t *expect) {
  if (type_cmp(found, expect)) {
    return;
  }
  char *expectstr = type_dump_to_string(expect);
  char *foundstr = type_dump_to_string(found);
  eprintf(loc, "expected '%s', found '%s'", expectstr, foundstr);
}

typedef enum {
  A_NONE,
  A_GLOBAL,
  A_FUNCDECL,
  A_PARAMDEF,
  A_BLOCK,
  A_STATEMENT,
  A_RETURN,
  A_BINARYOP,
  A_UNARYOP,
  A_FAC,
  A_DECL,
  A_GLOBDECL,
  A_ASSIGN,
  A_FUNCALL,
  A_PARAM,
  A_ARRAY,
  A_INDEX,
  A_TYPEDEF,
  A_CAST,
} ast_kind_t;

typedef struct ast_t_ {
  ast_kind_t kind;
  location_t forerror;
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
      struct ast_t_ *dest;
      struct ast_t_ *expr;
    } assign;
    struct {
      token_t name;
      struct ast_t_ *params;
    } funcall;
    struct {
      struct ast_t_ *expr;
      struct ast_t_ *num;
    } index;
    struct {
      type_t type;
      token_t name;
    } typedef_;
    struct {
      type_t target;
      struct ast_t_ *ast;
    } cast;
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

void ast_dump(ast_t *ast, bool dumptype) {
  if (!ast) {
    printf("NULL");
    return;
  }

  switch (ast->kind) {
    case A_NONE: assert(0);
    case A_GLOBAL:
      printf("GLOBAL(");
      ast_dump(ast->as.astlist.ast, dumptype);
      printf(" ");
      ast_dump(ast->as.astlist.next, dumptype);
      printf(")");
      break;
    case A_FUNCDECL:
      {
        char *str = type_dump_to_string(&ast->as.funcdecl.type);
        printf("FUNCDECL(%s " SV_FMT " ", str, SV_UNPACK(ast->as.funcdecl.name.image));
        free_ptr(str);
        ast_dump(ast->as.funcdecl.params, dumptype);
        printf(" ");
        ast_dump(ast->as.funcdecl.block, dumptype);
        printf(")");
      }
      break;
    case A_PARAMDEF:
      {
        char *str = type_dump_to_string(&ast->as.paramdef.type);
        printf("PARAMDEF(%s " SV_FMT " ", str, SV_UNPACK(ast->as.paramdef.name.image));
        free_ptr(str);
        ast_dump(ast->as.paramdef.next, dumptype);
        printf(")");
      }
      break;
    case A_BLOCK:
      printf("BLOCK(");
      ast_dump(ast->as.astlist.ast, dumptype);
      printf(" ");
      ast_dump(ast->as.astlist.next, dumptype);
      printf(")");
      break;
    case A_STATEMENT:
      printf("STATEMENT(");
      ast_dump(ast->as.ast, dumptype);
      printf(")");
      break;
    case A_RETURN:
      printf("RETURN(");
      ast_dump(ast->as.return_.expr, dumptype);
      printf(")");
      break;
    case A_BINARYOP:
      printf("BINARYOP(%s ", token_kind_to_string(ast->as.binaryop.op));
      ast_dump(ast->as.binaryop.lhs, dumptype);
      printf(" ");
      ast_dump(ast->as.binaryop.rhs, dumptype);
      printf(")");
      break;
    case A_UNARYOP:
      printf("UNARYOP(%s", token_kind_to_string(ast->as.unaryop.op));
      ast_dump(ast->as.unaryop.arg, dumptype);
      printf(")");
      break;
    case A_FAC:      printf("FAC(" SV_FMT ")", SV_UNPACK(ast->as.fac.image)); break;
    case A_GLOBDECL: printf("GLOB"); __attribute__((fallthrough));
    case A_DECL:
      {
        char *str = type_dump_to_string(&ast->as.decl.type);
        printf("DECL(%s " SV_FMT " ", str, SV_UNPACK(ast->as.decl.name.image));
        free_ptr(str);
        ast_dump(ast->as.decl.expr, dumptype);
        printf(")");
      }
      break;
    case A_ASSIGN:
      printf("ASSIGN(");
      ast_dump(ast->as.assign.dest, dumptype);
      printf(" ");
      ast_dump(ast->as.assign.expr, dumptype);
      printf(")");
      break;
    case A_FUNCALL:
      printf("FUNCALL(" SV_FMT " ", SV_UNPACK(ast->as.funcall.name.image));
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
    case A_ARRAY:
      printf("ARRAY(");
      ast_dump(ast->as.astlist.ast, dumptype);
      printf(" ");
      ast_dump(ast->as.astlist.next, dumptype);
      printf(")");
      break;
    case A_INDEX:
      printf("INDEX(");
      ast_dump(ast->as.index.expr, dumptype);
      printf(" ");
      ast_dump(ast->as.index.num, dumptype);
      printf(")");
      break;
    case A_TYPEDEF:
      {
        char *str = type_dump_to_string(&ast->as.typedef_.type);
        printf("TYPEDEF(%s " SV_FMT ")", str, SV_UNPACK(ast->as.typedef_.name.image));
        free_ptr(str);
      }
      break;
    case A_CAST:
      {
        char *str = type_dump_to_string(&ast->as.cast.target);
        printf("CAST(%s ", str);
        ast_dump(ast->as.cast.ast, dumptype);
        free_ptr(str);
      }
      break;
  }
  if (dumptype) {
    char *str = type_dump_to_string(&ast->type);
    printf(" {%s}", str);
    free_ptr(str);
  }
}

#define LABEL_MAX 128

typedef struct {
  enum {
    B_INST,
    B_HEX,
    B_HEX2,
    B_SETLABEL,
    B_LABEL,
    B_RELLABEL,
    B_STRING,
    B_DB,
  } kind;
  struct {
    instruction_t inst;
    int num;
    char str[LABEL_MAX];
  } arg;
} bytecode_t;

void bytecode_dump(bytecode_t b, FILE *stream) {
  switch (b.kind) {
    case B_INST:     fprintf(stream, "%s ", instruction_to_string(b.arg.inst)); break;
    case B_HEX:      fprintf(stream, "0x%02X ", b.arg.num); break;
    case B_HEX2:     fprintf(stream, "0x%04X ", b.arg.num); break;
    case B_SETLABEL: fprintf(stream, "\n%s: ", b.arg.str); break;
    case B_LABEL:    fprintf(stream, "%s ", b.arg.str); break;
    case B_RELLABEL: fprintf(stream, "$%s ", b.arg.str); break;
    case B_STRING:   fprintf(stream, "%s ", b.arg.str); break;
    case B_DB:       fprintf(stream, "db %d ", b.arg.num); break;
  }
}

#define ALIAS_MAX  128
#define SYMBOL_MAX 128
#define SCOPE_MAX  32
#define DATA_MAX   128
#define CODE_MAX   128

typedef struct {
  token_t name;
  type_t *type;
  enum {
    INFO_NONE,
    INFO_LOCAL,
    INFO_GLOBAL,
    INFO_TYPE,
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
  int uli;  // unique label id
  compiled_t *compiled;
} state_t;

void state_init(state_t *state) {
  assert(state);
  *state = (state_t){0};
  ++state->scope_num;
}

void state_push_scope(state_t *state) {
  assert(state);
  assert(state->scope_num + 1 < SCOPE_MAX);
  memset(&state->scopes[state->scope_num], 0, sizeof(scope_t));
  ++state->scope_num;
}

void state_drop_scope(state_t *state) {
  assert(state);
  assert(state->scope_num > 0);
  --state->scope_num;
}

symbol_t *state_find_symbol(state_t *state, token_t token) {
  assert(state);
  for (int j = state->scope_num > 0 ? state->scope_num - 1 : 0; j >= 0; --j) {
    scope_t *scope = &state->scopes[j];
    for (int i = 0; i < scope->symbol_num; ++i) {
      if (sv_eq(scope->symbols[i].name.image, token.image)) {
        return &scope->symbols[i];
      }
    }
  }
  eprintf(token.loc, "symbol not declared: " SV_FMT, SV_UNPACK(token.image));
  assert(0);
}

void state_add_symbol(state_t *state, symbol_t symbol) {
  assert(state);

  catch = true;
  if (setjmp(catch_buf) == 0) {
    symbol_t *s = state_find_symbol(state, symbol.name);
    eprintf(symbol.name.loc, "redefinition of symbol '" SV_FMT "', defined at %d:%d", SV_UNPACK(symbol.name.image),
            s->name.loc.row, s->name.loc.col);
    catch = false;
  }

  scope_t *scope = &state->scopes[state->scope_num - 1];
  assert(scope->symbol_num + 1 < SYMBOL_MAX);
  scope->symbols[scope->symbol_num++] = symbol;
}

void state_solve_type_alias(state_t *state, type_t *type) {
  assert(state);
  assert(type);

  switch (type->kind) {
    case TY_NONE: assert(0);
    case TY_VOID:
    case TY_CHAR:
    case TY_INT:  break;
    case TY_FUNC: assert(0);
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
      break;
    case TY_ALIAS:
      {
        symbol_t *s = state_find_symbol(state, type->as.alias.name);
        if (s->kind != INFO_TYPE) {
          eprintf(type->as.alias.name.loc, "expected to be a type");
        }
        assert(s->type);
        type->as.alias.type = s->type;
      }
      break;
    case TY_STRUCT:
      if (type->as.struct_.fieldlist) {
        state_solve_type_alias(state, type->as.struct_.fieldlist);
      }
      break;
    case TY_FIELDLIST:
      assert(type->as.fieldlist.type);
      state_solve_type_alias(state, type->as.fieldlist.type);
      if (type->as.fieldlist.next) {
        state_solve_type_alias(state, type->as.fieldlist.next);
      }
      break;
  }
}

void code_dump(compiled_t *compiled, FILE *stream) {
  assert(compiled);

  fprintf(stream, "GLOBAL _start");
  for (int i = 0; i < compiled->data_num; ++i) {
    bytecode_dump(compiled->data[i], stream);
  }
  fprintf(stream, "\n_start: ");
  for (int i = 0; i < compiled->init_num; ++i) {
    bytecode_dump(compiled->init[i], stream);
  }
  fprintf(stream, "\n\tDECSP CALLR $main POPA RET\n");
  for (int i = 0; i < compiled->code_num; ++i) {
    bytecode_dump(compiled->code[i], stream);
  }
  fprintf(stream, "\n");
}

type_t parse_type(tokenizer_t *tokenizer);
type_t parse_structdef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_expect(tokenizer, T_STRUCT);
  token_t name = {0};
  if (token_peek(tokenizer).kind == T_SYM) {
    name = token_next(tokenizer);
  }
  token_expect(tokenizer, T_BRO);
  if (token_next_if_kind(tokenizer, T_BRC)) {
    return (type_t){TY_STRUCT, {.struct_ = {name, NULL}}};
  }

  sv_t fields[128] = {0};
  int field_num = 0;

  type_t ftype = parse_type(tokenizer);
  token_t fname = token_expect(tokenizer, T_SYM);
  token_expect(tokenizer, T_SEMICOLON);
  type_t type = {TY_FIELDLIST, {.fieldlist = {type_malloc(ftype), fname, NULL}}};
  type_t *typei = &type;

  fields[field_num++] = fname.image;

  catch = false;

  if (name.kind != T_NONE && ftype.kind == TY_ALIAS && ftype.as.alias.is_struct &&
      sv_eq(ftype.as.alias.name.image, name.image)) {
    eprintf(fname.loc, "uncomplete type, maybe wanna use 'struct " SV_FMT " *" SV_FMT "'", SV_UNPACK(name.image),
            SV_UNPACK(fname.image));
  }

  while (!token_next_if_kind(tokenizer, T_BRC)) {
    ftype = parse_type(tokenizer);
    fname = token_expect(tokenizer, T_SYM);
    token_expect(tokenizer, T_SEMICOLON);
    typei->as.fieldlist.next = type_malloc((type_t){TY_FIELDLIST, {.fieldlist = {type_malloc(ftype), fname, NULL}}});
    typei = typei->as.fieldlist.next;

    for (int i = 0; i < field_num; ++i) {
      if (sv_eq(fields[i], fname.image)) {
        eprintf(fname.loc, "redefinition of field");
      }
    }
    assert(field_num < 128);
    fields[field_num++] = fname.image;

    if (name.kind != T_NONE && ftype.kind == TY_ALIAS && ftype.as.alias.is_struct &&
        sv_eq(ftype.as.alias.name.image, name.image)) {
      eprintf(fname.loc, "uncomplete type, maybe wanna use 'struct " SV_FMT " *" SV_FMT "'", SV_UNPACK(name.image),
              SV_UNPACK(fname.image));
    }
  }

  return (type_t){TY_STRUCT, {.struct_ = {name, type_malloc(type)}}};
}

type_t parse_type(tokenizer_t *tokenizer) {
  assert(tokenizer);

  bool is_struct = token_next_if_kind(tokenizer, T_STRUCT);

  type_t type = {0};

  token_t token = token_peek(tokenizer);
  if (token_next_if_kind(tokenizer, T_VOIDKW)) {
    type = (type_t){TY_VOID, {}};
  } else if (token_next_if_kind(tokenizer, T_INTKW)) {
    type = (type_t){TY_INT, {}};
  } else if (token_next_if_kind(tokenizer, T_CHARKW)) {
    type = (type_t){TY_CHAR, {}};
  } else {
    token = token_expect(tokenizer, T_SYM);
    type = (type_t){TY_ALIAS, {.alias = {token, NULL, is_struct}}};
  }

  if (is_struct && type.kind != TY_ALIAS) {
    eprintf(token.loc, "invalid struct type");  // TODO better error
  }

  if (token_next_if_kind(tokenizer, T_STAR)) {
    type = (type_t){TY_PTR, {.ptr = type_malloc(type)}};
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
  ast_t *ast = ast_malloc((ast_t){A_PARAM, expr->forerror, {0}, {.astlist = {expr, NULL}}});
  ast_t *asti = ast;

  while (token_next_if_kind(tokenizer, T_COLON)) {
    expr = parse_expr(tokenizer);
    asti->as.astlist.next = ast_malloc((ast_t){A_PARAM, expr->forerror, {0}, {.astlist = {expr, NULL}}});
    asti = asti->as.astlist.next;
  }

  token_expect(tokenizer, T_PARC);

  return ast;
}

ast_t *parse_funcall(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *param = parse_param(tokenizer);

  return ast_malloc(
    (ast_t){A_FUNCALL, param ? location_union(name.loc, param->forerror) : name.loc, {0}, {.funcall = {name, param}}});
}

ast_t *parse_array(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t bro = token_expect(tokenizer, T_BRO);
  if (token_peek(tokenizer).kind == T_BRC) {
    token_t brc = token_expect(tokenizer, T_BRC);
    catch = false;
    eprintf(location_union(bro.loc, brc.loc), "unvalid empty array");
  }

  ast_t *expr = parse_expr(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_ARRAY, expr->forerror, {0}, {.astlist = {expr, NULL}}});
  ast_t *asti = ast;
  while (token_next_if_kind(tokenizer, T_COLON)) {
    expr = parse_expr(tokenizer);
    asti->as.astlist.next = ast_malloc((ast_t){A_ARRAY, expr->forerror, {0}, {.astlist = {expr, NULL}}});
    asti = asti->as.astlist.next;
  }
  token_t brc = token_expect(tokenizer, T_BRC);
  ast->forerror = location_union(bro.loc, brc.loc);
  return ast;
}

ast_t *parse_fac(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);

  tokenizer_t savetok = *tokenizer;

  if (token.kind == T_PARO) {
    token_next(tokenizer);
    ast_t *expr = parse_expr(tokenizer);
    token_expect(tokenizer, T_PARC);
    return expr;
  };

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

  if (token.kind != T_INT && token.kind != T_SYM && token.kind != T_STRING && token.kind != T_HEX) {
    eprintf(token.loc, "unvalid token for fac: '%s'", token_kind_to_string(token.kind));
  }
  token_next(tokenizer);

  return ast_malloc((ast_t){A_FAC, token.loc, {0}, {.fac = token}});
}

ast_t *parse_access(tokenizer_t *tokenizer) {
  assert(tokenizer);

  ast_t *fac = parse_fac(tokenizer);

  if (token_peek(tokenizer).kind == T_DOT) {
    token_t dot = token_next(tokenizer);
    token_t name = token_expect(tokenizer, T_SYM);

    return ast_malloc((ast_t){A_BINARYOP,
                              location_union(fac->forerror, name.loc),
                              {0},
                              {.binaryop = {dot.kind, fac, ast_malloc((ast_t){A_FAC, name.loc, {0}, {.fac = name}})}}});
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
      ast_t *arg = parse_access(tokenizer);
      return ast_malloc(
        (ast_t){A_UNARYOP, location_union(token.loc, arg->forerror), {0}, {.unaryop = {token.kind, arg}}});
    default: break;
  }

  ast_t *ast = parse_access(tokenizer);

  if (token_next_if_kind(tokenizer, T_SQO)) {
    ast_t *expr = parse_expr(tokenizer);
    token_t sqc = token_expect(tokenizer, T_SQC);
    ast = ast_malloc((ast_t){A_INDEX, location_union(ast->forerror, sqc.loc), {}, {.index = {ast, expr}}});
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
    a =
      ast_malloc((ast_t){A_BINARYOP, location_union(a->forerror, b->forerror), {0}, {.binaryop = {token.kind, a, b}}});
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
    a =
      ast_malloc((ast_t){A_BINARYOP, location_union(a->forerror, b->forerror), {0}, {.binaryop = {token.kind, a, b}}});
  }

  return a;
}

ast_t *parse_decl(tokenizer_t *tokenizer) {
  assert(tokenizer);
  token_t start = token_peek(tokenizer);
  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *array_len_expr = NULL;
  array_len_t array_len = {ARRAY_LEN_NOTARRAY};
  if (token_next_if_kind(tokenizer, T_SQO)) {
    if (token_next_if_kind(tokenizer, T_SQC)) {
      array_len = (array_len_t){ARRAY_LEN_UNSET, 0};
    } else {
      array_len_expr = parse_expr(tokenizer);
      token_expect(tokenizer, T_SQC);
      array_len = (array_len_t){ARRAY_LEN_EXPR, 0};
      if (array_len_expr->kind == A_FAC && array_len_expr->as.fac.kind == T_INT) {
        array_len = (array_len_t){ARRAY_LEN_NUM, atoi(array_len_expr->as.fac.image.start)};
        array_len_expr = NULL;
      }
    }
  }
  ast_t *expr = NULL;
  if (token_next_if_kind(tokenizer, T_EQUAL)) {
    expr = parse_expr(tokenizer);
  }
  token_expect(tokenizer, T_SEMICOLON);
  if (array_len.kind != ARRAY_LEN_NOTARRAY) {
    type = (type_t){TY_ARRAY, {.array = {type_malloc(type), array_len}}};
  }
  if (expr && array_len.kind != ARRAY_LEN_UNSET) {
    expr = ast_malloc((ast_t){A_CAST, expr->forerror, {0}, {.cast = {type, expr}}});
  }
  return ast_malloc((ast_t){A_DECL,
                            location_union(start.loc, expr ? expr->forerror : name.loc),
                            {0},
                            {.decl = {type, name, expr, array_len_expr}}});
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
    // TODO: better forerror
    return ast_malloc((ast_t){A_RETURN, token.loc, {0}, {.return_ = {expr}}});
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
    if (token.kind == T_STAR) {
      dest = parse_expr(tokenizer);
    } else {
      dest = parse_access(tokenizer);
    }
    assert(dest);
    token_expect(tokenizer, T_EQUAL);
    ast_t *expr = parse_expr(tokenizer);
    token = token_expect(tokenizer, T_SEMICOLON);
    ast_t *ast =
      ast_malloc((ast_t){A_ASSIGN, location_union(dest->forerror, token.loc), {0}, {.assign = {dest, expr}}});
    catch = false;
    return ast;
  }
  *tokenizer = savetok;

  catch = true;
  if (setjmp(catch_buf) == 0) {
    ast_t *ast = parse_expr(tokenizer);
    token_expect(tokenizer, T_SEMICOLON);
    catch = false;
    return ast_malloc((ast_t){A_STATEMENT, ast->forerror, {0}, {.ast = ast}});
  }

  eprintf(token.loc, "expected a statement");
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

  ast_t *code = parse_code(tokenizer);
  ast_t *ast = ast_malloc((ast_t){A_BLOCK, location_union(token.loc, code->forerror), {0}, {.astlist = {code, NULL}}});
  ast_t *block = ast;

  while (token_peek(tokenizer).kind != T_BRC) {
    code = parse_code(tokenizer);
    if (code) {
      block->as.astlist.next =
        ast_malloc((ast_t){A_BLOCK, location_union(ast->forerror, code->forerror), {0}, {.astlist = {code, NULL}}});
      block = block->as.astlist.next;
    }
  }

  token_expect(tokenizer, T_BRC);

  return ast;
}

ast_t *parse_paramdef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t par = token_expect(tokenizer, T_PARO);
  if (token_next_if_kind(tokenizer, T_PARC)) {
    return NULL;
  }

  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);
  ast_t *ast =
    ast_malloc((ast_t){A_PARAMDEF, location_union(par.loc, name.loc), {0}, {.paramdef = {type, name, NULL}}});
  ast_t *asti = ast;

  while (token_next_if_kind(tokenizer, T_COLON)) {
    type = parse_type(tokenizer);
    name = token_expect(tokenizer, T_SYM);
    asti->as.paramdef.next =
      ast_malloc((ast_t){A_PARAMDEF, location_union(ast->forerror, name.loc), {0}, {.paramdef = {type, name, NULL}}});
    asti = asti->as.paramdef.next;
  }

  token_expect(tokenizer, T_PARC);

  return ast;
}

ast_t *parse_funcdecl(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t token = token_peek(tokenizer);
  type_t type = parse_type(tokenizer);
  token_t name = token_expect(tokenizer, T_SYM);

  ast_t *param = parse_paramdef(tokenizer);

  ast_t *block = parse_block(tokenizer);

  return ast_malloc((ast_t){A_FUNCDECL,
                            location_union(token.loc, block ? block->forerror : (param ? param->forerror : name.loc)),
                            {0},
                            {.funcdecl = {type, name, param, block}}});
}

ast_t *parse_typedef(tokenizer_t *tokenizer) {
  assert(tokenizer);

  token_t td = token_expect(tokenizer, T_TYPEDEF);

  type_t type = {0};

  tokenizer_t savetok = *tokenizer;
  catch = true;
  if (setjmp(catch_buf) == 0) {
    type = parse_structdef(tokenizer);
    catch = false;
  } else {
    *tokenizer = savetok;
    type = parse_type(tokenizer);
  }

  token_t name = token_expect(tokenizer, T_SYM);
  token_expect(tokenizer, T_SEMICOLON);

  return ast_malloc((ast_t){A_TYPEDEF, td.loc, {0}, {.typedef_ = {type, name}}});
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

  catch = true;
  if (setjmp(catch_buf) == 0) {
    ast_t *ast = parse_typedef(tokenizer);
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

  if (ast->type.kind == TY_NONE) {
    typecheck(ast, state);
  }

  type_expect(ast->forerror, &ast->type, &type);
}

void typecheck_expandable(ast_t *ast, state_t *state, type_t type) {
  assert(ast);

  if (ast->type.kind == TY_NONE) {
    typecheck(ast, state);
  }

  if (type_greaterthan(&type, &ast->type)) {
    return;
  }

  type_expect(ast->forerror, &ast->type, &type);
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

  ast->type = (type_t){TY_VOID, {}};
}

void typecheck(ast_t *ast, state_t *state) {
  assert(state);

  if (!ast) {
    return;
  }

  switch (ast->kind) {
    case A_NONE: assert(0);
    case A_GLOBAL:
    case A_BLOCK:
      assert(ast->as.astlist.ast);
      typecheck(ast->as.astlist.ast, state);
      typecheck(ast->as.astlist.next, state);
      ast->type = (type_t){TY_VOID, {}};
      break;
    case A_STATEMENT:
      assert(ast->as.ast);
      typecheck(ast->as.ast, state);
      ast->type = (type_t){TY_VOID, {}};
      break;
    case A_FUNCDECL:
      {
        state_add_symbol(state, (symbol_t){ast->as.funcdecl.name, &ast->type, 0, {}});
        state_push_scope(state);
        state->param = 0;
        typecheck(ast->as.funcdecl.params, state);
        state_solve_type_alias(state, &ast->as.funcdecl.type);
        ast->type =
          (type_t){TY_FUNC,
                   {.func = {&ast->as.funcdecl.type, ast->as.funcdecl.params ? &ast->as.funcdecl.params->type : NULL}}};

        if (ast->as.funcdecl.block) {
          typecheck_funcbody(ast->as.funcdecl.block, state, ast->as.funcdecl.type);
        }
        state_drop_scope(state);
      }
      break;
    case A_PARAMDEF:
      {
        scope_t *scope = &state->scopes[state->scope_num - 1];
        assert(scope->symbol_num + 1 < SYMBOL_MAX);
        ++state->param;
        state_solve_type_alias(state, &ast->as.paramdef.type);
        scope->symbols[scope->symbol_num++] =
          (symbol_t){ast->as.paramdef.name, &ast->as.paramdef.type, INFO_LOCAL, {-1 - state->param}};

        typecheck(ast->as.paramdef.next, state);
        ast->type =
          (type_t){TY_PARAM,
                   {.list = {&ast->as.paramdef.type, ast->as.paramdef.next ? &ast->as.paramdef.next->type : NULL}}};
      }
      break;
    case A_RETURN:
      typecheck(ast->as.return_.expr, state);
      if (ast->as.return_.expr) {
        ast->type = ast->as.return_.expr->type;
      } else {
        ast->type = (type_t){TY_VOID, {}};
      }
      break;
    case A_BINARYOP:
      assert(ast->as.binaryop.lhs);
      assert(ast->as.binaryop.rhs);
      typecheck(ast->as.binaryop.lhs, state);
      if (ast->as.binaryop.op == T_DOT) {
        assert(ast->as.binaryop.rhs->kind == A_FAC);
        assert(ast->as.binaryop.rhs->as.fac.kind == T_SYM);

        type_t *type = &ast->as.binaryop.lhs->type;
        if (!(type->kind == TY_ALIAS && type->as.alias.type && type->as.alias.type->kind == TY_STRUCT)) {
          eprintf(ast->as.binaryop.lhs->forerror, "expected an 'ALIAS STRUCT', found '%s'", type_dump_to_string(type));
        }

        type_t *f = type->as.alias.type->as.struct_.fieldlist;
        while (!sv_eq(f->as.fieldlist.name.image, ast->as.binaryop.rhs->as.fac.image)) {
          f = f->as.fieldlist.next;
        };
        if (!f) {
          eprintf(ast->as.binaryop.rhs->forerror, "member not found in '%s'", type_dump_to_string(type->as.alias.type));
        }

        ast->type = *f->as.fieldlist.type;
      } else {
        if (ast->as.binaryop.lhs->type.kind != TY_INT && ast->as.binaryop.lhs->type.kind != TY_PTR) {
          char *typestr = type_dump_to_string(&ast->as.binaryop.lhs->type);
          eprintf(ast->forerror, "expected 'INT' or 'PTR ...' found '%s'", typestr);
        }
        typecheck_expandable(ast->as.binaryop.rhs, state, (type_t){TY_INT, {{0}}});
        ast->type = ast->as.binaryop.lhs->type;
      }
      break;
    case A_UNARYOP:
      assert(ast->as.unaryop.arg);
      switch (ast->as.unaryop.op) {
        case T_MINUS:
          typecheck_expandable(ast->as.unaryop.arg, state, (type_t){TY_INT, {{0}}});
          ast->type.kind = TY_INT;
          break;
        case T_AND:
          typecheck(ast->as.unaryop.arg, state);
          ast->type = (type_t){TY_PTR, {.ptr = &ast->as.unaryop.arg->type}};
          break;
        case T_STAR:
          typecheck(ast->as.unaryop.arg, state);
          if (ast->as.unaryop.arg->type.kind != TY_PTR) {
            char *type = type_dump_to_string(&ast->as.unaryop.arg->type);
            eprintf(ast->as.unaryop.arg->forerror, "cannot dereference non PTR type: '%s'", type);
          }
          ast->type = *ast->as.unaryop.arg->type.as.ptr;
          break;
        default: assert(0);
      }
      break;
    case A_FAC:
      if (ast->as.fac.kind == T_INT) {
        ast->type.kind = TY_INT;
      } else if (ast->as.fac.kind == T_SYM) {
        symbol_t *s = state_find_symbol(state, ast->as.fac);
        ast->type = *s->type;
      } else if (ast->as.fac.kind == T_STRING) {
        ast->type = (type_t){TY_PTR, {.ptr = type_malloc((type_t){TY_CHAR, {}})}};
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
    case A_DECL:
      state_solve_type_alias(state, &ast->as.decl.type);
      if (ast->as.decl.type.kind == TY_ARRAY && ast->as.decl.type.as.array.len.kind == ARRAY_LEN_UNSET &&
          ast->as.decl.expr == NULL) {
        eprintf(ast->forerror, "array without length uninitialized");
      }
      if (ast->as.decl.type.kind == TY_ARRAY && ast->as.decl.type.as.array.len.kind == ARRAY_LEN_EXPR) {
        if (ast->as.decl.expr != NULL) {
          eprintf(ast->forerror, "array with variable length cannot be initialized");
        } else {
          typecheck_expect(ast->as.decl.array_len, state, (type_t){TY_INT, {}});
        }
      }
      if (ast->as.decl.expr) {
        typecheck_expandable(ast->as.decl.expr, state, ast->as.decl.type);
        if (ast->as.decl.type.kind == TY_ARRAY && ast->as.decl.type.as.array.len.kind == ARRAY_LEN_UNSET) {
          typecheck(ast->as.decl.expr, state);
          ast->as.decl.type = ast->as.decl.expr->type;
        }
        ast->as.decl.expr->type = ast->as.decl.type;
      }
      state_add_symbol(state, (symbol_t){ast->as.decl.name, &ast->as.decl.type, 0, {}});
      ast->type = (type_t){TY_VOID, {}};
      break;
    case A_ASSIGN:
      assert(ast->as.assign.dest);
      assert(ast->as.assign.expr);
      typecheck(ast->as.assign.dest, state);
      if (ast->as.assign.dest->type.kind == TY_ARRAY) {
        eprintf(ast->forerror, "assign to array");
      }
      typecheck_expect(ast->as.assign.expr, state, ast->as.assign.dest->type);
      ast->type = ast->as.assign.dest->type;
      break;
    case A_FUNCALL:
      {
        symbol_t *s = state_find_symbol(state, ast->as.funcall.name);
        if (s->type->kind != TY_FUNC) {
          char *foundstr = type_dump_to_string(s->type);
          eprintf(ast->forerror, "expected 'TY_FUNC', found '%s'", foundstr);
        }
        if (s->type->as.func.params == NULL && ast->as.funcall.params != NULL) {
          eprintf(ast->forerror, "too many parameters");
        }
        if (s->type->as.func.params != NULL && ast->as.funcall.params == NULL) {
          eprintf(ast->forerror, "too few parameters");
        }
        if (s->type->as.func.params) {
          typecheck_expect(ast->as.funcall.params, state, *s->type->as.func.params);
        }
        assert(s->type->as.func.ret);
        ast->type = *s->type->as.func.ret;
      }
      break;
    case A_PARAM:
      assert(ast->as.astlist.ast);
      typecheck(ast->as.astlist.ast, state);
      typecheck(ast->as.astlist.next, state);
      ast->type =
        (type_t){TY_PARAM,
                 {.list = {&ast->as.astlist.ast->type, ast->as.astlist.next ? &ast->as.astlist.next->type : NULL}}};
      break;
    case A_ARRAY:
      assert(ast->as.astlist.ast);
      if (ast->as.astlist.next) {
        typecheck(ast->as.astlist.next, state);
        typecheck_expandable(ast->as.astlist.ast, state, *ast->as.astlist.next->type.as.array.type);
        type_t type = ast->as.astlist.next->type;
        type.as.array.len.num += 1;
        ast->type = type;
      } else {
        typecheck(ast->as.astlist.ast, state);
        ast->type = (type_t){TY_ARRAY, {.array = {&ast->as.astlist.ast->type, .len = {ARRAY_LEN_NUM, 1}}}};
      }
      break;
    case A_INDEX:
      {
        typecheck(ast->as.index.expr, state);
        type_t *type = &ast->as.index.expr->type;
        if (type->kind == TY_ARRAY) {
          assert(ast->as.index.num);
          typecheck_expect(ast->as.index.num, state, (type_t){TY_INT, {}});
          ast->type = *type->as.array.type;
        } else if (type->kind == TY_PTR) {
          assert(type->as.ptr);
          ast->type = *type->as.ptr;
        } else {
          eprintf(ast->forerror, "expected an ARRAY or a PTR, found: '%s'", type_dump_to_string(type));
        }
      }
      break;
    case A_TYPEDEF:
      {
        type_t *type = &ast->as.typedef_.type;
        if (type->kind == TY_STRUCT && type->as.struct_.name.kind != T_NONE) {
          state_add_symbol(state, (symbol_t){type->as.struct_.name, type, 0, {}});
        }
        state_solve_type_alias(state, type);
        state_add_symbol(state, (symbol_t){ast->as.typedef_.name, &ast->as.typedef_.type, INFO_TYPE, {}});
        ast->type = (type_t){TY_VOID, {}};
      }
      break;
    case A_CAST:
      state_solve_type_alias(state, &ast->as.cast.target);

      if (ast->as.cast.target.kind == TY_ALIAS) {
        type_t *target = ast->as.cast.target.as.alias.type;
        assert(target);
        if (target->kind == TY_STRUCT && ast->as.cast.ast->kind == A_ARRAY) {
          type_t *f = target->as.struct_.fieldlist;
          ast_t *expr = ast->as.cast.ast;

          while (f && expr) {
            typecheck_expect(expr->as.astlist.ast, state, *f->as.fieldlist.type);
            expr->type = *target;
            expr->type.as.struct_.fieldlist = f;

            expr = expr->as.astlist.next;
            f = f->as.fieldlist.next;
          }

          if (f != NULL) {
            eprintf(ast->forerror, "too few fields");
          }
          if (expr != NULL) {
            eprintf(ast->forerror, "too many fields");
          }
        }
      } else {
        typecheck_expandable(ast->as.cast.ast, state, ast->as.cast.target);
      }
      ast->type = ast->as.cast.target;
      break;
  }
}

void data(compiled_t *compiled, bytecode_t b) {
  assert(compiled);
  assert(compiled->data_num + 1 < DATA_MAX);
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

// wants in B the addr
void coderead(state_t *state, type_t *type) {
  assert(state);
  assert(type);
  if (type->kind == TY_ALIAS) {
    coderead(state, type->as.alias.type);
    return;
  }

  compiled_t *compiled = state->compiled;

  int size = type_size(type);
  switch (size) {
    case 1:
      code(compiled, (bytecode_t){B_INST, {.inst = rB_AL}});
      code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
      state->sp += 2;
      break;
    case 2:
      code(compiled, (bytecode_t){B_INST, {.inst = rB_A}});
      code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
      break;
    default:
      assert(type->kind == TY_STRUCT);
      size += size % 2;
      size -= 2;
      code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
      code(compiled, (bytecode_t){B_HEX2, {.num = size}});
      code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
      code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
      for (; size > 0; size -= 2) {
        code(compiled, (bytecode_t){B_INST, {.inst = rB_A}});
        code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
        code(compiled, (bytecode_t){B_HEX, {.num = 2}});
        code(compiled, (bytecode_t){B_INST, {.inst = SUB}});
        code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
        state->sp += 2;
      }
      code(compiled, (bytecode_t){B_INST, {.inst = rB_A}});
      code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
      state->sp += 2;
  }
}

// wants in B the addr
void codewrite(compiled_t *compiled, type_t *type) {
  assert(compiled);
  assert(type);

  switch (type_size(type)) {
    case 1:  code(compiled, (bytecode_t){B_INST, {.inst = AL_rB}}); break;
    case 2:  code(compiled, (bytecode_t){B_INST, {.inst = A_rB}}); break;
    default: assert(0);
  }
}

void datanum(compiled_t *compiled, type_t *type, int num) {
  assert(compiled);
  assert(type);
  int size = type_size(type);
  switch (size) {
    case 1:  data(compiled, (bytecode_t){B_HEX, {.num = num}}); break;
    case 2:  data(compiled, (bytecode_t){B_HEX2, {.num = num}}); break;
    default: data(compiled, (bytecode_t){B_DB, {.num = size}});
  }
}

bytecode_t datauli(compiled_t *compiled, state_t *state) {
  assert(compiled);
  assert(state);
  bytecode_t b = {B_SETLABEL, {0}};
  sprintf(b.arg.str, "_%03d", state->uli);
  ++state->uli;
  data(compiled, b);
  return b;
}

void state_change_sp(state_t *state, int delta) {
  assert(state);
  assert(delta % 2 == 0);
  state->sp += delta;
  if (delta == 0) {
    return;
  }
  if (abs(delta) <= 6) {
    for (int i = 0; i < abs(delta) / 2; ++i) {
      if (delta < 0) {
        code(state->compiled, (bytecode_t){B_INST, {.inst = INCSP}});
      } else {
        code(state->compiled, (bytecode_t){B_INST, {.inst = DECSP}});
      }
    }
  } else {
    code(state->compiled, (bytecode_t){B_INST, {.inst = SP_A}});
    code(state->compiled, (bytecode_t){B_INST, {.inst = A_B}});
    code(state->compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
    code(state->compiled, (bytecode_t){B_HEX2, {.num = abs(delta)}});
    code(state->compiled, (bytecode_t){B_INST, {.inst = delta > 0 ? SUB : SUM}});
    code(state->compiled, (bytecode_t){B_INST, {.inst = A_SP}});
  }
}

// addr in A
void get_addr(state_t *state, token_t token) {
  assert(state);
  assert(token.kind == T_SYM);

  compiled_t *compiled = state->compiled;
  symbol_t *s = state_find_symbol(state, token);
  switch (s->kind) {
    case INFO_NONE: assert(0);
    case INFO_LOCAL:
      {
        int num = state->sp - s->info.local;
        code(compiled, (bytecode_t){B_INST, {.inst = SP_A}});
        code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
        code(compiled, (bytecode_t){B_HEX2, {.num = abs(num)}});
        code(compiled, (bytecode_t){B_INST, {.inst = num > 0 ? SUM : SUB}});
      }
      break;
    case INFO_GLOBAL:
      {
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
        bytecode_t b = {B_LABEL, {0}};
        sprintf(b.arg.str, "_%03d", s->info.global);
        code(compiled, b);
      }
      break;
    case INFO_TYPE: assert(0);
  }
}

void compile(ast_t *ast, state_t *state);
// addr in A
void get_addr_ast(state_t *state, ast_t *ast) {
  assert(state);
  assert(ast);

  compiled_t *compiled = state->compiled;

  switch (ast->kind) {
    case A_FAC: get_addr(state, ast->as.fac); break;
    case A_BINARYOP:
      {
        assert(ast->as.binaryop.op == T_DOT);
        get_addr_ast(state, ast->as.binaryop.lhs);
        unsigned int offset = 0;
        type_t *f = ast->as.binaryop.lhs->type.as.alias.type->as.struct_.fieldlist;
        while (!sv_eq(f->as.fieldlist.name.image, ast->as.binaryop.rhs->as.fac.image)) {
          if (f->as.fieldlist.type->kind == TY_CHAR && f->as.fieldlist.next &&
              f->as.fieldlist.next->as.fieldlist.type->kind != TY_CHAR) {
            offset += 2;
          } else {
            offset += type_size(f->as.fieldlist.type);
          }
          f = f->as.fieldlist.next;
        };
        assert(f);

        assert(offset < 256);

        if (offset == 1) {
          code(compiled, (bytecode_t){B_INST, {.inst = INCA}});
        } else if (offset > 1) {
          code(compiled, (bytecode_t){B_INST, {.inst = RAM_BL}});
          code(compiled, (bytecode_t){B_HEX, {.num = offset}});
          code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
        }
      }
      break;
    case A_INDEX:
      {
        assert(0);
        get_addr_ast(state, ast->as.index.expr);
        code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});

        compile(ast->as.index.num, state);

        switch (type_size(&ast->type)) {
          case 1:  break;
          case 4:  code(compiled, (bytecode_t){B_INST, {.inst = SHL}}); __attribute__((fallthrough));
          case 2:  code(compiled, (bytecode_t){B_INST, {.inst = SHL}}); break;
          default: assert(0);
        }

        code(compiled, (bytecode_t){B_INST, {.inst = POPB}});
        code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
      }
      break;
    case A_FUNCALL:
      compile(ast, state);
      code(compiled, (bytecode_t){B_INST, {.inst = SP_A}});
      code(compiled, (bytecode_t){B_INST, {.inst = RAM_BL}});
      code(compiled, (bytecode_t){B_HEX, {.num = 2}});
      code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
      break;
    default: assert(0);
  }
}

void compile(ast_t *ast, state_t *state) {
  assert(state);
  assert(ast);

  compiled_t *compiled = state->compiled;

  switch (ast->kind) {
    case A_NONE: assert(0);
    case A_GLOBAL:
    case A_BLOCK:
      compile(ast->as.astlist.ast, state);
      if (ast->as.astlist.next) {
        compile(ast->as.astlist.next, state);
      }
      break;
    case A_PARAM:
      if (ast->as.astlist.next) {
        compile(ast->as.astlist.next, state);
      }
      compile(ast->as.astlist.ast, state);
      break;
    case A_FUNCDECL:
      {
        state_add_symbol(state, (symbol_t){ast->as.funcdecl.name, &ast->type, 0, {}});
        state_push_scope(state);
        state->param = 4 + type_size_aligned(&ast->as.funcdecl.type);
        if (ast->as.funcdecl.params) {
          compile(ast->as.funcdecl.params, state);
        }
        bytecode_t b = {B_SETLABEL, {0}};
        sprintf(b.arg.str, SV_FMT, SV_UNPACK(ast->as.funcdecl.name.image));
        code(compiled, b);
        if (ast->as.funcdecl.block) {
          compile(ast->as.funcdecl.block, state);
        }
        if (!(compiled->code[compiled->code_num - 1].kind == B_INST &&
              compiled->code[compiled->code_num - 1].arg.inst == RET)) {
          state_change_sp(state, -state->sp);
          code(compiled, (bytecode_t){B_INST, {.inst = RET}});
        }
        state_drop_scope(state);
      }
      break;
    case A_PARAMDEF:
      {
        state_add_symbol(state, (symbol_t){ast->as.paramdef.name, &ast->as.paramdef.type, INFO_LOCAL, {-state->param}});
        state->param += type_size_aligned(&ast->as.paramdef.type);
        if (ast->as.paramdef.next) {
          compile(ast->as.paramdef.next, state);
        }
      }
      break;
    case A_STATEMENT:
      {
        int sp_start = state->sp;
        compile(ast->as.ast, state);
        assert(ast->as.ast->kind != A_RETURN);
        if (ast->as.ast->kind != A_DECL && state->sp - sp_start != 0) {
          state_change_sp(state, -(state->sp - sp_start));
        }
      }
      break;
    case A_RETURN:
      if (ast->as.return_.expr) {
        compile(ast->as.return_.expr, state);
        int size = type_size_aligned(&ast->as.return_.expr->type);
        int offset = state->sp + 2 + size;
        state_change_sp(state, -offset);

        code(compiled, (bytecode_t){B_INST, {.inst = SP_A}});
        code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_B}});
        code(compiled, (bytecode_t){B_HEX2, {.num = offset - size}});

        for (; size > 2; size -= 2) {
          code(compiled, (bytecode_t){B_INST, {.inst = SUB}});
          code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
          code(compiled, (bytecode_t){B_INST, {.inst = rB_A}});
          code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
          state->sp += 2;
          code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
          code(compiled, (bytecode_t){B_HEX, {.num = 2}});
        }
        code(compiled, (bytecode_t){B_INST, {.inst = SUB}});
        code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
        code(compiled, (bytecode_t){B_INST, {.inst = rB_A}});
        code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
        state->sp += 2;

        state_change_sp(state, 2);
      } else {
        state_change_sp(state, -state->sp);
      }
      break;
    case A_BINARYOP:
      if (ast->as.binaryop.op == T_DOT) {
        get_addr_ast(state, ast);
        code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
        coderead(state, &ast->type);
      } else {
        compile(ast->as.binaryop.lhs, state);
        compile(ast->as.binaryop.rhs, state);
        code(compiled, (bytecode_t){B_INST, {.inst = POPB}});
        code(compiled, (bytecode_t){B_INST, {.inst = POPA}});
        state->sp -= 4;
        switch (ast->as.binaryop.op) {
          case T_PLUS:  code(compiled, (bytecode_t){B_INST, {.inst = SUM}}); break;
          case T_MINUS: code(compiled, (bytecode_t){B_INST, {.inst = SUB}}); break;
          default:      assert(0);
        }
        code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
        state->sp += 2;
      }
      break;
    case A_UNARYOP:
      switch (ast->as.unaryop.op) {
        case T_STAR:
          compile(ast->as.unaryop.arg, state);
          code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
          coderead(state, &ast->type);
          break;
        default: assert(0);
      }
      break;
    case A_FAC:
      switch (ast->as.fac.kind) {
        case T_INT:
          {
            int num = atoi(ast->as.fac.image.start);
            if (num < 256) {
              code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
              code(compiled, (bytecode_t){B_HEX, {.num = num}});
            } else {
              code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
              code(compiled, (bytecode_t){B_HEX2, {.num = num}});
            }
            code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
            state->sp += 2;
          }
          break;
        case T_SYM:
          {
            symbol_t *s = state_find_symbol(state, ast->as.fac);
            assert(s->type);
            if (s->type->kind == TY_ARRAY) {
              assert(0);  // TODO error
            } else if (type_size(s->type) <= 2 && s->kind == INFO_LOCAL && s->info.local < state->sp &&
                       state->sp - s->info.local < 256) {
              code(compiled, (bytecode_t){B_INST, {.inst = PEEKAR}});
              code(compiled, (bytecode_t){B_HEX, {.num = state->sp - s->info.local}});
            } else {
              get_addr(state, ast->as.fac);
              code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
              coderead(state, &ast->type);
            }
          }
          break;
        case T_HEX:
          {
            int num = strtol(ast->as.fac.image.start, NULL, 16);
            if (ast->as.fac.image.len == 4) {
              code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
              code(compiled, (bytecode_t){B_HEX, {.num = num}});
            } else if (ast->as.fac.image.len == 6) {
              code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
              code(compiled, (bytecode_t){B_HEX2, {.num = num}});
            } else {
              assert(0);
            }
            code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
            state->sp += 2;
          }
          break;
        case T_STRING:
          bytecode_t uli = datauli(compiled, state);
          bytecode_t b = {B_STRING, {}};
          sprintf(b.arg.str, SV_FMT, SV_UNPACK(ast->as.fac.image));
          data(compiled, b);
          data(compiled, (bytecode_t){B_HEX, {.num = 0}});
          code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
          uli.kind = B_LABEL;
          code(compiled, uli);
          code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
          state->sp += 2;
          break;
        default: assert(0);
      }
      break;
    case A_DECL:
      if (ast->as.decl.expr) {
        compile(ast->as.decl.expr, state);
      } else if (ast->as.decl.type.kind == TY_ARRAY && ast->as.decl.type.as.array.len.kind == ARRAY_LEN_EXPR) {
        TODO;
        /*
           compile(ast->as.decl.array_len, state);
           code(compiled, (bytecode_t){B_INST, {.inst = SP_A}});
           code(compiled, (bytecode_t){B_INST, {.inst = INCA}});
           code(compiled, (bytecode_t){B_INST, {.inst = INCA}});
           code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
           code(compiled, (bytecode_t){B_INST, {.inst = POPA}});
           code(compiled, (bytecode_t){B_INST, {.inst = SUB}});
           code(compiled, (bytecode_t){B_INST, {.inst = A_SP}});
           state->sp -= 2;
           */
      } else {
        state_change_sp(state, type_size_aligned(&ast->as.decl.type));
      }
      state_add_symbol(state, (symbol_t){ast->as.decl.name, &ast->as.decl.type, INFO_LOCAL, {state->sp - 2}});
      break;
    case A_GLOBDECL: TODO; break;
    case A_ASSIGN:
      compile(ast->as.assign.expr, state);
      get_addr_ast(state, ast->as.assign.dest);
      code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
      code(compiled, (bytecode_t){B_INST, {.inst = POPA}});
      state->sp -= 2;
      codewrite(compiled, &ast->type);
      break;
    case A_FUNCALL:
      {
        if (ast->as.funcall.params) {
          compile(ast->as.funcall.params, state);
        }
        state_change_sp(state, type_size_aligned(&ast->type));
        code(compiled, (bytecode_t){B_INST, {.inst = CALLR}});
        bytecode_t b = {B_RELLABEL, {0}};
        sprintf(b.arg.str, SV_FMT, SV_UNPACK(ast->as.funcall.name.image));
        code(compiled, b);
      }
      break;
    case A_ARRAY:
      if (ast->type.as.array.type->kind == TY_CHAR) {
        if (ast->as.astlist.next) {
          if (ast->as.astlist.next->as.astlist.next) {
            compile(ast->as.astlist.next->as.astlist.next, state);
          }
          ast_t *a = ast->as.astlist.ast;
          ast_t *b = ast->as.astlist.next->as.astlist.ast;

          code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
          assert(a->kind == A_FAC && a->as.fac.kind == T_HEX);
          code(compiled, (bytecode_t){B_HEX, {.num = strtol(a->as.fac.image.start, NULL, 16)}});
          code(compiled, (bytecode_t){B_INST, {.inst = RAM_BL}});
          assert(b->kind == A_FAC && b->as.fac.kind == T_HEX);
          code(compiled, (bytecode_t){B_HEX, {.num = strtol(b->as.fac.image.start, NULL, 16)}});
          code(compiled, (bytecode_t){B_INST, {.inst = B_AH}});
          code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
          state->sp += 2;
        } else {
          compile(ast->as.astlist.ast, state);
        }
        break;
      }
      if (ast->as.astlist.next) {
        compile(ast->as.astlist.next, state);
      }
      compile(ast->as.astlist.ast, state);
      break;
    case A_INDEX:
      {
        compile(ast->as.index.expr, state);
        compile(ast->as.index.num, state);
        code(compiled, (bytecode_t){B_INST, {.inst = POPA}});
        switch (type_size(&ast->type)) {
          case 1:  break;
          case 4:  code(compiled, (bytecode_t){B_INST, {.inst = SHL}}); __attribute__((fallthrough));
          case 2:  code(compiled, (bytecode_t){B_INST, {.inst = SHL}}); break;
          default: TODO;
        }
        code(compiled, (bytecode_t){B_INST, {.inst = POPB}});
        state->sp -= 4;
        code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
        coderead(state, &ast->type);
      }
      break;
    case A_TYPEDEF: break;
    case A_CAST:
      if (ast->as.cast.target.kind == TY_PTR && ast->as.cast.ast->type.kind == TY_ARRAY) {
        get_addr_ast(state, ast->as.cast.ast);
        code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
        state->sp += 2;
      } else if (ast->as.cast.target.kind == TY_STRUCT && ast->as.cast.ast->kind == A_ARRAY) {
        exit(101);
      } else {
        int tsize = type_size_aligned(&ast->as.cast.target);
        int size = type_size_aligned(&ast->as.cast.ast->type);
        assert(tsize - size >= 0);
        if (tsize - size > 0) {
          code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
          code(compiled, (bytecode_t){B_HEX, {.num = 0}});
        }
        for (int i = 0; i < (tsize - size) / 2; ++i) {
          code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
        }
        state->sp += tsize - size;
        compile(ast->as.cast.ast, state);
      }
      break;
  }
}

/*
   void compile(ast_t *ast, state_t *state) {
   assert(state);
   if (!ast) { return; }

   compiled_t *compiled = state->compiled;

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
   memcpy(b.arg.str, ast->as.funcdecl.name.image.start,
   ast->as.funcdecl.name.image.len); code(compiled, b);
   compile(ast->as.funcdecl.block, state);
   if (!(compiled->code[compiled->code_num-1].kind == B_INST &&
   compiled->code[compiled->code_num-1].arg.inst == RET)) {
   assert(state->sp*2 < 256);
   if (state->sp > 3) {
   code(compiled, (bytecode_t){B_INST, {.inst = SP_A}});
   code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
   code(compiled, (bytecode_t){B_INST, {.inst = RAM_BL}});
   code(compiled, (bytecode_t){B_HEX, {.num = state->sp*2}});
   code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
   code(compiled, (bytecode_t){B_INST, {.inst = A_SP}});
   } else {
   for (int i = 0; i < state->sp; ++i)
   code(compiled, (bytecode_t){B_INST, {.inst = INCSP}});
   }
   code(compiled, (bytecode_t){B_INST, {.inst = RET}});
   }
   state_drop_scope(state);
   } break;
   case A_PARAMDEF:
   {
   scope_t *scope = &state->scopes[state->scope_num-1];
   assert(scope->symbol_num + 1 < SYMBOL_MAX);
   ++state->param;
   scope->symbols[scope->symbol_num ++] = (symbol_t) {ast->as.paramdef.name,
   &ast->as.paramdef.type, INFO_LOCAL, {-1-state->param}};

   compile(ast->as.paramdef.next, state);
   } break;
   case A_RETURN:
   if (ast->as.return_.expr) { compile(ast->as.return_.expr, state); }
   for (int i = 0; i < state->sp; ++i) {
   code(compiled, (bytecode_t){B_INST, {.inst = INCSP}});
   }
   code(compiled, (bytecode_t){B_INST, {.inst = RET}});
   break;
   case A_BINARYOP:
   assert(ast->as.binaryop.lhs);
   assert(ast->as.binaryop.rhs);
   if (ast->as.binaryop.op.kind == T_DOT) {
   get_addr_ast(state, ast);
   code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
   coderead(compiled, &ast->type);
   } else {
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
    get_addr_ast(state, ast->as.unaryop.arg);
    break;
  case T_STAR:
    compile(ast->as.unaryop.arg, state);
    assert(ast->as.unaryop.arg->type.kind == TY_PTR);
    code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
    coderead(compiled, &ast->type);
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
      coderead(compiled, &ast->type);
      break;
    case T_STRING:
      {
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
        bytecode_t b = datauli(compiled, state);
        b.kind = B_LABEL;
        code(compiled, b);
        b.kind = B_STRING;
        memset(b.arg.str, 0, LABEL_MAX);
        memcpy(b.arg.str, token.image.start, token.image.len);
        data(compiled, b);
        data(compiled, (bytecode_t){B_HEX, {.num = 0}});
      } break;
    case T_HEX:
      if (token.image.len - 2 == 2) {
        code(compiled, (bytecode_t){B_INST, {.inst = RAM_AL}});
        code(compiled, (bytecode_t){B_HEX, {.num = strtol(token.image.start+2,
              NULL, 16)}}); } else { code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
          code(compiled, (bytecode_t){B_HEX2, {.num = strtol(token.image.start+2,
                NULL, 16)}});
        }
        break;
    default:
        assert(0);
  }
} break;
case A_GLOBDECL:
{
  state_add_global_symbol(state, ast->as.decl.name, &ast->as.decl.type,
      state->uli);

  if (ast->as.decl.expr) {
    switch (ast->as.decl.expr->kind) {
      default:
        ast_dump(ast->as.decl.expr, true);
        exit(6);
    }
  } else {
    int size = type_size(&ast->as.decl.type);
    size += size%2;
    datauli(compiled, state);
    data(compiled, (bytecode_t){B_DB, {.num = size}});
  }

  if (ast->as.decl.expr) {
    ast_t *expr = ast->as.decl.expr;
    if (expr->kind == A_ARRAY) {
      compile(expr, state);
      int diff = ast->as.decl.type.as.array.len - expr->type.as.array.len;
      if (diff > 0) {
        data(compiled, (bytecode_t){B_DB, {.num =
            diff*type_size(expr->type.as.array.type)}});
      }
      return;
    }
    if (expr->kind == A_FAC) {
      switch (expr->as.fac.kind) {
        case T_INT:
          datauli(compiled, state);
          datanum(compiled, &ast->as.decl.type, atoi(expr->as.fac.image.start));
          return;
        case T_HEX:
          datauli(compiled, state);
          datanum(compiled, &ast->as.decl.type, atoi(expr->as.fac.image.start +
                2)); return; case T_STRING: compile(expr, state); return; default:
      }
    }
  }

  bytecode_t b = datauli(compiled, state);

  if (ast->as.decl.expr) {
    compiled->is_init = true;
    compile(ast->as.decl.expr, state);
    code(compiled, (bytecode_t){B_INST, {.inst = RAM_B}});
    b.kind = B_LABEL;
    code(compiled, b);
    codewrite(compiled, &ast->as.decl.type);
    compiled->is_init = false;
  }

  datanum(compiled, &ast->as.decl.type, 0);
} break;
case A_DECL:
state_add_local_symbol(state, ast->as.decl.name, &ast->as.decl.type);
if (ast->as.decl.expr) {
  compile(ast->as.decl.expr, state);
  code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
} else {
  int size = type_size(&ast->as.decl.type);
  size += size%2;
  if (size > 2) {
    code(compiled, (bytecode_t){B_INST, {.inst = SP_A}});
    code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
    code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
    code(compiled, (bytecode_t){B_HEX2, {.num = size}});
    code(compiled, (bytecode_t){B_INST, {.inst = A_SP}});
  } else {
    code(compiled, (bytecode_t){B_INST, {.inst = DECSP}});
  }
}
break;
case A_ASSIGN:
get_addr_ast(state, ast->as.assign.dest);

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
codewrite(compiled, &ast->type);
break;
case A_FUNCALL:
{
  compile(ast->as.funcall.params, state);
  code(compiled, (bytecode_t){B_INST, {.inst = CALLR}});
  bytecode_t b = {B_RELLABEL, {0}};
  memcpy(b.arg.str, ast->as.funcall.name.image.start,
      ast->as.funcall.name.image.len); code(compiled, b); } break; case A_PARAM:
  compile(ast->as.astlist.next, state);
  compile(ast->as.astlist.ast, state);
  code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
  ++ state->sp;
  break;
  case A_ARRAY:
  assert(0);
{
  bytecode_t b = datauli(compiled, state);
  b.kind = B_LABEL;

  ast_t *to_compile[ast->type.as.array.len];
  int offsets[ast->type.as.array.len];
  int to_compile_num = 0;

  int i = 0;
  for (ast_t *asti = ast; asti; asti = asti->as.astlist.next, i += 1) {
    if (asti->as.astlist.ast->kind == A_FAC) {
      switch (asti->as.astlist.ast->as.fac.kind) {
        case T_INT:
          datanum(compiled, ast->type.as.array.type,
              atoi(asti->as.astlist.ast->as.fac.image.start)); continue; case T_HEX:
            datanum(compiled, ast->type.as.array.type,
                strtol(asti->as.astlist.ast->as.fac.image.start, NULL, 16)); continue; default:
      }
    }

    datanum(compiled, ast->type.as.array.type, 0);
    to_compile[to_compile_num] = asti->as.astlist.ast;
    offsets[to_compile_num] = i;
    ++ to_compile_num;
  }

  int size = type_size(ast->type.as.array.type);

  int diff = ast->type.as.array.len - i;
  if (diff > 0) {
    data(compiled, (bytecode_t){B_DB, {.num = diff*size}});
  }

  compiled->is_init = true;
  for (int i = 0; i < to_compile_num; ++i) {
    compile(to_compile[i], state);
    code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});
    code(compiled, (bytecode_t){B_INST, {.inst = RAM_B}});
    code(compiled, b);
    if (i > 0) {
      code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
      code(compiled, (bytecode_t){B_HEX2, {.num = offsets[i]*size}});
      code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
      code(compiled, (bytecode_t){B_INST, {.inst = A_B}});
    }
    code(compiled, (bytecode_t){B_INST, {.inst = POPA}});
    codewrite(compiled, ast->type.as.array.type);

  }
  compiled->is_init = false;

  code(compiled, (bytecode_t){B_INST, {.inst = RAM_A}});
  code(compiled, b);
} break;
case A_INDEX:
compile(ast->as.index.num, state);
switch (type_size(&ast->type)) {
  case 1:
    break;
  case 2:
    code(compiled, (bytecode_t){B_INST, {.inst = SHL}}); // index * 2
    break;
  default:
    TODO;
}
code(compiled, (bytecode_t){B_INST, {.inst = PUSHA}});

get_addr(state, ast->as.index.name);
code(compiled, (bytecode_t){B_INST, {.inst = POPB}});
code(compiled, (bytecode_t){B_INST, {.inst = SUM}});
coderead(compiled, &ast->type);
break;
case A_TYPEDEF:
break;
}
}
*/
bool compiled_is_inst(compiled_t *compiled, int i, instruction_t inst) {
  assert(compiled);

  if (i >= compiled->code_num) {
    return false;
  }
  if (compiled->code[i].kind != B_INST) {
    return false;
  }
  if (compiled->code[i].arg.inst != inst) {
    return false;
  }
  return true;
}

void compiled_copy(compiled_t *compiled, int i, int a, int n) {
  assert(compiled);
  assert(n >= a + 1);
  memcpy(&compiled->code[i + a], &compiled->code[i + n], (compiled->code_num - i - n) * sizeof(bytecode_t));
  compiled->code_num -= n - a;
}

void optimize_compiled(compiled_t *compiled) {
  assert(compiled);

  for (int i = 0; i < compiled->code_num;) {
    if (compiled_is_inst(compiled, i, PEEKA) && compiled_is_inst(compiled, i + 1, A_B)) {
      compiled->code[i] = (bytecode_t){B_INST, {.inst = PEEKB}};
      compiled_copy(compiled, i, 1, 2);
      i = 0;
    } else if (compiled_is_inst(compiled, i, RAM_A) && compiled_is_inst(compiled, i + 2, A_B)) {
      compiled->code[i] = (bytecode_t){B_INST, {.inst = RAM_B}};
      compiled_copy(compiled, i, 2, 3);
      i = 0;
    } else if (compiled_is_inst(compiled, i, PUSHA) && compiled_is_inst(compiled, i + 1, PEEKA)) {
      compiled_copy(compiled, i, 1, 2);
      i = 0;
    } else if (compiled_is_inst(compiled, i, PEEKAR) && compiled->code[i + 1].arg.num == 2) {
      compiled->code[i] = (bytecode_t){B_INST, {.inst = PEEKA}};
      compiled_copy(compiled, i, 1, 2);
      i = 0;
    } else if (compiled_is_inst(compiled, i, SP_A) && compiled_is_inst(compiled, i + 1, RAM_BL) &&
               compiled_is_inst(compiled, i + 3, SUM) && compiled_is_inst(compiled, i + 4, A_B) &&
               compiled_is_inst(compiled, i + 5, rB_A)) {
      compiled->code[i] = (bytecode_t){B_INST, {.inst = PEEKAR}};
      compiled->code[i + 1] = compiled->code[i + 2];
      compiled_copy(compiled, i, 2, 6);
      i = 0;
    } else if (compiled_is_inst(compiled, i, RAM_A)) {
      // && compiled->code[i+1].kind == B_HEX2)
      // bytecode_dump(compiled->code[i+1], stdout); printf(" <-\n");

      // && compiled->code[i+1].arg.num < 256)
      // TODO: does not optimize this
      // compiled->code[i].arg.inst = RAM_AL;
      // compiled->code[i+1].kind = B_HEX;
      // i = 0;
      ++i;
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
          "                (if no module name or module all then it will execute "
          "only the tokenizer)\n"
          " -e <string>    compile the string provided\n"
          " -o <file>      write output to the file [default is 'out.asm']\n"
          " -O0 | -O       no optimization\n"
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

typedef enum { M_TOK, M_PAR, M_TYP, M_COM, M_COUNT } module_t;

typedef enum { OL_POST, OL_PRE, OL_COUNT } optlevel_t;

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

  assert(OL_COUNT < 8);
  uint8_t opt = 0;
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
          if (!*(argv + 1)) {
            debug = (1 << M_COUNT) - 1;
          } else {
            ++argv;
            debug = parse_module(*argv);
          }
          ++argv;
          break;
        case 'D':
          if (!*(argv + 1)) {
            debug = (1 << M_COUNT) - 1;
            exitat = (1 << M_COUNT) - 1;
          } else {
            ++argv;
            debug = parse_module(*argv);
            exitat = parse_module(*argv);
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
          ++argv;
          break;
        case 'h': help(0); break;
        case '-':
          if (strcmp(arg + 2, "help") == 0) {
            help(0);
          }
          __attribute__((fallthrough));
        default: fprintf(stderr, "ERROR: unknown flag '%s'", arg); help(1);
      }
    } else {
      assert(input_num + 1 < INPUTS_MAX);
      inputs[input_num++] = (input_t){INPUT_FILE, *argv};
      ++argv;
    }
  }

  tokenizer_t tokenizer = {0};
  compiled_t compiled = {0};
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
        }
        break;
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
      ast_dump(ast, false);
      printf("\n");
    }
    if ((exitat >> M_PAR) & 1) {
      exit(0);
    }

    state_init(&state);
    typecheck(ast, &state);
    if ((debug >> M_TYP) & 1) {
      printf("TYPED AST:\n");
      ast_dump(ast, true);
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

    if ((opt >> OL_PRE) & 1) {
      TODO;
    }

    state_init(&state);
    state.compiled = &compiled;
    compile(ast, &state);

    if ((opt >> OL_POST) & 1) {
      optimize_compiled(state.compiled);
    }

    if ((debug >> M_COM) & 1) {
      printf("ASSEMBLY:\n");
      code_dump(state.compiled, stdout);
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
    fprintf(stderr, "ERROR: cannot open file '%s': %s", output, strerror(errno));
    exit(1);
  }
  code_dump(state.compiled, file);

  return 0;
}
