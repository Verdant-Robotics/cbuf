#pragma once

#include "ast.h"

class SymbolTable {
  Array<ast_namespace*> spaces;

  //    bool add_struct(ast_struct *st);
  //    bool add_enum(ast_enum *en);
  bool add_namespace(ast_namespace* sp);

  TextType global_namespace_name = nullptr;

public:
  bool find_symbol(TextType name, const TextType current_nspace = nullptr) const;
  bool find_symbol(const ast_element* elem) const;
  ast_struct* find_struct(TextType name, const TextType current_nspace = nullptr) const;
  ast_enum* find_enum(TextType name, const TextType current_nspace = nullptr) const;
  ast_struct* find_struct(const ast_element* elem) const;
  ast_enum* find_enum(const ast_element* elem) const;
  ast_namespace* find_namespace(TextType name) const;
  bool initialize(ast_global* top_ast);
};
