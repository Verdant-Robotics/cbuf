#include "AstPrinter.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
    "short_string",
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
        if (elem->namespace_name) buffer->print("%s::%s ", elem->namespace_name, elem->custom_name);
        else buffer->print("%s ", elem->custom_name);
        if (sym) {
            auto *struc = sym->find_struct(elem);
            if (struc && !printed_types[struc->space]) {
                printed_types[struc->space] = 1;
                // Print the new namespace
                StringBuffer new_buf;
                StringBuffer *old_buf;
                old_buf = buffer;
                buffer = &new_buf;
                print_namespace(struc->space);
                buffer = old_buf;
                // Append it to our buffer
                buffer->prepend(&new_buf);
            }
            auto *enm = sym->find_enum(elem);
            if (enm && !printed_types[enm->space]) {
                printed_types[enm->space] = 1;
                // Print the new namespace
                StringBuffer new_buf;
                StringBuffer *old_buf;
                old_buf = buffer;
                buffer = &new_buf;
                print_namespace(enm->space);
                buffer = old_buf;
                // Append it to our buffer
                buffer->prepend(&new_buf);
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

void AstPrinter::print_enum(ast_enum *enm)
{
    printed_types[enm] = 1;
    buffer->print("enum %s{\n", enm->name);
    buffer->increase_ident();
    for(auto* el: enm->elements) {
        buffer->print("%s,\n", el);
    }
    buffer->decrease_ident();
    buffer->print("}\n");
}

void AstPrinter::print_struct(ast_struct *st)
{
    printed_types[st] = 1;
    buffer->print("struct %s {\n", st->name);
    buffer->increase_ident();
    for(auto *elem: st->elements) {
        print_elem(elem);
    }
    buffer->decrease_ident();
    buffer->print("}\n", "");
}

void AstPrinter::print_namespace(ast_namespace *sp)
{
    ast_namespace *oldsp = currentsp;
    currentsp = sp;
    printed_types[sp] = 1;
    if (strcmp(sp->name, GLOBAL_NAMESPACE)) {
      buffer->print("namespace %s {\n", sp->name);
      buffer->increase_ident();
    }
    for(auto *enm: sp->enums) {
        print_enum(enm);
    }
    for(auto *st: sp->structs) {
        print_struct(st);
    }
    if (strcmp(sp->name, GLOBAL_NAMESPACE)) {
      buffer->decrease_ident();
      buffer->print("}\n\n", "");
    } else {
      buffer->print("\n");
    }
}

void AstPrinter::print_ast(StringBuffer *buf, ast_global *ast)
{
    buffer = buf;
    for(auto *sp: ast->spaces) {
        print_namespace(sp);
    }

    print_namespace(&ast->global_space);

    buffer = nullptr;
}

void AstPrinter::print_ast(StringBuffer *buf, ast_struct *ast)
{
    buffer = buf;
    printed_types.clear();

    print_namespace(ast->space);

    buffer = nullptr;
    printed_types.clear();
}

void AstPrinter::print_ast(StringBuffer *buf, ast_element *elem)
{
    buffer = buf;
    printed_types.clear();    
    print_namespace(elem->enclosing_struct->space);
    buffer = nullptr;
    printed_types.clear();
}
