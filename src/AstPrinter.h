#pragma once

#include <map>

#include "StdStringBuffer.h"
#include "SymbolTable.h"
#include "ast.h"

class AstPrinter {
  StdStringBuffer* buffer;
  SymbolTable* sym = nullptr;
  std::map<void*, int> printed_types;

  void print_elem(ast_element* elem);
  void print_enum(ast_enum* enm);
  void print_struct(ast_struct* st);
  void print_namespace(ast_namespace* sp);

public:
  AstPrinter();
  ~AstPrinter();

  void setSymbolTable(SymbolTable* s) { sym = s; }

  void print_ast(StdStringBuffer* buf, ast_global* ast);
  void print_ast(StdStringBuffer* buf, ast_struct* ast);
  void print_ast(StdStringBuffer* buf, ast_element* elem);
};
