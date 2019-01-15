#pragma once

#include "ast.h"

class SymbolTable
{
    Array<ast_struct *> ar;
    Array<ast_enum *> enar;
public:
    bool add_struct(ast_struct *st);
    bool add_enum(ast_enum *en);
    bool find_symbol(TextType name) const;
    ast_struct* find_struct(TextType name) const;
    ast_enum*   find_enum(TextType name) const;
    bool initialize(ast_global *top_ast);
};