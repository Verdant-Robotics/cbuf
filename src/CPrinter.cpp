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

static size_t ElementTypeToBytes[] = {
    1, // "uint8_t",
    2, // "uint16_t",
    4, // "uint32_t",
    8, // "uint64_t",
    1, // "int8_t",
    2, // "int16_t",
    4, // "int32_t",
    8, // "int64_t",
    4, // "float",
    8, // "double",
    0, // "std::string",
    4, // "bool"
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

    buffer->print("%*s// This has to be the first member\n", ident+4, "");
    buffer->print("%*svrm_preamble preamble = {\n", ident+4, "");
    buffer->print("%*s0x%lX,\n", ident+8, "", st->hash_value);
    if (st->simple) buffer->print("%*ssizeof(%s),\n", ident+8, "", st->name);
    else buffer->print("%*s0,\n", ident+8, "");
    buffer->print("%*s};\n", ident+4, "");

    for(auto *elem: st->elements) {
        print(ident + 4, elem);
    }

    buffer->print("%*s/// This is here to ensure hash is always available, just in case.\n", ident+4, "");
    buffer->print("%*sstatic const uint64_t TYPE_HASH = 0x%lX;\n", ident+4, "", st->hash_value);

    buffer->print("\n");

    // Encode and decode
    if (st->simple) {
        buffer->print("%*ssize_t encode_size()\n", ident+4, "");
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*sreturn sizeof(%s);\n", ident+8, "", st->name);
        buffer->print("%*s}\n\n", ident+4, "");

        buffer->print("%*sbool encode(char *buf, unsigned int buf_size)\n", ident+4, "");
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*sif (buf_size < sizeof(%s)) return false;\n", ident+8, "", st->name);
        buffer->print("%*smemcpy(buf, this, sizeof(*this));\n", ident+8, "");
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
        buffer->print("%*svrm_preamble *pre = (vrm_preamble *)buf;\n", ident+8, "");
        buffer->print("%*sif (pre->hash != TYPE_HASH) return false;\n", ident+8, "");
        buffer->print("%*smemcpy(buf, this, sizeof(*this));\n", ident+8, "");
        buffer->print("%*sreturn true;\n", ident+8, "");
        buffer->print("%*s}\n\n", ident+4, "");

        buffer->print("%*sstatic bool decode(char *buf, unsigned int buf_size, %s** var)\n", ident+4, "", st->name);
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*sif (buf_size < sizeof(%s)) return false;\n", ident+8, "", st->name);
        buffer->print("%*svrm_preamble *pre = (vrm_preamble *)buf;\n", ident+8, "");
        buffer->print("%*sif (pre->hash != TYPE_HASH) return false;\n", ident+8, "");
        buffer->print("%*s*var = (%s *)buf;\n", ident+8, "", st->name);
        buffer->print("%*sreturn true;\n", ident+8, "");
        buffer->print("%*s}\n\n", ident+4, "");

    } else {
        buffer->print("%*ssize_t encode_size()\n", ident+4, "");
        buffer->print("%*s{\n", ident+4, "");
        buffer->print("%*ssize_t ret_size =\n", ident+8, "");
        buffer->print("%*ssizeof(preamble)\n", ident+12, "");
        for(auto *elem: st->elements) {
            if (elem->array_suffix) {
                if (elem->is_dynamic_array) {
                    buffer->print("%*s+ sizeof(%s) * %s.size() + 4\n", ident+12, "", ElementTypeToStr[elem->type], elem->name );
                } else {
                    buffer->print("%*s+ sizeof(%s) * %d\n", ident+12, "", ElementTypeToStr[elem->type], elem->array_suffix->size );
                }
            }
            if (elem->type == TYPE_CUSTOM) {
                buffer->print("%*s+ %s.encode_size()\n", ident+12, "", elem->name );
            } else if (elem->type == TYPE_STRING) {
                buffer->print("%*s+ %s.length() + 4\n", ident+12, "", elem->name );
            } else {
                buffer->print("%*s+ sizeof(%s) // for %s\n", ident+12, "", ElementTypeToStr[elem->type], elem->name );
            }
        }
        buffer->print("%*s;\n", ident+8, "");
        buffer->print("%*sreturn ret_size;\n", ident+8, "");
        buffer->print("%*s}\n\n", ident+4, "");


        // Just the prototype, implement further down
        buffer->print("%*sbool encode(char *buf, unsigned int buf_size);\n", ident+4, "");
        buffer->print("%*s// This variant allocates memory on the buffer, to be freed later.\n", ident+4, "");
        buffer->print("%*sconst char *encode();\n", ident+4, "");
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
    buffer->print("#include <stdint.h> // uint8_t and such\n");
    buffer->print("#include <string.h> // memcpy\n");
    buffer->print("#include <vector>   // std::vector\n");
    buffer->print("#include <string>   // std::string\n");
    buffer->print("\n");

    for(auto *sp: top_ast->spaces) {
        print(0, sp);
    }

    for(auto *st: top_ast->global_space.structs) {
        print(0, st);
    }

    buffer = nullptr;
}
