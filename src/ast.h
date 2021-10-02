#pragma once
#include "Array.h"
#include "SrcLocation.h"
#include "TextType.h"
#include "mytypes.h"

struct ast_namespace;
struct ast_struct;
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

struct ast_array_definition {
  u64 size = 0;  // Leave zero for dynamic
  ast_array_definition* next = nullptr;
};

struct ast_element {
  TextType name = nullptr;
  ElementType type;
  TextType custom_name = nullptr;
  TextType namespace_name = nullptr;
  TextType init_value = nullptr;
  ast_struct* enclosing_struct = nullptr;
  SrcLocation loc;
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
  bool simple = false;
  bool simple_computed = false;
  bool hash_computed = false;
  bool has_compact = false;
  bool compact_computed = false;
  bool naked = false;
};

struct ast_enum {
  TextType name = nullptr;
  Array<TextType> elements;
  ast_namespace* space = nullptr;
  FileData* file = nullptr;
  SrcLocation loc;
  bool is_class = false;
};

struct ast_value {
  ElementType type;
  u64 int_val = 0;
  f64 float_val = 0;
  TextType str_val = nullptr;
  bool is_hex = false;
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
