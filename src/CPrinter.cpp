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

static bool simple_type(ElementType t)
{
    if (t != TYPE_STRING && t != TYPE_CUSTOM) {
        return true;
    }
    return false;
}

static bool struct_type(const ast_element *elem, const SymbolTable *sym )
{
    if (elem->type == TYPE_CUSTOM) {
        return sym->find_struct(elem->custom_name) != nullptr;
    }
    return false;
}

static bool enum_type(const ast_element *elem, const SymbolTable *sym )
{
    if (elem->type == TYPE_CUSTOM) {
        return sym->find_enum(elem->custom_name) != nullptr;
    }
    return false;
}


void CPrinter::print(ast_namespace *sp)
{
    buffer->print("namespace %s {\n", sp->name);
    buffer->increase_ident();

    for(auto *en: sp->enums) {
        print(en);
    }

    for(auto *st: sp->structs) {
        print(st);
    }
    
    for (auto *cn : sp->channels) {
        print(cn);
    }

    buffer->decrease_ident();
    buffer->print("};\n\n");
}

void CPrinter::print_net(ast_struct *st)
{
    buffer->print("size_t encode_net_size()\n");
    buffer->print("{\n"); buffer->increase_ident();
    buffer->print("size_t ret_size = sizeof(uint64_t) + sizeof(uint32_t); //sizeof(preamble)\n");
    for(auto *elem: st->elements) {
        if (elem->array_suffix) {
            if (simple_type(elem->type)) {
                // All the elements have the same size, easy
                buffer->print("ret_size += sizeof(%s) *",
                    ElementTypeToStr[elem->type] );
                if (elem->is_dynamic_array) {
                    buffer->print_no(" %s.size();\n", elem->name);
                    buffer->print("ret_size += sizeof(uint32_t); // Encode the length of %s\n",
                        elem->name);
                } else {
                    buffer->print_no(" %lu;\n", elem->array_suffix->size);
                }
            } else if (struct_type(elem, sym)) {
                // check if custom is simple or not
                auto *inner_st = sym->find_struct(elem->custom_name);
                if (inner_st->simple) {
                    buffer->print("ret_size += %s[0].encode_net_size() *", elem->name );
                    if (elem->is_dynamic_array) {
                        buffer->print_no(" %s.size();\n", elem->name);
                    } else {
                        buffer->print_no(" %lu;\n", elem->array_suffix->size);
                    }
                } else {
                    // We have to compute the size one by one...
                    buffer->print("for (auto &%s_elem: %s) {\n", elem->name, elem->name);
                    buffer->increase_ident();
                    buffer->print("ret_size + %s_elem.encode_net_size();\n", elem->name );
                    buffer->decrease_ident();
                    buffer->print("}\n");
                }
            } else if (enum_type(elem, sym)) {
                // Enums are always s32
                buffer->print("ret_size += sizeof(int32_t);\n");
            } else {
                assert(elem->type == TYPE_STRING);
                // We have to compute the size one by one...
                buffer->print("for (auto &%s_elem: %s) {\n", elem->name, elem->name);
                buffer->increase_ident();
                buffer->print("ret_size += %s_elem.length() + 4;\n", elem->name);
                buffer->decrease_ident();
                buffer->print("}\n");
            }
        } else if (struct_type(elem, sym)) {
            buffer->print("ret_size += %s.encode_net_size();\n", elem->name );
        } else if (enum_type(elem, sym)) {
            buffer->print("ret_size += sizeof(int32_t);\n", elem->name );
        } else if (elem->type == TYPE_STRING) {
            buffer->print("ret_size += %s.length() + 4;\n", elem->name );
        } else {
            buffer->print("ret_size += sizeof(%s); // for %s\n", ElementTypeToStr[elem->type], elem->name );
        }
    }
    buffer->print("return ret_size;\n"); buffer->decrease_ident();
    buffer->print("}\n\n");

    buffer->print("bool encode_net(char *buf, unsigned int buf_size)\n");
    buffer->print("{\n"); buffer->increase_ident();
    buffer->print("preamble.size = encode_net_size();\n");
    buffer->print("if (buf_size < preamble.size) return false;\n");
    buffer->print("*(uint64_t *)buf = preamble.hash;\n");
    buffer->print("buf += sizeof(uint64_t);\n");
    buffer->print("*(uint32_t *)buf = preamble.size;\n");
    buffer->print("buf += sizeof(uint32_t);\n");
    for(auto *elem: st->elements) {
        if (elem->array_suffix) {
            if (simple_type(elem->type)) {
                if (elem->is_dynamic_array) {
                    buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                    buffer->print("buf += sizeof(uint32_t);\n");
                    buffer->print("memcpy(buf, &%s[0], %s.size()*sizeof(%s));\n",
                        elem->name, elem->name, ElementTypeToStr[elem->type]);
                    buffer->print("buf += %s.size()*sizeof(%s);\n", elem->name, ElementTypeToStr[elem->type]);
                } else {
                    buffer->print("memcpy(buf, %s, %d*sizeof(%s));\n",
                        elem->name, elem->array_suffix->size, ElementTypeToStr[elem->type]);
                    buffer->print("buf += %lu*sizeof(%s);\n", elem->array_suffix->size, ElementTypeToStr[elem->type]);
                }
            } else if (struct_type(elem, sym)) {
                auto *inner_st = sym->find_struct(elem->custom_name);
                if (inner_st->simple) {
                    if (elem->is_dynamic_array) {
                        buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("memcpy(buf, &%s[0], %s.size()*%s[0].encode_net_size());\n",
                            elem->name, elem->name, elem->name);
                        buffer->print("buf += %s.size()*%s[0].encode_net_size();\n", elem->name, elem->name);
                    } else {
                        buffer->print("memcpy(buf, %s, %d*%s[0].encode_net_size());\n",
                            elem->name, elem->array_suffix->size, elem->name);
                        buffer->print("buf += %lu*%s[0].encode_net_size();\n", elem->array_suffix->size, elem->name);
                    }
                } else {
                    if (elem->is_dynamic_array) {
                        buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                    } // No need to encode elements on the static array case, we know them already
                    buffer->print("for(auto &%s_elem: %s) {\n", elem->name, elem->name);
                    buffer->increase_ident();
                    buffer->print("%s_elem.encode(buf, buf_size);\n", elem->name);
                    buffer->print("buf += %s_elem.encode_net_size();\n", elem->name);
                    buffer->decrease_ident();
                    buffer->print("}\n");
                }
            } else if (enum_type(elem, sym)) {
                    if (elem->is_dynamic_array) {
                        buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("memcpy(buf, &%s[0], %s.size()*sizeof(int32_t));\n",
                            elem->name, elem->name);
                        buffer->print("buf += %s.size()*sizeof(int32_t);\n", elem->name);
                    } else {
                        buffer->print("memcpy(buf, %s, %d*sizeof(int32_t));\n",
                            elem->name, elem->array_suffix->size);
                        buffer->print("buf += %lu*sizeof(int32_t);\n", elem->array_suffix->size);
                    }
            } else {
                assert(elem->type == TYPE_STRING);

                if (elem->is_dynamic_array) {
                    buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                    buffer->print("buf += sizeof(uint32_t);\n");
                } // No need to encode elements on the static array case, we know them already
                buffer->print("for(auto &%s_elem: %s) {\n", elem->name, elem->name);
                buffer->increase_ident();
                buffer->print("*(uint32_t *)buf = (uint32_t)%s_elem.length();\n", elem->name);
                buffer->print("buf += sizeof(uint32_t);\n");
                buffer->print("memcpy(buf, %s_elem.c_str(), %s_elem.length());\n", elem->name, elem->name);
                buffer->print("buf += %s_elem.length();\n", elem->name);
                buffer->decrease_ident();
                buffer->print("}\n");
            }
        } else if (struct_type(elem, sym)) {
            buffer->print("%s.encode_net(buf, buf_size);\n", elem->name);
            buffer->print("buf += %s.encode_net_size();\n", elem->name);
        } else if (enum_type(elem, sym)) {
            // this is an enum, treat it as an s32;
            buffer->print("*(int32_t *)buf = (int32_t)%s;\n", elem->name);
            buffer->print("buf += sizeof(int32_t);\n");
        } else if (elem->type == TYPE_STRING) {
            buffer->print("*(uint32_t *)buf = (uint32_t)%s.length();\n", elem->name );
            buffer->print("buf += sizeof(uint32_t);\n");
            buffer->print("memcpy(buf, %s.c_str(), %s.length());\n", elem->name, elem->name);
            buffer->print("buf += %s.length();\n", elem->name);
        } else {
            buffer->print("*(%s *)buf = %s;\n", ElementTypeToStr[elem->type], elem->name );
            buffer->print("buf += sizeof(%s);\n", ElementTypeToStr[elem->type]);
        }
    }
    buffer->print("\n");
    buffer->print("return true;\n"); buffer->decrease_ident();
    buffer->print("}\n\n");

    buffer->print("bool decode_net(char *buf, unsigned int buf_size)\n");
    buffer->print("{\n"); buffer->increase_ident();
    buffer->print("cbuf_preamble *pre = (cbuf_preamble *)buf;\n");
    buffer->print("if (pre->hash != TYPE_HASH) return false;\n");
    buffer->print("if (pre->size > buf_size) return false;\n");
    buffer->print("preamble.size = pre->size;\n");
    buffer->print("buf += sizeof(uint64_t) + sizeof(uint32_t);\n");

    for(auto *elem: st->elements) {
        if (elem->array_suffix) {
            if (simple_type(elem->type)) {
                if (elem->is_dynamic_array) {
                    buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                    buffer->print("buf += sizeof(uint32_t);\n");
                    buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                    buffer->increase_ident();
                    buffer->print("%s.push_back(*(%s *)buf);\n", elem->name, ElementTypeToStr[elem->type] );
                    buffer->print("buf += sizeof(%s);\n", ElementTypeToStr[elem->type]);
                    buffer->decrease_ident();
                    buffer->print("}\n");
                } else {
                    buffer->print("memcpy(%s, buf, %d*sizeof(%s));\n",
                        elem->name, elem->array_suffix->size, ElementTypeToStr[elem->type]);
                    buffer->print("buf += %lu*sizeof(%s);\n", elem->array_suffix->size, ElementTypeToStr[elem->type]);
                }
            } else if (struct_type(elem, sym)) {
                auto *inner_st = sym->find_struct(elem->custom_name);
                if (inner_st->simple) {
                    if (elem->is_dynamic_array) {
                        buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                        buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                        buffer->increase_ident();
                        buffer->print("%s[i].decode_net(buf, buf_size);\n", elem->name );
                        buffer->print("buf += %s[i].preamble.size;\n", elem->name);
                        buffer->decrease_ident();
                        buffer->print("}\n");
                    } else {
                        buffer->print("memcpy(%s, buf, %d*%s[0].encode_net_size());\n",
                            elem->name, elem->array_suffix->size, elem->name);
                        buffer->print("buf += %lu*%s[0].encode_net_size();\n", elem->array_suffix->size, elem->name);
                    }
                } else {
                    if (elem->is_dynamic_array) {
                        buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                        buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                    } else { // No need to decode the number of elements on the static array case, we know them already
                        buffer->print("for(uint32_t i=0; i<%lu; i++) {\n", elem->array_suffix->size );
                    }
                    buffer->increase_ident();
                    buffer->print("%s[i].decode_net(buf, buf_size);\n", elem->name );
                    buffer->print("buf += %s[i].preamble.size;\n", elem->name);
                    buffer->decrease_ident();
                    buffer->print("}\n");
                }
            } else if (enum_type(elem, sym)) {
                if (elem->is_dynamic_array) {
                    buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                    buffer->print("buf += sizeof(uint32_t);\n");
                    buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                    buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                    buffer->increase_ident();
                    buffer->print("%s[i] = *(%s *)buf;\n", elem->name, elem->custom_name );
                    buffer->print("buf += sizeof(int32_t);\n");
                    buffer->decrease_ident();
                    buffer->print("}\n");
                } else {
                    buffer->print("memcpy(%s, buf, %d*sizeof(int32_t));\n",
                        elem->name, elem->array_suffix->size);
                    buffer->print("buf += %lu*sizeof(int32_t);\n", elem->array_suffix->size, elem->name);
                }
            } else {
                assert(elem->type == TYPE_STRING);

                if (elem->is_dynamic_array) {
                    buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                    buffer->print("buf += sizeof(uint32_t);\n");
                    buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                    buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                } else {
                    buffer->print("for(uint32_t i=0; i<%lu; i++) {\n", elem->array_suffix->size );
                }
                buffer->increase_ident();
                buffer->print("uint32_t %s_length = *(uint32_t *)buf;\n", elem->name );
                buffer->print("buf += sizeof(uint32_t);\n");
                buffer->print("%s[i] = std::string(buf, %s_length);\n", elem->name, elem->name );
                buffer->print("buf += %s_length;\n", elem->name);
                buffer->decrease_ident();
                buffer->print("}\n");
            }
        } else if (struct_type(elem, sym)) {
            buffer->print("%s.decode_net(buf, buf_size);\n", elem->name );
            buffer->print("buf += %s.preamble.size;\n", elem->name);
        } else if (enum_type(elem, sym)) {
            buffer->print("%s = *(%s *)buf;\n", elem->name, elem->custom_name);
            buffer->print("buf += sizeof(int32_t);\n");
        } else if (elem->type == TYPE_STRING) {
            buffer->print("uint32_t %s_length = *(uint32_t *)buf;\n", elem->name );
            buffer->print("buf += sizeof(uint32_t);\n");
            buffer->print("%s = std::string(buf, %s_length);\n", elem->name, elem->name );
            buffer->print("buf += %s_length;\n", elem->name);
        } else {
            buffer->print("%s = *(%s *)buf;\n", elem->name, ElementTypeToStr[elem->type]);
            buffer->print("buf += sizeof(%s);\n", ElementTypeToStr[elem->type]);
        }
    }
    buffer->print("\n");
    buffer->print("return true;\n"); buffer->decrease_ident();
    buffer->print("}\n\n");

}

void CPrinter::print(ast_struct *st)
{
    buffer->print("struct %s {\n", st->name);
    buffer->increase_ident();

    buffer->print("// This has to be the first member\n");
    buffer->print("cbuf_preamble preamble = {\n"); buffer->increase_ident();
    buffer->print("0x%lX,\n", st->hash_value);
    if (st->simple)
        buffer->print("sizeof(%s),\n", st->name);
    else
        buffer->print("0,\n");
    buffer->print("};\n"); buffer->decrease_ident();

    for (auto *elem : st->elements) {
        print(elem);
    }

    buffer->print("/// This is here to ensure hash is always available, just in case.\n");
    buffer->print("static const uint64_t TYPE_HASH = 0x%lX;\n", st->hash_value);
    buffer->print("static uint64_t hash() { return TYPE_HASH; }\n");
    buffer->print("static constexpr const char* TYPE_STRING = \"%s\";\n", st->name);
    
    buffer->print("\n");

    // Encode and decode
    if (st->simple)
    {
        buffer->print("size_t encode_size()\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("return sizeof(%s);\n", st->name); buffer->decrease_ident();
        buffer->print("}\n\n");
        buffer->print("bool encode(char *buf, unsigned int buf_size)\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("if (buf_size < sizeof(%s)) return false;\n", st->name);
        buffer->print("memcpy(buf, this, sizeof(*this));\n");
        buffer->print("return true;\n"); buffer->decrease_ident();
        buffer->print("}\n\n");
        buffer->print("// This variant allows for no copy when writing to disk.\n");
        buffer->print("const char *encode()\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("return (const char *)this;\n"); buffer->decrease_ident();
        buffer->print("}\n\n");
        buffer->print("bool decode(char *buf, unsigned int buf_size)\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("if (buf_size < sizeof(%s)) return false;\n", st->name);
        buffer->print("cbuf_preamble *pre = (cbuf_preamble *)buf;\n");
        buffer->print("if (pre->hash != TYPE_HASH) return false;\n");
        buffer->print("memcpy(this, buf, sizeof(*this));\n");
        buffer->print("return true;\n"); buffer->decrease_ident();
        buffer->print("}\n\n");
        buffer->print("static bool decode(char *buf, unsigned int buf_size, %s** var)\n", st->name);
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("if (buf_size < sizeof(%s)) return false;\n", st->name);
        buffer->print("cbuf_preamble *pre = (cbuf_preamble *)buf;\n");
        buffer->print("if (pre->hash != TYPE_HASH) return false;\n");
        buffer->print("*var = (%s *)buf;\n", st->name);
        buffer->print("return true;\n"); buffer->decrease_ident();
        buffer->print("}\n\n"); 
        print_net(st);
    } else {
        buffer->print("size_t encode_size()\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("size_t ret_size = sizeof(preamble);\n");
        for(auto *elem: st->elements) {
            if (elem->array_suffix) {
                if (simple_type(elem->type)) {
                    // All the elements have the same size, easy
                    buffer->print("ret_size += sizeof(%s) *", 
                        ElementTypeToStr[elem->type] );
                    if (elem->is_dynamic_array) {
                        buffer->print_no(" %s.size();\n", elem->name);
                        buffer->print("ret_size += sizeof(uint32_t); // Encode the length of %s\n", 
                            elem->name);
                    } else {
                        buffer->print_no(" %lu;\n", elem->array_suffix->size);
                    }
                } else if (struct_type(elem, sym)) {
                    // check if custom is simple or not
                    auto *inner_st = sym->find_struct(elem->custom_name);
                    if (inner_st->simple) {
                        buffer->print("ret_size += %s[0].encode_size() *", elem->name );
                        if (elem->is_dynamic_array) {
                            buffer->print_no(" %s.size();\n", elem->name);
                        } else {
                            buffer->print_no(" %lu;\n", elem->array_suffix->size);
                        }
                    } else {
                        // We have to compute the size one by one...
                        buffer->print("for (auto &%s_elem: %s) {\n", elem->name, elem->name);
                        buffer->increase_ident();
                        buffer->print("ret_size + %s_elem.encode_size();\n", elem->name );
                        buffer->decrease_ident();
                        buffer->print("}\n");
                    }
                } else if (enum_type(elem, sym)) {
                    // Enums are always s32
                    buffer->print("ret_size += sizeof(int32_t);\n");
                } else {
                    assert(elem->type == TYPE_STRING);
                    // We have to compute the size one by one...
                    buffer->print("for (auto &%s_elem: %s) {\n", elem->name, elem->name);
                    buffer->increase_ident();
                    buffer->print("ret_size += %s_elem.length() + 4;\n", elem->name);
                    buffer->decrease_ident();
                    buffer->print("}\n");
                }
            } else if (struct_type(elem, sym)) {
                buffer->print("ret_size += %s.encode_size();\n", elem->name );
            } else if (enum_type(elem, sym)) {
                buffer->print("ret_size += sizeof(int32_t);\n", elem->name );
            } else if (elem->type == TYPE_STRING) {
                buffer->print("ret_size += %s.length() + 4;\n", elem->name );
            } else {
                buffer->print("ret_size += sizeof(%s); // for %s\n", ElementTypeToStr[elem->type], elem->name );
            }
        }
        buffer->print("return ret_size;\n"); buffer->decrease_ident();
        buffer->print("}\n\n");

        buffer->print("bool encode(char *buf, unsigned int buf_size)\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("preamble.size = encode_size();\n");
        buffer->print("if (buf_size < preamble.size) return false;\n");
        buffer->print("*(uint64_t *)buf = preamble.hash;\n");
        buffer->print("buf += sizeof(uint64_t);\n");
        buffer->print("*(uint32_t *)buf = preamble.size;\n");
        buffer->print("buf += sizeof(uint32_t);\n");
        for(auto *elem: st->elements) {
            if (elem->array_suffix) {
                if (simple_type(elem->type)) {
                    if (elem->is_dynamic_array) {
                        buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("memcpy(buf, &%s[0], %s.size()*sizeof(%s));\n", 
                            elem->name, elem->name, ElementTypeToStr[elem->type]);
                        buffer->print("buf += %s.size()*sizeof(%s);\n", elem->name, ElementTypeToStr[elem->type]);
                    } else {
                        buffer->print("memcpy(buf, %s, %d*sizeof(%s));\n", 
                            elem->name, elem->array_suffix->size, ElementTypeToStr[elem->type]);
                        buffer->print("buf += %lu*sizeof(%s);\n", elem->array_suffix->size, ElementTypeToStr[elem->type]);
                    }
                } else if (struct_type(elem, sym)) {
                    auto *inner_st = sym->find_struct(elem->custom_name);
                    if (inner_st->simple) {
                        if (elem->is_dynamic_array) {
                            buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                            buffer->print("buf += sizeof(uint32_t);\n");
                            buffer->print("memcpy(buf, &%s[0], %s.size()*%s[0].encode_size());\n", 
                                elem->name, elem->name, elem->name);
                            buffer->print("buf += %s.size()*%s[0].encode_size();\n", elem->name, elem->name);
                        } else {
                            buffer->print("memcpy(buf, %s, %d*%s[0].encode_size());\n",
                                elem->name, elem->array_suffix->size, elem->name);
                            buffer->print("buf += %lu*%s[0].encode_size();\n", elem->array_suffix->size, elem->name);
                        }
                    } else {
                        if (elem->is_dynamic_array) {
                            buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                            buffer->print("buf += sizeof(uint32_t);\n");
                        } // No need to encode elements on the static array case, we know them already
                        buffer->print("for(auto &%s_elem: %s) {\n", elem->name, elem->name);
                        buffer->increase_ident();
                        buffer->print("%s_elem.encode(buf, buf_size);\n", elem->name);
                        buffer->print("buf += %s_elem.encode_size();\n", elem->name);
                        buffer->decrease_ident();
                        buffer->print("}\n");
                    }
                } else if (enum_type(elem, sym)) {
                        if (elem->is_dynamic_array) {
                            buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                            buffer->print("buf += sizeof(uint32_t);\n");
                            buffer->print("memcpy(buf, &%s[0], %s.size()*sizeof(int32_t));\n", 
                                elem->name, elem->name);
                            buffer->print("buf += %s.size()*sizeof(int32_t);\n", elem->name);
                        } else {
                            buffer->print("memcpy(buf, %s, %d*sizeof(int32_t));\n",
                                elem->name, elem->array_suffix->size);
                            buffer->print("buf += %lu*sizeof(int32_t);\n", elem->array_suffix->size);
                        }
                } else {
                    assert(elem->type == TYPE_STRING);

                    if (elem->is_dynamic_array) {
                        buffer->print("*(uint32_t *)buf = (uint32_t)%s.size();\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                    } // No need to encode elements on the static array case, we know them already
                    buffer->print("for(auto &%s_elem: %s) {\n", elem->name, elem->name);
                    buffer->increase_ident();
                    buffer->print("*(uint32_t *)buf = (uint32_t)%s_elem.length();\n", elem->name);
                    buffer->print("buf += sizeof(uint32_t);\n");
                    buffer->print("memcpy(buf, %s_elem.c_str(), %s_elem.length());\n", elem->name, elem->name);
                    buffer->print("buf += %s_elem.length();\n", elem->name);
                    buffer->decrease_ident();
                    buffer->print("}\n");
                }
            } else if (struct_type(elem, sym)) {
                buffer->print("%s.encode(buf, buf_size);\n", elem->name);
                buffer->print("buf += %s.encode_size();\n", elem->name);
            } else if (enum_type(elem, sym)) {
                // this is an enum, treat it as an s32;
                buffer->print("*(int32_t *)buf = (int32_t)%s;\n", elem->name);
                buffer->print("buf += sizeof(int32_t);\n");            
            } else if (elem->type == TYPE_STRING) {
                buffer->print("*(uint32_t *)buf = (uint32_t)%s.length();\n", elem->name );
                buffer->print("buf += sizeof(uint32_t);\n");
                buffer->print("memcpy(buf, %s.c_str(), %s.length());\n", elem->name, elem->name);
                buffer->print("buf += %s.length();\n", elem->name);
            } else {            
                buffer->print("*(%s *)buf = %s;\n", ElementTypeToStr[elem->type], elem->name );
                buffer->print("buf += sizeof(%s);\n", ElementTypeToStr[elem->type]);
            }
        }
        buffer->print("\n");
        buffer->print("return true;\n"); buffer->decrease_ident();
        buffer->print("}\n\n");

        buffer->print("const char *encode()\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("preamble.size = encode_size();\n");
        buffer->print("char *buf = (char *)malloc(preamble.size);\n");
        buffer->print("encode(buf, preamble.size);\n");
        buffer->print("return buf;\n"); buffer->decrease_ident();
        buffer->print("}\n\n");

        buffer->print("bool decode(char *buf, unsigned int buf_size)\n");
        buffer->print("{\n"); buffer->increase_ident();
        buffer->print("cbuf_preamble *pre = (cbuf_preamble *)buf;\n");
        buffer->print("if (pre->hash != TYPE_HASH) return false;\n");
        buffer->print("if (pre->size > buf_size) return false;\n");
        buffer->print("preamble.size = pre->size;\n");
        buffer->print("buf += sizeof(uint64_t) + sizeof(uint32_t);\n");

        for(auto *elem: st->elements) {
            if (elem->array_suffix) {
                if (simple_type(elem->type)) {
                    if (elem->is_dynamic_array) {
                        buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                        buffer->increase_ident();
                        buffer->print("%s.push_back(*(%s *)buf);\n", elem->name, ElementTypeToStr[elem->type] );
                        buffer->print("buf += sizeof(%s);\n", ElementTypeToStr[elem->type]);
                        buffer->decrease_ident();
                        buffer->print("}\n");
                    } else {
                        buffer->print("memcpy(%s, buf, %d*sizeof(%s));\n", 
                            elem->name, elem->array_suffix->size, ElementTypeToStr[elem->type]);
                        buffer->print("buf += %lu*sizeof(%s);\n", elem->array_suffix->size, ElementTypeToStr[elem->type]);
                    }
                } else if (struct_type(elem, sym)) {
                    auto *inner_st = sym->find_struct(elem->custom_name);
                    if (inner_st->simple) {
                        if (elem->is_dynamic_array) {
                            buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                            buffer->print("buf += sizeof(uint32_t);\n");
                            buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                            buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                            buffer->increase_ident();
                            buffer->print("%s[i].decode(buf, buf_size);\n", elem->name );
                            buffer->print("buf += %s[i].preamble.size;\n", elem->name);
                            buffer->decrease_ident();
                            buffer->print("}\n");
                        } else {
                            buffer->print("memcpy(%s, buf, %d*%s[0].encode_size());\n", 
                                elem->name, elem->array_suffix->size, elem->name);
                            buffer->print("buf += %lu*%s[0].encode_size();\n", elem->array_suffix->size, elem->name);
                        }
                    } else {
                        if (elem->is_dynamic_array) {
                            buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                            buffer->print("buf += sizeof(uint32_t);\n");
                            buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                            buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                        } else { // No need to decode the number of elements on the static array case, we know them already
                            buffer->print("for(uint32_t i=0; i<%lu; i++) {\n", elem->array_suffix->size );
                        }
                        buffer->increase_ident();
                        buffer->print("%s[i].decode(buf, buf_size);\n", elem->name );
                        buffer->print("buf += %s[i].preamble.size;\n", elem->name);
                        buffer->decrease_ident();
                        buffer->print("}\n");
                    }
                } else if (enum_type(elem, sym)) {
                    if (elem->is_dynamic_array) {
                        buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                        buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                        buffer->increase_ident();
                        buffer->print("%s[i] = *(%s *)buf;\n", elem->name, elem->custom_name );
                        buffer->print("buf += sizeof(int32_t);\n");
                        buffer->decrease_ident();
                        buffer->print("}\n");
                    } else {
                        buffer->print("memcpy(%s, buf, %d*sizeof(int32_t));\n", 
                            elem->name, elem->array_suffix->size);
                        buffer->print("buf += %lu*sizeof(int32_t);\n", elem->array_suffix->size, elem->name);
                    }                
                } else {
                    assert(elem->type == TYPE_STRING);

                    if (elem->is_dynamic_array) {
                        buffer->print("uint32_t %s_count = *(uint32_t *)buf;\n", elem->name );
                        buffer->print("buf += sizeof(uint32_t);\n");
                        buffer->print("%s.resize(%s_count);\n", elem->name, elem->name);
                        buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name );
                    } else {
                        buffer->print("for(uint32_t i=0; i<%lu; i++) {\n", elem->array_suffix->size );
                    }
                    buffer->increase_ident();
                    buffer->print("uint32_t %s_length = *(uint32_t *)buf;\n", elem->name );
                    buffer->print("buf += sizeof(uint32_t);\n");
                    buffer->print("%s[i] = std::string(buf, %s_length);\n", elem->name, elem->name );
                    buffer->print("buf += %s_length;\n", elem->name);
                    buffer->decrease_ident();
                    buffer->print("}\n");
                }
            } else if (struct_type(elem, sym)) {
                buffer->print("%s.decode(buf, buf_size);\n", elem->name );
                buffer->print("buf += %s.preamble.size;\n", elem->name);
            } else if (enum_type(elem, sym)) {
                buffer->print("%s = *(%s *)buf;\n", elem->name, elem->custom_name);
                buffer->print("buf += sizeof(int32_t);\n");            
            } else if (elem->type == TYPE_STRING) {
                buffer->print("uint32_t %s_length = *(uint32_t *)buf;\n", elem->name );
                buffer->print("buf += sizeof(uint32_t);\n");
                buffer->print("%s = std::string(buf, %s_length);\n", elem->name, elem->name );
                buffer->print("buf += %s_length;\n", elem->name);
            } else {
                buffer->print("%s = *(%s *)buf;\n", elem->name, ElementTypeToStr[elem->type]);
                buffer->print("buf += sizeof(%s);\n", ElementTypeToStr[elem->type]);
            }
        }
        buffer->print("\n");
        buffer->print("return true;\n"); buffer->decrease_ident();
        buffer->print("}\n\n"); 

        print_net(st);
    }

    buffer->decrease_ident();
    buffer->print("};\n\n");
}

void CPrinter::print(ast_channel *cn)
{
    buffer->print("struct %s {\n", cn->name); buffer->increase_ident();
    buffer->print("circular_buffer indices;\n");
    buffer->print("char* data = nullptr;\n");
    buffer->print("\n");
    buffer->print("// this function will open the channel, allocate memory, set the indices, and\n");
    buffer->print("// do any other needed initialization\n");
    buffer->print("bool open_channel(const std::string &topic_name);\n");
    buffer->print("\n");

    buffer->print("// This function will actually initialize the channel and put data on it\n");
    buffer->print("bool prod_init(int num_elems = 10)\n");
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("return false;\n");
    buffer->decrease_ident();
    buffer->print("}\n");
    buffer->print("\n");


    buffer->print("// Producer: get a pointer to a struct to fill\n");
    buffer->print("%s* prod_get_image()\n", cn->inner_struct);
    buffer->print("{\n"); buffer->increase_ident();
    buffer->print("// This call might block\n");
    buffer->print("unsigned int elem_index = indices.get_next_empty();\n");
    buffer->print("\n");
    buffer->print("%s* inner_data = (%s *)data;\n", cn->inner_struct, cn->inner_struct);
    buffer->print("return &inner_data[elem_index];\n"); buffer->decrease_ident();
    buffer->print("}\n"); 
    buffer->print("\n");

    buffer->print("// Producer: This function assumes that the image* previously returned will no longer be used\n");
    buffer->print("void prod_publish()\n");
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("indices.publish();\n");
    buffer->decrease_ident();
    buffer->print("}\n");
    buffer->print("\n");

    buffer->print("// Consumer: get a pointer to the next struct from the publisher\n");
    buffer->print("%s* cons_get_image()\n", cn->inner_struct);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("// This call might block\n");
    buffer->print("// TODO: how to handle multiple consumers?\n");
    buffer->print("unsigned int elem_index = indices.get_next_full();\n");
    buffer->print("\n");
    buffer->print("%s* inner_data = (%s *)data;\n", cn->inner_struct, cn->inner_struct);
    buffer->print("return &inner_data[elem_index];\n");
    buffer->decrease_ident();
    buffer->print("}\n");
    buffer->print("\n");

    buffer->print("// Consumer: This function assumes that the image* previously returned will no longer be used\n");
    buffer->print("void cons_release()\n");
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("indices.release();\n");
    buffer->decrease_ident();
    buffer->print("}\n");
    buffer->print("\n");

    buffer->print("// This function will do a resize (TO BE DONE\n");
    buffer->print("bool resize()\n");
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("return true;\n");
    buffer->decrease_ident();
    buffer->print("}\n");
    buffer->print("\n");

    buffer->decrease_ident();
    buffer->print("};\n\n");
}

void CPrinter::print(ast_enum *en)
{
    buffer->print("enum %s\n", en->name);
    buffer->print("{\n");
    buffer->increase_ident();
    for(auto el:en->elements) {
        buffer->print("%s,\n", el);
    }
    buffer->decrease_ident();
    buffer->print("};\n\n");
}

void CPrinter::print(ast_element *elem)
{
    buffer->print("");
    int old_ident = buffer->get_ident();
    buffer->set_ident(0);

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
    buffer->set_ident(old_ident);
}

void CPrinter::print(StringBuffer *buf, ast_global *top_ast, SymbolTable *symbols)
{
    buffer = buf;
    sym = symbols;

    buffer->print("#pragma once\n");
    buffer->print("#include \"cbuf_preamble.h\"\n");
    buffer->print("#include <stdint.h> // uint8_t and such\n");
    buffer->print("#include <string.h> // memcpy\n");
    buffer->print("#include <vector>   // std::vector\n");
    buffer->print("#include <string>   // std::string\n");
    buffer->print("\n");

    for(auto *en: top_ast->global_space.enums) {
        print(en);
    }

    for(auto *st: top_ast->global_space.structs) {
        print(st);
    }

    for (auto *cn : top_ast->global_space.channels) {
        print(cn);
    }

    for(auto *sp: top_ast->spaces) {
        print(sp);
    }

    buffer = nullptr;
    sym = nullptr;
}
