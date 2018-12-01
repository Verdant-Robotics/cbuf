#pragma once

#include "ast.h"
#include "StringBuffer.h"

class AstPrinter
{
    StringBuffer *buffer;
    void print_elem(u32 ident, ast_element *elem);
    void print_struct(u32 ident, ast_struct *st);
    void print_namespace(u32 ident, ast_namespace *sp);
public:
    AstPrinter();
    ~AstPrinter();

    void print_ast(StringBuffer *buf, ast_global *ast);
    void print_ast(StringBuffer *buf, ast_struct *ast);
    void print_ast(StringBuffer *buf, ast_element *elem);
};