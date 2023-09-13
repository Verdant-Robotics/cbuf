#pragma once

#include "StdStringBuffer.h"
#include "SymbolTable.h"
#include "ast.h"

class CPrinter {
  FileData* main_file = nullptr;
  StdStringBuffer* buffer;
  SymbolTable* sym;
  void print(ast_namespace* sp);
  void print(ast_struct* st);
  void print(ast_enum* en);
  void print(ast_element* elem);
  void print(ast_const* cst);

  void printInit(ast_element* elem);

  void printLoaderDeclaration(ast_namespace* sp);
  void printLoaderDeclaration(ast_struct* st);
  void printLoader(ast_namespace* sp);
  void printLoader(ast_struct* st);
  void printLoader(ast_element* elem);

  void print_net(ast_struct* st);

  // helpers
  void helper_print_array_suffix(ast_element* elem);

public:
  // Prints the main C header for a cbuf object
  void print(StdStringBuffer* buf, ast_global* top_ast, SymbolTable* symbols);

  // Prints a C header with json calls used to load a cbuf with data from a json object
  void printLoader(StdStringBuffer* buf, ast_global* top_ast, SymbolTable* symbols, const char* c_name);

  // Prints C-style dependencies to be used in a build system
  bool printDepfile(StdStringBuffer* buf, ast_global* top_ast, Array<const char*>& incs, std::string& error,
                    const char* c_name, const char* outfile);
};
