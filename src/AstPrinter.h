#pragma once

#include "ast.h"

class AstPrinter
{
    char *buffer;
    char *end;
    void print_elem(u32 ident, ast_element *elem);
    void print_struct(u32 ident, ast_struct *st);
    void print_namespace(u32 ident, ast_namespace *sp);
public:
    AstPrinter();
    ~AstPrinter();

    const char *print_ast(ast_global *ast);
    const char *print_ast(ast_struct *ast);
    const char *print_ast(ast_element *elem);
};