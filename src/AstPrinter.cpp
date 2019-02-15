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

void AstPrinter::print_elem(ast_element *elem)
{
    ast_array_definition *ar = elem->array_suffix;
    bool close_array = false;

    if (ar && ar->size == 0 && elem->is_dynamic_array) {
        buffer->print("std::vector< ");
        close_array = true;
    }
    if (elem->custom_name) {
        buffer->print("%s ", elem->custom_name);
    } else {
        buffer->print("%s ", ElementTypeToStr[elem->type]);
    }
    if (close_array) {
        buffer->print("> ");
    }
    buffer->print("%s", elem->name);
    while(ar != nullptr) {
        if (ar->size != 0) buffer->print("[%ld]", ar->size);
        ar = ar->next;
    }
    buffer->print(";\n");
}

void AstPrinter::print_struct(ast_struct *st)
{
    buffer->print("struct %s {\n", "", st->name);
    buffer->increase_ident();
    for(auto *elem: st->elements) {
        print_elem(elem);
    }
    buffer->decrease_ident();
    buffer->print("}\n\n", "");
}

void AstPrinter::print_channel(ast_channel *cn)
{
    // deprecated
}

void AstPrinter::print_namespace(ast_namespace *sp)
{
    buffer->print("namespace %s {\n", "", sp->name);
    buffer->increase_ident();
    for(auto *st: sp->structs) {
        print_struct(st);
    }
    buffer->decrease_ident();
    buffer->print("}\n\n", "");
}

void AstPrinter::print_ast(StringBuffer *buf, ast_global *ast)
{
    buffer = buf;
    for(auto *sp: ast->spaces) {
        print_namespace(sp);
    }

    for(auto *st: ast->global_space.structs) {
        print_struct(st);
    }
    buffer = nullptr;
}

void AstPrinter::print_ast(StringBuffer *buf, ast_struct *ast)
{
    buffer = buf;
    print_struct(ast);
    buffer = nullptr;
}

void AstPrinter::print_ast(StringBuffer *buf, ast_element *elem)
{
    buffer = buf;
    print_elem(elem);
    buffer = nullptr;
}
