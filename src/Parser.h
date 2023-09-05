#pragma once

#include "Interp.h"
#include "Lexer.h"
#include "ast.h"

struct Args {
  Array<const char*> incs;
  const char* srcfile = nullptr;
  const char* outfile = nullptr;
  const char* jsonfile = nullptr;
  const char* depfile = nullptr;
  bool help = false;
};

class Parser {
  Allocator* pool;
  ast_global* top_level_ast = nullptr;
  void Error(const char* msg, ...);
  void ErrorWithLoc(SrcLocation& loc, const char* msg, ...);
  bool MustMatchToken(TOKEN_TYPE type, const char* msg);
  ast_element* parseElementDeclaration();
  ast_namespace* find_existing_namespace(const TextType name);
  void parseImport();
  ast_global* ParseInternal(ast_global* top);
  ast_expression* parseLiteral();
  ast_expression* parseSimpleLiteral();
  ast_expression* parseArrayLiteral();
  ast_expression* parseUnaryExpression();
  ast_expression* parseBinOpExpressionRecursive(u32 oldprec, ast_expression* lhs);
  ast_expression* parseBinOpExpression();
  ast_expression* parseExpression();
  ast_array_expression* parseArrayExpression();
  ast_value* computeExpressionValue(ast_expression* expr);

public:
  Lexer* lex = nullptr;
  Interp* interp = nullptr;
  Args* args = nullptr;
  bool success;

  ast_global* Parse(const char* filename, Allocator* pool, ast_global* top = nullptr);
  ast_global* ParseBuffer(const char* buffer, u64 buf_size, Allocator* pool, ast_global* top);

  ast_enum* parseEnum();
  ast_struct* parseStruct();
  ast_namespace* parseNamespace();
  ast_const* parseConst();
};
