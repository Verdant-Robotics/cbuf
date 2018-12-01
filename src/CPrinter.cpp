#include "CPrinter.h"
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

void CPrinter::print(u32 ident, ast_namespace *sp)
{
    buffer->print("%*snamespace %s {\n",ident, "", sp->name);
    for(auto *st: sp->structs) {
        print(ident+4,st);
    }
    buffer->print("%*s};\n\n", ident, "");
}

void CPrinter::print(u32 ident, ast_struct *st)
{
    buffer->print("%*sstruct %s {\n", ident, "", st->name);

    buffer->print("%*svrm_preamble preamble;\n", ident+4, "");

    for(auto *elem: st->elements) {
        print(ident + 4, elem);
    }

    // Constructor
    buffer->print("%*s%s()\n", ident+4, "", st->name);
    buffer->print("%*s{\n", ident+4, "");
    buffer->print("%*spreamble.hash = 0x%lX;\n", ident+8, "", st->hash_value);
    if (st->simple) {
        buffer->print("%*spreamble.size = sizeof(%s);\n", ident+8, "", st->name);
    } else {
        buffer->print("%*spreamble.size = 0; // size to be computed later\n", ident+8, "");
    }
    buffer->print("%*s}\n\n", ident+4, "");

    // Encode and decode
    if (st->simple) {
        buffer->print("%*sbool encode(char *buf, unsigned int buf_size)\n", ident+4, "");
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*sif (buf_size < sizeof(%s)) return false;\n", ident+8, "", st->name);
        buffer->print("%*smemcpy(buf, this, this->size);\n", ident+8, "");
        buffer->print("%*sreturn true;\n", ident+8, "");
        buffer->print("%*s}\n\n", ident+4, "");

        buffer->print("%*s// This variant allows for no copy when writing to disk.\n", ident+4, "");
        buffer->print("%*sconst char *encode()\n", ident+4, "");
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*sreturn (const char *)this;\n", ident+8, "");
        buffer->print("%*s}\n\n", ident+4, "");

        buffer->print("%*sbool decode(char *buf, unsigned int buf_size)\n", ident+4, "");
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*sif (buf_size < sizeof(%s)) return false;\n", ident+8, "", st->name);
        buffer->print("%*smemcpy(buf, this, this->size);\n", ident+8, "");
        buffer->print("%*sreturn true;\n", ident+8, "");
        buffer->print("%*s}\n\n", ident+4, "");

        buffer->print("%*s// This variant allows for no copy when writing to disk.\n", ident+4, "");
        buffer->print("%*sconst char *encode()\n", ident+4, "");
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*sreturn (const char *)this;\n", ident+8, "");
        buffer->print("%*s}\n\n", ident+4, "");

    } else {
        // Just the prototype, implement further down
        buffer->print("%*sbool encode(char *buf, unsigned int buf_size);\n", ident+4, "");
        buffer->print("%*s// This variant allocates memory on the buffer, to be freed later.\n", ident+4, "");
        buffer->print("%*sconst char *encode()\n", ident+4, "");
    }

    buffer->print("%*s};\n\n", ident, "");
}

void CPrinter::print(u32 ident, ast_element *elem)
{
    buffer->print("%*s", ident, ""); // indentation
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

void CPrinter::print(StringBuffer *buf, ast_global *top_ast)
{
    buffer = buf;

    buffer->print("#pragma once\n");
    buffer->print("#include \"vrm_preamble.h\"\n");
    buffer->print("\n");

    for(auto *sp: top_ast->spaces) {
        print(0, sp);
    }

    for(auto *st: top_ast->global_space.structs) {
        print(0, st);
    }

    buffer = nullptr;
}
