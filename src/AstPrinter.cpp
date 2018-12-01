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
    buffer = new char[8196];
    buffer[0] = 0;
    end = buffer;
}

AstPrinter::~AstPrinter()
{
    delete buffer;
    buffer = nullptr;
    end = nullptr;
}

void AstPrinter::print_elem(u32 ident, ast_element *elem)
{
    end += sprintf(end, "%*s", ident, ""); // indentation
    ast_array_definition *ar = elem->array_suffix;
    bool close_array = false;

    if (ar && ar->size == 0 && elem->is_dynamic_array) {
        end += sprintf(end, "std::vector< ");
        close_array = true;
    }
    if (elem->custom_name) {
        end += sprintf(end, "%s ", elem->custom_name);
    } else {
        end += sprintf(end, "%s ", ElementTypeToStr[elem->type]);
    }
    if (close_array) {
        end += sprintf(end, "> ");
    }
    end += sprintf(end, "%s", elem->name);
    while(ar != nullptr) {
        if (ar->size != 0) end += sprintf(end, "[%ld]", ar->size);
        ar = ar->next;
    }
    end += sprintf(end, ";\n");
}

void AstPrinter::print_struct(u32 ident, ast_struct *st)
{
    end += sprintf(end, "%*sstruct %s {\n", ident, "", st->name);
    for(auto *elem: st->elements) {
        print_elem(ident + 4, elem);
    }
    end += sprintf(end, "%*s}\n\n", ident, "");
}

void AstPrinter::print_namespace(u32 ident, ast_namespace *sp)
{
    end += sprintf(end, "%*snamespace %s {\n",ident, "", sp->name);
    for(auto *st: sp->structs) {
        print_struct(ident+4,st);
    }
    end += sprintf(end, "%*s}\n\n", ident, "");
}

const char *AstPrinter::print_ast(ast_global *ast)
{
    end = buffer;
    end[0] = 0;

    for(auto *sp: ast->spaces) {
        print_namespace(0, sp);
    }

    for(auto *st: ast->global_space.structs) {
        print_struct(0, st);
    }
    return buffer;
}

const char *AstPrinter::print_ast(ast_struct *ast)
{
    end = buffer;
    end[0] = 0;

    print_struct(0, ast);
    return buffer;
}

const char *AstPrinter::print_ast(ast_element *elem)
{
    end = buffer;
    end[0] = 0;

    print_elem(0, elem);
    return buffer;
}
