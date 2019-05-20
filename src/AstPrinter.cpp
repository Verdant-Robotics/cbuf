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
    "string",
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

    if (elem->custom_name) {
        buffer->print("%s ", elem->custom_name);
        if (sym) {
            auto *struc = sym->find_struct(elem->custom_name);
            if (struc && ! printed_types.find(struc) ) {
                // Mark the struct as printed
                // Print the struct we saw
                // Append it to our buffer

            }
            auto *elem = sym->find_enum(elem->custom_name);
            if (elem && ! printed_types.find(elem)) {
                // Mark the enum as printed
                // Print the enum we saw
                // Append it to our buffer
            }
        }
    } else {
        buffer->print("%s ", ElementTypeToStr[elem->type]);
    }
    buffer->print_no("%s", elem->name);

    while(ar != nullptr) {
        if (ar->size != 0) buffer->print_no("[%ld]", ar->size);
        else buffer->print_no("[]");
        ar = ar->next;
    }
    buffer->print_no(";\n");
}

void AstPrinter::print_struct(ast_struct *st)
{
    buffer->print("struct %s {\n", "", st->name);
    buffer->increase_ident();
    for(auto *elem: st->elements) {
        print_elem(elem);
    }
    buffer->decrease_ident();
    buffer->print("}\n", "");
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
    printed_types.clear();

    print_struct(ast);
    buffer = nullptr;
    printed_types.clear();
}

void AstPrinter::print_ast(StringBuffer *buf, ast_element *elem)
{
    buffer = buf;
    printed_types.clear();
    print_elem(elem);
    buffer = nullptr;
    printed_types.clear();
}
