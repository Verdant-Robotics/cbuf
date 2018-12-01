#include "AstPrinter.h"
#include <stdlib.h>
#include <stdio.h>

static const char * ElementTypeToStr[] = {
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "float",
    "double",
    "std::string",
    "bool"
};

AstPrinter::AstPrinter()
{
}

AstPrinter::~AstPrinter()
{
}

void AstPrinter::print_elem(u32 ident, ast_element *elem)
{
    buffer.print("%*s", ident, ""); // indentation
    ast_array_definition *ar = elem->array_suffix;
    bool close_array = false;

    if (ar && ar->size == 0 && elem->is_dynamic_array) {
        buffer.print("std::vector< ");
        close_array = true;
    }
    if (elem->custom_name) {
        buffer.print("%s ", elem->custom_name);
    } else {
        buffer.print("%s ", ElementTypeToStr[elem->type]);
    }
    if (close_array) {
        buffer.print("> ");
    }
    buffer.print("%s", elem->name);
    while(ar != nullptr) {
        if (ar->size != 0) buffer.print("[%ld]", ar->size);
        ar = ar->next;
    }
    buffer.print(";\n");
}

void AstPrinter::print_struct(u32 ident, ast_struct *st)
{
    buffer.print("%*sstruct %s {\n", ident, "", st->name);
    for(auto *elem: st->elements) {
        print_elem(ident + 4, elem);
    }
    buffer.print("%*s}\n\n", ident, "");
}

void AstPrinter::print_namespace(u32 ident, ast_namespace *sp)
{
    buffer.print("%*snamespace %s {\n",ident, "", sp->name);
    for(auto *st: sp->structs) {
        print_struct(ident+4,st);
    }
    buffer.print("%*s}\n\n", ident, "");
}

const char *AstPrinter::print_ast(ast_global *ast)
{
    buffer.reset();

    for(auto *sp: ast->spaces) {
        print_namespace(0, sp);
    }

    for(auto *st: ast->global_space.structs) {
        print_struct(0, st);
    }
    return buffer.get_buffer();
}

const char *AstPrinter::print_ast(ast_struct *ast)
{
    buffer.reset();

    print_struct(0, ast);
    return buffer.get_buffer();
}

const char *AstPrinter::print_ast(ast_element *elem)
{
    buffer.reset();

    print_elem(0, elem);
    return buffer.get_buffer();
}
