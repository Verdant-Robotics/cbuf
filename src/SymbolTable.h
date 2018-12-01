#pragma once

#include "ast.h"

class SymbolTable
{
    Array<ast_struct *> ar;
public:
    bool add_struct(ast_struct *st);
    ast_struct *find_symbol(TextType name);
    bool initialize(ast_global *top_ast);
};