#pragma once

#include "ast.h"
#include "StringBuffer.h"
#include "SymbolTable.h"

class CPrinter
{
    StringBuffer *buffer;
    SymbolTable *sym;
    void print(u32 ident, ast_namespace *sp);
    void print(u32 ident, ast_struct *st);
    void print(u32 ident, ast_element *elem);
public:
    void print(StringBuffer *buf, ast_global *top_ast, SymbolTable *symbols);
};