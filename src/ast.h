#pragma once
#include "Array.h"
#include "SrcLocation.h"
#include "TextType.h"
#include "TokenType.h"
#include "mytypes.h"

struct ast_namespace;
struct ast_struct;
struct ast_value;
class FileData;

enum ElementType {
  TYPE_U8 = 0,
  TYPE_U16,
  TYPE_U32,
  TYPE_U64,
  TYPE_S8,
  TYPE_S16,
  TYPE_S32,
  TYPE_S64,
  TYPE_F32,
  TYPE_F64,
  TYPE_STRING,
  TYPE_SHORT_STRING,
  TYPE_BOOL,
  TYPE_CUSTOM
};

enum ExpressionType { EXPTYPE_LITERAL = 0, EXPTYPE_UNARY, EXPTYPE_BINARY, EXPTYPE_ARRAY_LITERAL };

enum ValueType {
  VALTYPE_INVALID = 0,
  VALTYPE_INTEGER,
  VALTYPE_FLOAT,
  VALTYPE_STRING,
  VALTYPE_BOOL,
  VALTYPE_IDENTIFIER,
  VALTYPE_ARRAY
};

struct ast_array_definition {
  u64 size = 0;  // Leave zero for dynamic
  ast_array_definition* next = nullptr;
};

struct ast_element {
  TextType name = nullptr;
  ElementType type;
  TextType custom_name = nullptr;
  TextType namespace_name = nullptr;
  ast_value* init_value = nullptr;
  ast_struct* enclosing_struct = nullptr;
  SrcLocation loc;
  u32 csize = 0;     // Size of the element, backend dependent
  u32 coffset = 0;   // Offset of the element if enclosed in struct
  u32 typesize = 0;  // size to increment dst buffer when reading elements
  bool is_dynamic_array = false;
  bool is_compact_array = false;
  ast_array_definition* array_suffix = nullptr;
};

struct ast_struct {
  TextType name = nullptr;
  Array<ast_element*> elements;
  ast_namespace* space = nullptr;
  FileData* file = nullptr;
  SrcLocation loc;
  u64 hash_value = 0;
  u32 csize = 0;  // Size of the struct, backend dependent
  bool simple = false;
  bool simple_computed = false;
  bool hash_computed = false;
  bool has_compact = false;
  bool compact_computed = false;
  bool naked = false;
};

struct enum_item {
  TextType item_name = nullptr;
  s64 item_value = 0;
  bool item_assigned = false;
};

struct ast_enum {
  TextType name = nullptr;
  Array<enum_item> elements;
  ast_namespace* space = nullptr;
  FileData* file = nullptr;
  SrcLocation loc;
  bool is_class = false;
};

// Generic parent for expressions
struct ast_expression {
  ExpressionType exptype;
  ElementType elemtype;
};

struct ast_value : ast_expression {
  ast_value() { exptype = EXPTYPE_LITERAL; }
  ElementType type;
  ValueType valtype = VALTYPE_INVALID;
  s64 int_val = 0;
  f64 float_val = 0;
  bool bool_val = false;
  TextType str_val = nullptr;
  bool is_hex = false;
};

struct ast_array_value : ast_value {
  ast_array_value() {
    exptype = EXPTYPE_ARRAY_LITERAL;
    valtype = VALTYPE_ARRAY;
  }
  Array<ast_value*> values;
};

struct ast_unaryexp : ast_expression {
  ast_unaryexp() { exptype = EXPTYPE_UNARY; }
  ast_expression* expr = nullptr;
  TOKEN_TYPE op = TK_INVALID;
};

struct ast_binaryexp : ast_expression {
  ast_binaryexp() { exptype = EXPTYPE_BINARY; }
  ast_expression* lhs = nullptr;
  ast_expression* rhs = nullptr;
  TOKEN_TYPE op = TK_INVALID;
};

// An expression to hold an array of values whose type is not known
// It is possible that this is not a valid expression, which will be checked
// when it is converted to a value
struct ast_array_expression : ast_expression {
  ast_array_expression()
      : ast_expression() {
    exptype = EXPTYPE_ARRAY_LITERAL;
  }
  Array<ast_expression*> expressions;
};

struct ast_const {
  TextType name = nullptr;
  ElementType type;
  SrcLocation loc;
  FileData* file = nullptr;
  ast_namespace* space = nullptr;
  ast_value value;
};

struct ast_namespace {
  TextType name = nullptr;
  Array<ast_struct*> structs;
  Array<ast_enum*> enums;
  Array<ast_const*> consts;
};

#define GLOBAL_NAMESPACE "__global_namespace"

struct ast_global {
  Array<ast_namespace*> spaces;
  Array<ast_enum*> enums;
  Array<ast_const*> consts;
  Array<TextType> imported_files;
  ast_namespace global_space;
  FileData* main_file = nullptr;
};
