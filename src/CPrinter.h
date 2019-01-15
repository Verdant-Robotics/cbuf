#pragma once

#include "ast.h"
#include "StringBuffer.h"
#include "SymbolTable.h"

class CPrinter
{
    StringBuffer *buffer;
    SymbolTable *sym;
    void print(ast_namespace *sp);
    void print(ast_struct *st);
    void print(ast_enum *en);
    void print(ast_channel *cn);
    void print(ast_element *elem);

  public:
    void print(StringBuffer *buf, ast_global *top_ast, SymbolTable *symbols);
};