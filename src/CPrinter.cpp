#include "CPrinter.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "AstPrinter.h"
#include "FileData.h"
#include "ast.h"
#include "fileutils.h"

// clang-format off
static const char* ElementTypeToStrC[] = {
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
  "VString<15>",
  "bool"
};
// clang-format on

static bool simple_type(ElementType t) {
  if (t != TYPE_STRING && t != TYPE_CUSTOM) {
    return true;
  }
  return false;
}

static bool struct_type(const ast_element* elem, const SymbolTable* sym) {
  if (elem->type == TYPE_CUSTOM) {
    return sym->find_struct(elem) != nullptr;
  }
  return false;
}

static bool enum_type(const ast_element* elem, const SymbolTable* sym) {
  if (elem->type == TYPE_CUSTOM) {
    return sym->find_enum(elem) != nullptr;
  }
  return false;
}

static void print_ast_value(const ast_value* val, StdStringBuffer* buffer) {
  switch (val->valtype) {
    case VALTYPE_INTEGER:
      buffer->print_no("%zd", ssize_t(val->int_val));
      break;
    case VALTYPE_FLOAT:
      buffer->print_no("%f", val->float_val);
      break;
    case VALTYPE_BOOL:
      buffer->print_no("%s", val->bool_val ? "true" : "false");
      break;
    case VALTYPE_STRING:
      buffer->print_no("\"%s\"", val->str_val);
      break;
    case VALTYPE_IDENTIFIER:
      buffer->print_no("%s", val->str_val);
      break;
    case VALTYPE_ARRAY: {
      buffer->print_no("{");
      auto arr_val = static_cast<const ast_array_value*>(val);
      for (auto v = arr_val->values.begin(); v != arr_val->values.end(); ++v) {
        print_ast_value(*v, buffer);
        if (std::next(v) != arr_val->values.end()) {
          buffer->print_no(", ");
        }
      }
      buffer->print_no("}");
      break;
    }
    default:
      assert(false && "Unknown value type in print_ast_value");
      break;
  }
}

const char* get_str_for_elem_type(ElementType t) { return ElementTypeToStrC[t]; }

void CPrinter::print(ast_const* cst) {
  if (cst->type == TYPE_STRING) {
    buffer->print("constexpr const char * %s = \"%s\";\n", cst->name, cst->value.str_val);
  } else {
    buffer->print("constexpr %s %s = %s;\n", ElementTypeToStrC[cst->type], cst->name, cst->value.str_val);
  }
}

void CPrinter::print(ast_namespace* sp) {
  // Emit GCC and clang pragmas to disable -Wpacked
  buffer->print("#if defined(__clang__)\n");
  buffer->print("#pragma clang diagnostic push\n");
  buffer->print("#pragma clang diagnostic ignored \"-Wunaligned-access\"\n");
  buffer->print("#elif defined(__GNUC__)\n");
  buffer->print("#pragma GCC diagnostic push\n");
  buffer->print("#pragma GCC diagnostic ignored \"-Wpacked\"\n");
  buffer->print("#endif\n\n");

  buffer->print("namespace %s {\n", sp->name);
  buffer->increase_ident();

  for (auto* cst : sp->consts) {
    // effectively skip consts imported from other files
    if (cst->file && !strcmp(cst->file->getFilename(), main_file->getFilename())) {
      print(cst);
    }
  }
  buffer->print("\n");

  for (auto* en : sp->enums) {
    print(en);
  }

  for (auto* st : sp->structs) {
    print(st);
  }

  buffer->decrease_ident();
  buffer->print("}\n\n");

  // Emit GCC and clang pragmas to re-enable -Wpacked
  buffer->print("#if defined(__clang__)\n");
  buffer->print("#pragma clang diagnostic pop\n");
  buffer->print("#elif defined(__GNUC__)\n");
  buffer->print("#pragma GCC diagnostic pop\n");
  buffer->print("#endif\n\n");
}

void CPrinter::helper_print_array_suffix(ast_element* elem) {
  if (elem->is_dynamic_array) {
    buffer->print_no(" %s.size();\n", elem->name);
    buffer->print("ret_size += sizeof(uint32_t); // Encode the length of %s\n", elem->name);
  } else if (elem->is_compact_array) {
    // compact array
    buffer->print_no(" num_%s;\n", elem->name);
    buffer->print("ret_size += sizeof(uint32_t); // Encode the length of %s in the var num_%s\n", elem->name,
                  elem->name);
  } else {
    // plain simple static array
    buffer->print_no(" %" PRIu64 ";\n", elem->array_suffix->size);
  }
}

void CPrinter::print_net(ast_struct* st) {
  const char* naked_suffix = st->naked ? "_naked" : "";
  buffer->print("size_t encode_net_size() const\n");
  buffer->print("{\n");
  buffer->increase_ident();
  if (!st->naked) {
    buffer->print("size_t ret_size = sizeof(uint64_t)*3; //sizeof(preamble)\n");
  } else {
    buffer->print("size_t ret_size = 0;\n");
  }
  for (auto* elem : st->elements) {
    char custom_type[128];
    if (elem->type == TYPE_CUSTOM) {
      if (elem->namespace_name != nullptr) {
        sprintf(custom_type, "%s::%s", elem->namespace_name, elem->custom_name);
      } else {
        sprintf(custom_type, "%s", elem->custom_name);
      }
    }
    if (elem->array_suffix) {
      if (simple_type(elem->type)) {
        // All the elements have the same size, easy
        buffer->print("ret_size += sizeof(%s) *", ElementTypeToStrC[elem->type]);
        helper_print_array_suffix(elem);
      } else if (struct_type(elem, sym)) {
        // check if custom is simple or not
        auto* inner_st = sym->find_struct(elem);
        if (inner_st->simple && !inner_st->has_compact) {
          // if the inner struct has no compacting (all sizes are the same as the first element)
          buffer->print("ret_size += %s[0].encode_net_size() *", elem->name);
          helper_print_array_suffix(elem);
        } else {
          // We have to compute the size of each element one by one...
          if (elem->is_dynamic_array) {
            buffer->print("uint32_t num_%s = uint32_t(%s.size());\n", elem->name, elem->name);
            buffer->print("ret_size += sizeof(uint32_t); // Encode the length of the variable array %s\n",
                          elem->name);
          } else if (elem->is_compact_array) {
            // nothing to do here, num_<elem_name> already exists
            buffer->print("ret_size += sizeof(uint32_t); // Encode the length of %s in the var num_%s\n",
                          elem->name, elem->name);
          } else {
            buffer->print("uint32_t num_%s = %" PRIu64 ";\n", elem->name, elem->array_suffix->size);
          }
          // No need to encode elements on the static array case, we know them already
          buffer->print("for(int %s_index=0; %s_index < int(num_%s); %s_index++) {\n", elem->name, elem->name,
                        elem->name, elem->name);
          buffer->increase_ident();
          buffer->print("ret_size += %s[size_t(%s_index)].encode_net_size();\n", elem->name, elem->name);
          buffer->decrease_ident();
          buffer->print("}\n");
        }
      } else if (enum_type(elem, sym)) {
        // Enums are always s32
        buffer->print("ret_size += sizeof(int32_t) * ");
        helper_print_array_suffix(elem);
      } else if (elem->type == TYPE_CUSTOM) {
        buffer->print("ret_size += sizeof(%s) * ", custom_type);
        helper_print_array_suffix(elem);
      } else {
        assert(elem->type == TYPE_STRING);
        assert(elem->is_compact_array == false);  // not supported
        // We have to compute the size one by one...
        if (elem->is_dynamic_array) {
          buffer->print("ret_size += sizeof(uint32_t); // To account for the length of %s\n", elem->name);
        }
        buffer->print("for (auto &%s_elem: %s) {\n", elem->name, elem->name);
        buffer->increase_ident();
        buffer->print("ret_size += %s_elem.length() + 4;\n", elem->name);
        buffer->decrease_ident();
        buffer->print("}\n");
      }
    } else if (struct_type(elem, sym)) {
      buffer->print("ret_size += %s.encode_net_size();\n", elem->name);
    } else if (enum_type(elem, sym)) {
      buffer->print("ret_size += sizeof(int32_t);\n");
    } else if (elem->type == TYPE_STRING) {
      buffer->print("ret_size += %s.length() + 4;\n", elem->name);
    } else if (elem->type == TYPE_CUSTOM) {
      buffer->print("ret_size += sizeof(%s); // for %s\n", custom_type, elem->name);
    } else {
      buffer->print("ret_size += sizeof(%s); // for %s\n", ElementTypeToStrC[elem->type], elem->name);
    }
  }
  buffer->print("return ret_size;\n");
  buffer->decrease_ident();
  buffer->print("}\n\n");

  buffer->print("bool encode_net%s(char *data, unsigned int buf_size) const\n", naked_suffix);
  buffer->print("{\n");
  buffer->increase_ident();
  if (!st->naked) {
    buffer->print("preamble.setSize(uint32_t(encode_net_size()));\n");
    buffer->print("if (buf_size < preamble.size()) return false;\n");
    buffer->print("*reinterpret_cast<uint32_t *>(data) = CBUF_MAGIC;\n");
    buffer->print("data += sizeof(uint32_t);\n");
    buffer->print("*reinterpret_cast<uint32_t *>(data) = preamble.size_;\n");
    buffer->print("data += sizeof(uint32_t);\n");
    buffer->print("*reinterpret_cast<uint64_t *>(data) = preamble.hash;\n");
    buffer->print("data += sizeof(uint64_t);\n");
    buffer->print("*reinterpret_cast<double *>(data) = preamble.packet_timest;\n");
    buffer->print("data += sizeof(double);\n");
  }
  for (auto* elem : st->elements) {
    char custom_type[128];
    if (elem->type == TYPE_CUSTOM) {
      if (elem->namespace_name != nullptr) {
        sprintf(custom_type, "%s::%s", elem->namespace_name, elem->custom_name);
      } else {
        sprintf(custom_type, "%s", elem->custom_name);
      }
    }
    if (elem->array_suffix) {
      if (simple_type(elem->type)) {
        if (elem->is_dynamic_array) {
          buffer->print("*reinterpret_cast<uint32_t *>(data) = uint32_t(this->%s.size());\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("memcpy(data, &this->%s[0], this->%s.size()*sizeof(%s));\n", elem->name, elem->name,
                        ElementTypeToStrC[elem->type]);
          buffer->print("data += this->%s.size()*sizeof(%s);\n", elem->name, ElementTypeToStrC[elem->type]);
        } else if (elem->is_compact_array) {
          buffer->print("*reinterpret_cast<uint32_t *>(data) = num_%s;\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("memcpy(data, &this->%s[0], num_%s*sizeof(%s));\n", elem->name, elem->name,
                        ElementTypeToStrC[elem->type]);
          buffer->print("data += num_%s*sizeof(%s);\n", elem->name, ElementTypeToStrC[elem->type]);
        } else {
          // normal static array
          buffer->print("memcpy(data, this->%s, %d*sizeof(%s));\n", elem->name, (int)elem->array_suffix->size,
                        ElementTypeToStrC[elem->type]);
          buffer->print("data += %d*sizeof(%s);\n", (int)elem->array_suffix->size,
                        ElementTypeToStrC[elem->type]);
        }
      } else if (struct_type(elem, sym)) {
        auto* inner_st = sym->find_struct(elem);
        if (inner_st->simple && !inner_st->has_compact) {
          // if all the elements of inner_st have the same size
          if (elem->is_dynamic_array) {
            buffer->print("*reinterpret_cast<uint32_t *>(data) = uint32_t(this->%s.size());\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
            buffer->print("memcpy(data, &this->%s[0], %s.size()*this->%s[0].encode_net_size());\n",
                          elem->name, elem->name, elem->name);
            buffer->print("data += this->%s.size()*this->%s[0].encode_net_size();\n", elem->name, elem->name);
          } else if (elem->is_compact_array) {
            buffer->print("*reinterpret_cast<uint32_t *>(data) = num_%s;\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
            buffer->print("memcpy(data, &this->%s[0], num_%s*this->%s[0].encode_net_size());\n", elem->name,
                          elem->name, elem->name);
            buffer->print("data += num_%s*%s[0].encode_net_size();\n", elem->name, elem->name);
          } else {
            buffer->print("memcpy(data, this->%s, %d*this->%s[0].encode_net_size());\n", elem->name,
                          (int)elem->array_suffix->size, elem->name);
            buffer->print("data += %d*this->%s[0].encode_net_size();\n", (int)elem->array_suffix->size,
                          elem->name);
          }
        } else {
          if (elem->is_dynamic_array) {
            buffer->print("uint32_t num_%s = uint32_t(this->%s.size());\n", elem->name, elem->name);
            buffer->print("*reinterpret_cast<uint32_t *>(data) = num_%s;\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
          } else if (elem->is_compact_array) {
            buffer->print("*reinterpret_cast<uint32_t *>(data) = num_%s;\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
          } else {
            buffer->print("uint32_t num_%s = %" PRIu64 ";\n", elem->name, elem->array_suffix->size);
          }  // No need to encode the number of elements on the static array case, we know them already
          buffer->print("for(size_t %s_index=0; %s_index < num_%s; %s_index++) {\n", elem->name, elem->name,
                        elem->name, elem->name);
          buffer->increase_ident();
          buffer->print("auto& %s_elem = this->%s[%s_index];\n", elem->name, elem->name, elem->name);
          buffer->print("%s_elem.encode_net%s(data, buf_size);\n", elem->name,
                        inner_st->naked ? "_naked" : "");
          buffer->print("data += %s_elem.encode_net_size();\n", elem->name);
          buffer->decrease_ident();
          buffer->print("}\n");
        }
      } else if (enum_type(elem, sym)) {
        if (elem->is_dynamic_array) {
          buffer->print("*reinterpret_cast<uint32_t *>(data) = uint32_t(this->%s.size());\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("memcpy(data, &this->%s[0], this->%s.size()*sizeof(int32_t));\n", elem->name,
                        elem->name);
          buffer->print("data += this->%s.size()*sizeof(int32_t);\n", elem->name);
        } else if (elem->is_compact_array) {
          buffer->print("*reinterpret_cast<uint32_t *>(data) = num_%s;\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("memcpy(data, &this->%s[0], num_%s*sizeof(int32_t));\n", elem->name, elem->name);
          buffer->print("data += num_%s*sizeof(int32_t);\n", elem->name);
        } else {
          buffer->print("memcpy(data, this->%s, %d*sizeof(int32_t));\n", elem->name,
                        (int)elem->array_suffix->size);
          buffer->print("data += %d*sizeof(int32_t);\n", (int)elem->array_suffix->size);
        }
      } else if (elem->type == TYPE_CUSTOM) {
        if (elem->is_dynamic_array) {
          buffer->print("*reinterpret_cast<uint32_t *>(data) = uint32_t(this->%s.size());\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("memcpy(data, &this->%s[0], this->%s.size()*sizeof(%s));\n", elem->name, elem->name,
                        custom_type);
          buffer->print("data += this->%s.size()*sizeof(%s);\n", elem->name, custom_type);
        } else if (elem->is_compact_array) {
          buffer->print("*reinterpret_cast<uint32_t *>(data) = num_%s;\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("memcpy(data, &this->%s[0], num_%s*sizeof(%s));\n", elem->name, elem->name,
                        custom_type);
          buffer->print("data += num_%s*sizeof(%s);\n", elem->name, custom_type);
        } else {
          buffer->print("memcpy(data, this->%s, %d*sizeof(int32_t));\n", elem->name,
                        (int)elem->array_suffix->size);
          buffer->print("data += %d*sizeof(int32_t);\n", (int)elem->array_suffix->size);
        }
        buffer->print("ret_size += sizeof(this->%s); // for %s\n", custom_type, elem->name);
      } else {
        assert(elem->type == TYPE_STRING);
        assert(elem->is_compact_array == false);

        if (elem->is_dynamic_array) {
          buffer->print("*reinterpret_cast<uint32_t *>(data) = uint32_t(this->%s.size());\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
        }  // No need to encode elements on the static array case, we know them already
        buffer->print("for(auto &%s_elem: %s) {\n", elem->name, elem->name);
        buffer->increase_ident();
        buffer->print("*reinterpret_cast<uint32_t *>(data) = uint32_t(%s_elem.length());\n", elem->name);
        buffer->print("data += sizeof(uint32_t);\n");
        buffer->print("memcpy(data, %s_elem.c_str(), %s_elem.length());\n", elem->name, elem->name);
        buffer->print("data += %s_elem.length();\n", elem->name);
        buffer->decrease_ident();
        buffer->print("}\n");
      }
    } else if (struct_type(elem, sym)) {
      auto* inner_st = sym->find_struct(elem);
      buffer->print("%s.encode_net%s(data, buf_size);\n", elem->name, inner_st->naked ? "_naked" : "");
      buffer->print("data += %s.encode_net_size();\n", elem->name);
    } else if (enum_type(elem, sym)) {
      // this is an enum, treat it as an s32;
      buffer->print("*reinterpret_cast<int32_t *>(data) = int32_t(this->%s);\n", elem->name);
      buffer->print("data += sizeof(int32_t);\n");
    } else if (elem->type == TYPE_STRING) {
      buffer->print("*reinterpret_cast<uint32_t *>(data) = uint32_t(this->%s.length());\n", elem->name);
      buffer->print("data += sizeof(uint32_t);\n");
      buffer->print("memcpy(data, this->%s.c_str(), this->%s.length());\n", elem->name, elem->name);
      buffer->print("data += this->%s.length();\n", elem->name);
    } else if (elem->type == TYPE_CUSTOM) {
      buffer->print("*reinterpret_cast<%s *>(data) = this->%s;\n", custom_type, elem->name);
      buffer->print("data += sizeof(%s);\n", custom_type);
    } else {
      buffer->print("*reinterpret_cast<%s *>(data) = this->%s;\n", ElementTypeToStrC[elem->type], elem->name);
      buffer->print("data += sizeof(%s);\n", ElementTypeToStrC[elem->type]);
    }
  }
  buffer->print("\n");
  buffer->print("return true;\n");
  buffer->decrease_ident();
  buffer->print("}\n\n");

  buffer->print("bool decode_net%s(char *data, unsigned int buf_size)\n", naked_suffix);
  buffer->print("{\n");
  buffer->increase_ident();
  if (!st->naked) {
    buffer->print("uint32_t magic = *reinterpret_cast<uint32_t *>(data);\n");
    buffer->print("if (magic != CBUF_MAGIC) CBUF_RETURN_FALSE();\n");
    buffer->print("preamble.magic = magic;\n");
    buffer->print("data += sizeof(uint32_t);\n");
    buffer->print("uint32_t dec_size = *reinterpret_cast<uint32_t *>(data);\n");
    buffer->print("preamble.size_ = dec_size;\n");
    buffer->print("if (preamble.size() > buf_size) CBUF_RETURN_FALSE();\n");
    buffer->print("data += sizeof(uint32_t);\n");
    buffer->print("uint64_t buf_hash = *reinterpret_cast<uint64_t *>(data);\n");
    buffer->print("if (buf_hash != TYPE_HASH) CBUF_RETURN_FALSE();\n");
    buffer->print("data += sizeof(uint64_t);\n");
    buffer->print("preamble.packet_timest = *reinterpret_cast<double *>(data);\n");
    buffer->print("data += sizeof(double);\n");
  }

  for (auto* elem : st->elements) {
    char custom_type[128];
    if (elem->type == TYPE_CUSTOM) {
      if (elem->namespace_name != nullptr) {
        sprintf(custom_type, "%s::%s", elem->namespace_name, elem->custom_name);
      } else {
        sprintf(custom_type, "%s", elem->custom_name);
      }
    }
    if (elem->array_suffix) {
      if (simple_type(elem->type)) {
        if (elem->is_dynamic_array) {
          buffer->print("uint32_t %s_count = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("this->%s.resize(%s_count);\n", elem->name, elem->name);
          buffer->print("memcpy(&this->%s[0], data, %s_count*sizeof(%s));\n", elem->name, elem->name,
                        ElementTypeToStrC[elem->type]);
          buffer->print("data += %s_count*sizeof(%s);\n", elem->name, ElementTypeToStrC[elem->type]);
        } else if (elem->is_compact_array) {
          buffer->print("num_%s = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("if (num_%s > sizeof(this->%s)/sizeof(%s)) CBUF_RETURN_FALSE();\n", elem->name,
                        elem->name, ElementTypeToStrC[elem->type]);
          buffer->print("memcpy(this->%s, data, num_%s*sizeof(%s));\n", elem->name, elem->name,
                        ElementTypeToStrC[elem->type]);
          buffer->print("data += num_%s*sizeof(%s);\n", elem->name, ElementTypeToStrC[elem->type]);
        } else {
          buffer->print("memcpy(this->%s, data, %d*sizeof(%s));\n", elem->name, (int)elem->array_suffix->size,
                        ElementTypeToStrC[elem->type]);
          buffer->print("data += %d*sizeof(%s);\n", (int)elem->array_suffix->size,
                        ElementTypeToStrC[elem->type]);
        }
      } else if (struct_type(elem, sym)) {
        auto* inner_st = sym->find_struct(elem);
        if (inner_st->simple && !inner_st->has_compact) {
          if (elem->is_dynamic_array) {
            buffer->print("uint32_t %s_count = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
            buffer->print("this->%s.resize(%s_count);\n", elem->name, elem->name);
            buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name);
            buffer->increase_ident();
            buffer->print("if (!this->%s[i].decode_net%s(data, buf_size)) CBUF_RETURN_FALSE();\n", elem->name,
                          inner_st->naked ? "_naked" : "");
            if (inner_st->naked) {
              buffer->print("data += this->%s[i].encode_net_size();\n", elem->name);
            } else {
              buffer->print("data += this->%s[i].preamble.size();\n", elem->name);
            }
            buffer->decrease_ident();
            buffer->print("}\n");
          } else if (elem->is_compact_array) {
            // @warning: if the substruct has compact arrays too, this won't work
            // since each element can have a different size
            buffer->print("num_%s = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
            buffer->print("memcpy(this->%s, data, num_%s*this->%s[0].encode_net_size());\n", elem->name,
                          elem->name, elem->name);
            buffer->print("data += num_%s*this->%s[0].encode_net_size();\n", elem->name, elem->name);
          } else {
            buffer->print("memcpy(this->%s, data, %d*this->%s[0].encode_net_size());\n", elem->name,
                          (int)elem->array_suffix->size, elem->name);
            buffer->print("data += %d*this->%s[0].encode_net_size();\n", (int)elem->array_suffix->size,
                          elem->name);
          }
        } else {
          if (elem->is_dynamic_array) {
            buffer->print("uint32_t num_%s = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
            buffer->print("this->%s.resize(num_%s);\n", elem->name, elem->name);
          } else if (elem->is_compact_array) {
            buffer->print("num_%s = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
            buffer->print("data += sizeof(uint32_t);\n");
          } else {  // No need to decode the number of elements on the static array case, we know them already
            buffer->print("uint32_t num_%s = %" PRIu64 ";\n", elem->name, elem->array_suffix->size);
          }
          buffer->print("for(uint32_t i=0; i<num_%s; i++) {\n", elem->name);
          buffer->increase_ident();
          buffer->print("if (!this->%s[i].decode_net%s(data, buf_size)) CBUF_RETURN_FALSE();\n", elem->name,
                        inner_st->naked ? "_naked" : "");
          if (inner_st->naked) {
            buffer->print("data += this->%s[i].encode_net_size();\n", elem->name);
          } else {
            buffer->print("data += this->%s[i].preamble.size();\n", elem->name);
          }
          buffer->decrease_ident();
          buffer->print("}\n");
        }
      } else if (enum_type(elem, sym)) {
        if (elem->is_dynamic_array) {
          buffer->print("uint32_t %s_count = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("this->%s.resize(%s_count);\n", elem->name, elem->name);
          buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name);
          buffer->increase_ident();
          buffer->print("this->%s[i] = *reinterpret_cast<%s *>(data);\n", elem->name, custom_type);
          buffer->print("data += sizeof(int32_t);\n");
          buffer->decrease_ident();
          buffer->print("}\n");
        } else if (elem->is_compact_array) {
          buffer->print("num_%s = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("if (num_%s > sizeof(this->%s)/sizeof(int32_t)) CBUF_RETURN_FALSE();\n", elem->name,
                        elem->name);
          buffer->print("memcpy(this->%s, data, num_%s*sizeof(int32_t));\n", elem->name, elem->name);
          buffer->print("data += num_%s*sizeof(int32_t);\n", elem->name);
        } else {
          buffer->print("memcpy(this->%s, data, %d*sizeof(int32_t));\n", elem->name,
                        (int)elem->array_suffix->size);
          buffer->print("data += %" PRIu64 "*sizeof(int32_t);\n", elem->array_suffix->size);
        }
      } else if (elem->type == TYPE_CUSTOM) {
        if (elem->is_dynamic_array) {
          buffer->print("uint32_t %s_count = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("this->%s.resize(%s_count);\n", elem->name, elem->name);
          buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name);
          buffer->increase_ident();
          buffer->print("if (!this->%s[i].decode_net(data, buf_size)) CBUF_RETURN_FALSE();\n", elem->name);
          buffer->print("data += this->%s[i].preamble.size();\n", elem->name);
          buffer->decrease_ident();
          buffer->print("}\n");
        } else if (elem->is_compact_array) {
          buffer->print("num_%s = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("memcpy(this->%s, data, num_%s*this->%s[0].encode_net_size());\n", elem->name,
                        elem->name, elem->name);
          buffer->print("data += num_%s*this->%s[0].encode_net_size();\n", elem->name, elem->name);
        } else {  // @warning: what happens here when the encode net size is not the same on each elem?
          buffer->print("memcpy(this->%s, data, %d*this->%s[0].encode_net_size());\n", elem->name,
                        (int)elem->array_suffix->size, elem->name);
          buffer->print("data += %" PRIu64 "*this->%s[0].encode_net_size();\n", elem->array_suffix->size,
                        elem->name);
        }
      } else {
        assert(elem->type == TYPE_STRING);
        assert(elem->is_compact_array == false);  // not supported yet

        if (elem->is_dynamic_array) {
          buffer->print("uint32_t %s_count = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
          buffer->print("data += sizeof(uint32_t);\n");
          buffer->print("this->%s.resize(%s_count);\n", elem->name, elem->name);
          buffer->print("for(uint32_t i=0; i<%s_count; i++) {\n", elem->name);
        } else {
          buffer->print("for(uint32_t i=0; i<%d; i++) {\n", (int)elem->array_suffix->size);
        }
        buffer->increase_ident();
        buffer->print("uint32_t %s_length = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
        buffer->print("data += sizeof(uint32_t);\n");
        buffer->print("this->%s[i].assign(data, %s_length);\n", elem->name, elem->name);
        buffer->print("data += %s_length;\n", elem->name);
        buffer->decrease_ident();
        buffer->print("}\n");
      }
    } else if (struct_type(elem, sym)) {
      auto* inner_st = sym->find_struct(elem);
      buffer->print("if (!this->%s.decode_net%s(data, buf_size)) CBUF_RETURN_FALSE();\n", elem->name,
                    inner_st->naked ? "_naked" : "");
      if (inner_st->naked) {
        buffer->print("data += this->%s.encode_net_size();\n", elem->name);
      } else {
        buffer->print("data += this->%s.preamble.size();\n", elem->name);
      }
    } else if (enum_type(elem, sym)) {
      buffer->print("this->%s = *reinterpret_cast<%s *>(data);\n", elem->name, custom_type);
      buffer->print("data += sizeof(int32_t);\n");
    } else if (elem->type == TYPE_STRING) {
      buffer->print("uint32_t %s_length = *reinterpret_cast<uint32_t *>(data);\n", elem->name);
      buffer->print("data += sizeof(uint32_t);\n");
      buffer->print("this->%s.assign(data, %s_length);\n", elem->name, elem->name);
      buffer->print("data += %s_length;\n", elem->name);
    } else if (elem->type == TYPE_CUSTOM) {
      buffer->print("this->%s = *reinterpret_cast<%s *>(data);\n", elem->name, custom_type);
      buffer->print("data += sizeof(%s);\n", custom_type);
    } else {
      buffer->print("this->%s = *reinterpret_cast<%s *>(data);\n", elem->name, ElementTypeToStrC[elem->type]);
      buffer->print("data += sizeof(%s);\n", ElementTypeToStrC[elem->type]);
    }
  }
  buffer->print("\n");
  buffer->print("return true;\n");
  buffer->decrease_ident();
  buffer->print("}\n\n");
}

void CPrinter::print(ast_struct* st) {
  if (st->file != main_file) return;

  buffer->print("#pragma pack(push, 1)\n");
  buffer->print("struct %s {\n", st->name);
  buffer->increase_ident();

  if (st->naked) {
    buffer->print("// There is no preamble, this is a naked struct\n\n");
  } else {
    buffer->print("// This has to be the first member\n");
    buffer->print("mutable cbuf_preamble preamble = {\n");
    buffer->increase_ident();
    buffer->print("CBUF_MAGIC,\n");
    if (st->simple)
      buffer->print("sizeof(%s),\n", st->name);
    else
      buffer->print("0,\n");
    buffer->print("0x%" PRIX64 ",\n", st->hash_value);
    buffer->print("0.0,\n");
    buffer->print("};\n");
    buffer->decrease_ident();
  }

  buffer->print("bool operator==(const %s&) const = default;\n", st->name);

  for (auto* elem : st->elements) {
    print(elem);
  }

  if (!st->naked) {
    buffer->print("/// This is here to ensure hash is always available, just in case.\n");
    buffer->print("static const uint64_t TYPE_HASH = 0x%" PRIX64 ";\n", st->hash_value);
    buffer->print("static constexpr uint64_t hash() { return TYPE_HASH; }\n");
    buffer->print("static constexpr const char* TYPE_STRING = \"");
    if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buffer->print_no("%s::", st->space->name);
    buffer->print_no("%s\";\n", st->name);
  }
  buffer->print("static constexpr bool is_simple() { return %s; }\n", (st->simple ? "true" : "false"));
  buffer->print("static constexpr bool supports_compact() { return %s; }\n",
                (st->has_compact ? "true" : "false"));
  buffer->print("\n");

  buffer->print("void Init()\n");
  buffer->print("{\n");
  buffer->increase_ident();
  for (auto* elem : st->elements) {
    printInit(elem);
  }
  buffer->decrease_ident();
  buffer->print("}\n\n");

  buffer->print("static void handle_metadata(cbuf_metadata_fn fn, void *ctx)\n");
  buffer->print("{\n");
  buffer->increase_ident();
  if (!st->naked) {
    buffer->print("(*fn)(cbuf_string, hash(), TYPE_STRING, ctx);\n");
  }
  for (auto* elem : st->elements) {
    if (struct_type(elem, sym)) {
      buffer->print("");
      if (elem->namespace_name) {
        buffer->print_no("%s::", elem->namespace_name);
      }
      if (elem->custom_name) {
        buffer->print_no("%s", elem->custom_name);
      } else {
        buffer->print_no("%s", ElementTypeToStrC[elem->type]);
      }
      buffer->print_no("::handle_metadata(fn, ctx);\n");
    }
  }
  buffer->decrease_ident();
  buffer->print("}\n\n");

  const char* naked_suffix = st->naked ? "_naked" : "";
  // Encode and decode
  if (st->simple) {
    buffer->print("size_t encode_size() const\n");
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("return sizeof(%s);\n", st->name);
    buffer->decrease_ident();
    buffer->print("}\n\n");
    buffer->print("void free_encode(const char *) const {}\n\n");
    buffer->print("bool encode%s(char *data, unsigned int buf_size) const\n", naked_suffix);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("if (buf_size < sizeof(%s)) return false;\n", st->name);
    buffer->print("memcpy(data, this, sizeof(*this));\n");
    buffer->print("return true;\n");
    buffer->decrease_ident();
    buffer->print("}\n\n");
    buffer->print("// This variant allows for no copy when writing to disk.\n");
    buffer->print("const char *encode%s() const\n", naked_suffix);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("return reinterpret_cast<const char *>(this);\n");
    buffer->decrease_ident();
    buffer->print("}\n\n");
    buffer->print("bool decode%s(char *data, unsigned int buf_size)\n", naked_suffix);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("if (buf_size < sizeof(%s)) return false;\n", st->name);
    if (!st->naked) {
      buffer->print("cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);\n");
      buffer->print("if (pre->hash != TYPE_HASH) return false;\n");
    }
    buffer->print("memcpy(this, data, sizeof(*this));\n");
    buffer->print("return true;\n");
    buffer->decrease_ident();
    buffer->print("}\n\n");
    buffer->print("static bool decode%s(char *data, unsigned int buf_size, %s** var)\n", naked_suffix,
                  st->name);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("if (buf_size < sizeof(%s)) return false;\n", st->name);
    if (!st->naked) {
      buffer->print("cbuf_preamble *pre = reinterpret_cast<cbuf_preamble *>(data);\n");
      buffer->print("if (pre->hash != TYPE_HASH) return false;\n");
    }
    buffer->print("*var = reinterpret_cast<%s *>(data);\n", st->name);
    buffer->print("return true;\n");
    buffer->decrease_ident();
    buffer->print("}\n\n");
    print_net(st);
  } else {
    buffer->print("size_t encode_size() const\n");
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("return encode_net_size();\n");
    buffer->decrease_ident();
    buffer->print("}\n\n");

    buffer->print("void free_encode(char *p) const\n");
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("free(p);\n");
    buffer->decrease_ident();
    buffer->print("}\n\n");

    buffer->print("bool encode%s(char *data, unsigned int buf_size) const\n", naked_suffix);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("return encode_net%s(data, buf_size);\n", naked_suffix);
    buffer->decrease_ident();
    buffer->print("}\n\n");

    buffer->print("char *encode%s() const\n", naked_suffix);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("size_t __struct_size = encode_size();\n");
    if (!st->naked) {
      buffer->print("preamble.setSize(uint32_t(__struct_size));\n");
    }
    buffer->print("char *data = reinterpret_cast<char *>(malloc(__struct_size));\n");
    buffer->print("encode%s(data, __struct_size);\n", naked_suffix);
    buffer->print("return data;\n");
    buffer->decrease_ident();
    buffer->print("}\n\n");

    buffer->print("bool decode%s(char *data, unsigned int buf_size)\n", naked_suffix);
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("return decode_net%s(data, buf_size);\n", naked_suffix);
    buffer->decrease_ident();
    buffer->print("}\n\n");

    print_net(st);
  }

  AstPrinter astPrinter;
  StdStringBuffer buf;
  astPrinter.setSymbolTable(sym);
  astPrinter.print_ast(&buf, st);
  buffer->print("static constexpr const char * cbuf_string = R\"CBUF_CODE(\n%s)CBUF_CODE\";\n\n",
                buf.get_buffer());
  buffer->decrease_ident();
  buffer->print("};\n");
  buffer->print("#pragma pack(pop)\n\n");
}

void CPrinter::print(ast_enum* en) {
  if (en->file != main_file) return;

  buffer->print("enum %s%s\n", en->is_class ? "class " : "", en->name);
  buffer->print("{\n");
  buffer->increase_ident();
  for (auto el : en->elements) {
    if (el.item_assigned) {
      buffer->print("%s = %zd,\n", el.item_name, ssize_t(el.item_value));
    } else {
      buffer->print("%s,\n", el.item_name);
    }
  }
  buffer->decrease_ident();
  buffer->print("};\n\n");
}

void CPrinter::printInit(ast_element* elem) {
  if (elem->is_compact_array) {
    buffer->print("num_%s = 0;\n", elem->name);
  }

  ast_array_definition* ar = elem->array_suffix;

  if (ar && ar->size == 0 && elem->is_dynamic_array) {
    // dynamic array case
    buffer->print("%s.clear();\n", elem->name);
  } else if (ar && (ar->size > 0)) {
    // static arrays (compact and not too)
    buffer->print("for(int %s_index = 0; %s_index < %zu; %s_index++) {\n", elem->name, elem->name,
                  size_t(ar->size), elem->name);
    buffer->increase_ident();
    buffer->print("%s[%s_index]", elem->name, elem->name);
    if (struct_type(elem, sym)) {
      buffer->print_no(".Init();\n");
    } else if (enum_type(elem, sym)) {
      buffer->print_no(" = %s(0);\n", elem->custom_name);
    } else if (elem->type == TYPE_STRING) {
      buffer->print_no(" = \"\";\n");
    } else if (elem->type == TYPE_SHORT_STRING) {
      buffer->print_no(" = \"\";\n");
    } else if (elem->type == TYPE_BOOL) {
      buffer->print_no(" = false;\n");
    } else {
      buffer->print_no(" = 0;\n");
    }
    buffer->decrease_ident();
    buffer->print("}\n");
  } else {
    // single element case
    buffer->print("%s", elem->name);

    if (elem->init_value != nullptr) {
      auto& val = elem->init_value;
      switch (val->valtype) {
        case VALTYPE_INTEGER:
          buffer->print_no(" = %zd;\n", ssize_t(val->int_val));
          break;
        case VALTYPE_FLOAT:
          buffer->print_no(" = %f;\n", val->float_val);
          break;
        case VALTYPE_BOOL:
          buffer->print_no(" = %s;\n", val->bool_val ? "true" : "false");
          break;
        case VALTYPE_STRING:
          buffer->print_no(" = \"%s\";\n", val->str_val);
          break;
        case VALTYPE_IDENTIFIER:
          buffer->print_no(" = %s;\n", val->str_val);
          break;
        default:
          break;
      }
    } else if (struct_type(elem, sym)) {
      buffer->print_no(".Init();\n");
    } else if (enum_type(elem, sym)) {
      buffer->print_no(" = %s(0);\n", elem->custom_name);
    } else if (elem->type == TYPE_STRING) {
      buffer->print_no("= \"\";\n");
    } else if (elem->type == TYPE_SHORT_STRING) {
      buffer->print_no("= \"\";\n");
    } else if (elem->type == TYPE_BOOL) {
      buffer->print_no("= false;\n");
    } else {
      buffer->print_no("= 0;\n");
    }
  }
}

void CPrinter::print(ast_element* elem) {
  if (elem->is_compact_array) {
    buffer->print("uint32_t num_%s = 0;\n", elem->name);
  }
  buffer->print("");
  int old_ident = buffer->get_ident();
  buffer->set_ident(0);

  ast_array_definition* ar = elem->array_suffix;
  bool close_array = false;

  if (ar && ar->size == 0 && elem->is_dynamic_array) {
    buffer->print("std::vector< ");
    close_array = true;
  }
  if (elem->namespace_name) {
    buffer->print("%s::", elem->namespace_name);
  }
  if (elem->custom_name) {
    buffer->print("%s ", elem->custom_name);
  } else {
    buffer->print("%s ", ElementTypeToStrC[elem->type]);
  }
  if (close_array) {
    buffer->print("> ");
  }
  buffer->print("%s", elem->name);
  while (ar != nullptr) {
    if (ar->size != 0) buffer->print("[%" PRIu64 "]", ar->size);
    ar = ar->next;
  }
  if (elem->init_value != nullptr) {
    auto& val = elem->init_value;
    buffer->print(" = ");
    print_ast_value(val, buffer);
  }
  buffer->print(";\n");
  buffer->set_ident(old_ident);
}

static void convertCbufToH(char* out, const char* in) {
  while (*in) {
    if (*in == '.') {
      *out++ = *in++;
      *out++ = 'h';
      *out = 0;
      break;
    }
    *out++ = *in++;
  }
}

void CPrinter::print(StdStringBuffer* buf, ast_global* top_ast, SymbolTable* symbols) {
  buffer = buf;
  sym = symbols;
  main_file = top_ast->main_file;

  buffer->print("#pragma once\n");
  buffer->print("#include \"cbuf_preamble.h\"\n");
  buffer->print("#include <stdint.h> // uint8_t and such\n");
  buffer->print("#include <string.h> // memcpy\n");
  buffer->print("#include <vector>   // std::vector\n");
  buffer->print("#include <string>   // std::string\n");
  buffer->print("#include \"vstring.h\"\n");
  buffer->print("\n");

  for (auto imp_file : top_ast->imported_files) {
    char head_buf[256];
    convertCbufToH(head_buf, imp_file);
    buffer->print("#include \"%s\"\n", head_buf);
  }
  buffer->print("\n");

  for (auto* cst : top_ast->consts) {
    // effectively skip consts imported from other files
    if (cst->file && !strcmp(cst->file->getFilename(), top_ast->main_file->getFilename())) {
      print(cst);
    }
  }
  buffer->print("\n");

  for (auto* en : top_ast->global_space.enums) {
    print(en);
  }

  for (auto* st : top_ast->global_space.structs) {
    print(st);
  }

  for (auto* sp : top_ast->spaces) {
    print(sp);
  }

  buffer = nullptr;
  sym = nullptr;
  main_file = nullptr;
}

void CPrinter::printLoaderDeclaration(ast_namespace* sp) {
  for (auto* st : sp->structs) {
    printLoaderDeclaration(st);
  }
}

void CPrinter::printLoaderDeclaration(ast_struct* st) {
  buffer->print("#if !defined(_JSON_DECLARATION_%s_)\n", st->name);
  buffer->print("#define _JSON_DECLARATION_%s_\n", st->name);
  buffer->print("template<>\n");
  buffer->print("  void loadFromJson(const Hjson::Value& json, ");
  if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buffer->print_no("%s::", st->space->name);
  buffer->print_no("%s& obj);\n", st->name);
  buffer->print("#endif // _JSON_DECLARATION_%s_\n", st->name);
}

void CPrinter::printLoader(ast_element* elem) {
  buffer->print("do { // Loading %s\n", elem->name);
  buffer->increase_ident();

  if (elem->array_suffix) {
    if (elem->is_dynamic_array) {
      // dynamic array
      buffer->print("const Hjson::Value& vec_%s = json[\"%s\"];\n", elem->name, elem->name);
      buffer->print("if (!vec_%s.defined()) break;\n", elem->name);
      buffer->print("obj.%s.resize(vec_%s.size());\n", elem->name, elem->name);
      buffer->print("for( int %s_index=0; %s_index < vec_%s.size(); %s_index++) {\n", elem->name, elem->name,
                    elem->name, elem->name);
    } else if (elem->is_compact_array) {
      buffer->print("if (!json[\"%s\"].defined()) break;\n", elem->name);
      buffer->print("obj.num_%s = json[\"%s\"].size();\n", elem->name, elem->name);
      buffer->print("for( int %s_index=0; %s_index < obj.num_%s; %s_index++) {\n", elem->name, elem->name,
                    elem->name, elem->name);
    } else {
      buffer->print("if (!json[\"%s\"].defined()) break;\n", elem->name);
      buffer->print("uint32_t num_%s = %" PRIu64 ";\n", elem->name, elem->array_suffix->size);
      buffer->print("for( int %s_index=0; %s_index < num_%s; %s_index++) {\n", elem->name, elem->name,
                    elem->name, elem->name);
    }

    // static array
    buffer->increase_ident();
    buffer->print("const Hjson::Value& jelem = json[\"%s\"][%s_index];\n", elem->name, elem->name);
    buffer->print("auto& elem = obj.%s[%s_index];\n", elem->name, elem->name);
    if (struct_type(elem, sym)) {
      buffer->print("loadFromJson(jelem, elem);\n");
      buffer->decrease_ident();
      buffer->print("}\n");
      buffer->decrease_ident();
      buffer->print("} while(0);\n");
      return;
    } else if (enum_type(elem, sym)) {
      buffer->print("{\n");
      buffer->increase_ident();
      buffer->print("int %s_int;\n", elem->name);
      buffer->print("if (get_value_int(jelem, %s_int)) {\n", elem->name);
      buffer->increase_ident();
      buffer->print("obj.%s[%s_index] = ", elem->name, elem->name);
      if (strcmp(elem->enclosing_struct->space->name, GLOBAL_NAMESPACE))
        buffer->print_no("%s::", elem->enclosing_struct->space->name);
      buffer->print_no("%s(%s_int);\n", elem->custom_name, elem->name);
      buffer->print("}\n");  // close if
      buffer->decrease_ident();
      buffer->print("}\n");  // close local block
      buffer->decrease_ident();
      buffer->print("}\n");  // close for loop
      buffer->decrease_ident();
      buffer->print("} while(0);\n");
      return;
    }

    switch (elem->type) {
      case TYPE_U8:
      case TYPE_U16:
      case TYPE_U32:
      case TYPE_U64:
        buffer->print("get_value_uint(jelem, elem);\n");
        break;
      case TYPE_S8:
      case TYPE_S16:
      case TYPE_S32:
      case TYPE_S64:
        buffer->print("get_value_int(jelem, elem);\n");
        break;
      case TYPE_F32:
        buffer->print("get_value_float(jelem, elem);\n");
        break;
      case TYPE_F64:
        buffer->print("get_value_double(jelem, elem);\n");
        break;
      case TYPE_STRING:
        buffer->print("get_value_string(jelem, elem);\n");
        break;
      case TYPE_SHORT_STRING:
        buffer->print("{\n");
        buffer->increase_ident();
        buffer->print("std::string tmp;\n");
        buffer->print("if (get_value_string(jelem, tmp)) {\n");
        buffer->increase_ident();
        buffer->print("elem = tmp;\n");
        buffer->decrease_ident();
        buffer->print("}\n");
        buffer->decrease_ident();
        buffer->print("}\n");
        break;
      case TYPE_BOOL:
        buffer->print("get_value_bool(jelem, elem);\n");
        break;
      case TYPE_CUSTOM:
        buffer->print("// NOT SURE WHAT TO PUT HERE for %s\n", elem->name);
        break;
    }

    buffer->decrease_ident();
    buffer->print("}\n");
  } else if (struct_type(elem, sym)) {
    // this is a struct
    buffer->print("loadFromJson(json[\"%s\"], obj.%s);\n", elem->name, elem->name);
  } else if (enum_type(elem, sym)) {
    buffer->print("{\n");
    buffer->increase_ident();
    buffer->print("int %s_int;\n", elem->name);
    buffer->print("if (get_member_int(json, \"%s\", %s_int)) {\n", elem->name, elem->name);
    buffer->increase_ident();
    buffer->print("obj.%s = ", elem->name);
    if (strcmp(elem->enclosing_struct->space->name, GLOBAL_NAMESPACE))
      buffer->print_no("%s::", elem->enclosing_struct->space->name);
    buffer->print_no("%s(%s_int);\n", elem->custom_name, elem->name);
    buffer->decrease_ident();
    buffer->print("}\n");
    buffer->decrease_ident();
    buffer->print("}\n");
  } else {
    switch (elem->type) {
      case TYPE_U8:
      case TYPE_U16:
      case TYPE_U32:
      case TYPE_U64:
        buffer->print("get_member_uint(json, \"%s\", obj.%s);\n", elem->name, elem->name);
        break;
      case TYPE_S8:
      case TYPE_S16:
      case TYPE_S32:
      case TYPE_S64:
        buffer->print("get_member_int(json, \"%s\", obj.%s);\n", elem->name, elem->name);
        break;
      case TYPE_F32:
        buffer->print("get_member_float(json, \"%s\", obj.%s);\n", elem->name, elem->name);
        break;
      case TYPE_F64:
        buffer->print("get_member_double(json, \"%s\", obj.%s);\n", elem->name, elem->name);
        break;
      case TYPE_STRING:
        buffer->print("get_member_string(json, \"%s\", obj.%s);\n", elem->name, elem->name);
        break;
      case TYPE_SHORT_STRING:
        buffer->print("{\n");
        buffer->increase_ident();
        buffer->print("std::string tmp;\n");
        buffer->print("if (get_member_string(json, \"%s\", tmp)) {\n", elem->name);
        buffer->increase_ident();
        buffer->print("obj.%s = tmp;\n", elem->name);
        buffer->decrease_ident();
        buffer->print("}\n");
        buffer->decrease_ident();
        buffer->print("}\n");
        break;
      case TYPE_BOOL:
        buffer->print("get_member_bool_relaxed(json, \"%s\", obj.%s);\n", elem->name, elem->name);
        break;
      case TYPE_CUSTOM:
        buffer->print("// NOT SURE WHAT TO PUT HERE for %s\n", elem->name);
        break;
    }
  }
  buffer->decrease_ident();
  buffer->print("} while(0);\n");
}

void CPrinter::printLoader(ast_namespace* sp) {
  for (auto* st : sp->structs) {
    printLoader(st);
    buffer->print("\n");
  }
  buffer->print("\n");
  buffer->print("\n");
}

void CPrinter::printLoader(ast_struct* st) {
  buffer->print("#if !defined(_JSON_IMPLEMENTATION_%s_)\n", st->name);
  buffer->print("#define _JSON_IMPLEMENTATION_%s_\n", st->name);

  if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buffer->print_no("namespace %s {\n", st->space->name);
  buffer->print_no("struct %s;\n", st->name);
  if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buffer->print("}\n");
  buffer->print("template<>\n");
  buffer->print("inline void loadFromJson(const Hjson::Value& json, ");
  if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buffer->print_no("%s::", st->space->name);
  buffer->print_no("%s& obj)\n", st->name);
  buffer->print("{\n");
  buffer->increase_ident();

  for (auto& elem : st->elements) {
    printLoader(elem);
  }

  buffer->decrease_ident();
  buffer->print("}\n");
  buffer->print("#endif // _JSON_IMPLEMENTATION_%s_\n", st->name);
}

static void convertCbufToJsonH(char* out, const char* in) {
  while (*in) {
    if (*in == '.') {
      *out++ = '_';
      *out++ = 'j';
      *out++ = 's';
      *out++ = 'o';
      *out++ = 'n';
      *out++ = *in++;
      *out++ = 'h';
      *out = 0;
      break;
    }
    *out++ = *in++;
  }
}

void CPrinter::printLoader(StdStringBuffer* buf, ast_global* top_ast, SymbolTable* symbols,
                           const char* c_name) {
  buffer = buf;
  sym = symbols;
  main_file = top_ast->main_file;

  buffer->print("#pragma once\n");
  buffer->print("#include <hjson.h>\n");
  buffer->print("#include <toolbox/hjson_helper.h> // this header has json helpers\n");
  buffer->print("// Please include all the required cbuf C headers before this file\n");
  buffer->print("#include \"%s.h\"\n", c_name);
  buffer->print("template<typename T>\n");
  buffer->print("void loadFromJson(const Hjson::Value& json, T&obj);\n");
  buffer->print("\n");

  for (auto imp_file : top_ast->imported_files) {
    char head_buf[256];
    convertCbufToJsonH(head_buf, imp_file);
    buffer->print("#include \"%s\"\n", head_buf);
  }
  buffer->print("\n");

  for (auto* st : top_ast->global_space.structs) {
    printLoaderDeclaration(st);
  }

  for (auto* sp : top_ast->spaces) {
    printLoaderDeclaration(sp);
  }

  buffer->print("\n");
  buffer->print("\n");
  buffer->print("\n");

  for (auto* st : top_ast->global_space.structs) {
    printLoader(st);
  }

  for (auto* sp : top_ast->spaces) {
    printLoader(sp);
  }

  buffer = nullptr;
  sym = nullptr;
  main_file = nullptr;
}

bool CPrinter::printDepfile(StdStringBuffer* buf, ast_global* top_ast, Array<const char*>& incs,
                            std::string& error, const char* c_name, const char* outfile) {
  buffer = buf;

  buffer->print("%s : %s ", outfile, c_name);
  for (int i = 0; i < top_ast->imported_files.size(); i++) {
    std::string p = getCanonicalPath(top_ast->imported_files[i]);
    if (p.empty()) {
      // we could not find an imported file, search on include paths
      char ipbuf[512];
      for (int ip = 0; ip < incs.size(); ip++) {
        snprintf(ipbuf, sizeof(ipbuf), "%s/%s", incs[ip], top_ast->imported_files[i]);
        p = getCanonicalPath(ipbuf);
        if (!p.empty()) break;
      }
      if (p.empty()) {
        error = "Include file " + std::string(top_ast->imported_files[i]) + " could not be found!";
        return false;
      }
    }
    buffer->print("\\\n  %s ", p.c_str());
  }
  buffer->print("\n");

  error.clear();
  return true;
}
