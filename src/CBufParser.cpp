#include "CBufParser.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <cmath>

#include "Interp.h"
#include "Parser.h"
#include "SymbolTable.h"
#include "cbuf_preamble.h"

template <class T>
std::string to_string(T val) {
  return std::to_string(val);
}

template <>
std::string to_string(float val) {
  if (std::isnan(val)) {
    return "NaN";
  }
  return std::to_string(val);
}

template <>
std::string to_string(double val) {
  if (std::isnan(val)) {
    return "NaN";
  }
  return std::to_string(val);
}

void insert_with_quotes(std::string& str, const char* s, size_t size) {
  for (size_t i = 0; i < size; i++) {
    if (s[i] == 0) return;
    if (s[i] == '"' || s[i] == '\'') {
      str += '\\';
    }
    str += s[i];
  }
}

template <class T>
void print(T elem) {
  printf("%d", elem);
}

template <>
void print<u32>(u32 elem) {
  printf("%u", elem);
}

template <>
void print<u64>(u64 elem) {
  printf("%" PRIu64, elem);
}

template <>
void print<s64>(s64 elem) {
  printf("%" PRId64, elem);
}

template <>
void print<f64>(f64 elem) {
  printf("%.18f", elem);

  // Use this if you want the binary double
  // printf("0x%" PRIx64, *(u64 *)(double *)&elem);
}

template <>
void print<f32>(f32 elem) {
  printf("%.10f", elem);
}

template <class T>
void process_element_jstr(const ast_element* elem, u8*& bin_buffer, size_t& buf_size, std::string& jstr) {
  T val;
  if (elem->array_suffix) {
    assert(elem->type != TYPE_CUSTOM);
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":[";

    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else if (elem->is_compact_array) {
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }
    for (int i = 0; i < array_size; i++) {
      val = *(T*)bin_buffer;
      bin_buffer += sizeof(val);
      buf_size -= sizeof(val);
      if (i > 0) {
        jstr += ",";
      }
      jstr += ::to_string(val);
    }
    jstr += "]";
  } else {
    // This is a single element, not an array
    val = *(T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":";
    jstr += ::to_string(val);
  }
}

void process_element_string_jstr(const ast_element* elem, u8*& bin_buffer, size_t& buf_size,
                                 std::string& jstr) {
  if (elem->array_suffix) {
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":[";

    u32 array_size;
    if (elem->is_dynamic_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else if (elem->is_compact_array) {
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size;
      // This is a dynamic array
      str_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(str_size);
      buf_size -= sizeof(str_size);
      if (i > 0) {
        jstr += ",";
      }
      jstr += "\"";
      insert_with_quotes(jstr, (char*)bin_buffer, str_size);
      jstr += "\"";

      bin_buffer += str_size;
      buf_size -= str_size;
    }
    jstr += "]";
    return;
  }

  jstr += "\"";
  jstr += elem->name;
  jstr += "\":\"";

  // This is a string
  u32 str_size;
  // This is a dynamic array
  str_size = *(u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  insert_with_quotes(jstr, (char*)bin_buffer, str_size);
  jstr += "\"";
  bin_buffer += str_size;
  buf_size -= str_size;
}

void process_element_short_string_jstr(const ast_element* elem, u8*& bin_buffer, size_t& buf_size,
                                       std::string& jstr) {
  char str[16];
  if (elem->array_suffix) {
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":[";

    u32 array_size;
    if (elem->is_dynamic_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else if (elem->is_compact_array) {
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size = 16;
      if (i > 0) {
        jstr += ",";
      }
      jstr += "\"";
      memcpy(str, bin_buffer, str_size);
      jstr += str;
      jstr += "\"";

      bin_buffer += str_size;
      buf_size -= str_size;
    }
    jstr += "]";
    return;
  }

  jstr += "\"";
  jstr += elem->name;
  jstr += "\":\"";

  // This is a static string
  u32 str_size = 16;
  memcpy(str, bin_buffer, str_size);
  jstr += str;
  jstr += "\"";
  bin_buffer += str_size;
  buf_size -= str_size;
}

#if defined(HJSON_PRESENT)
template <class T>
void process_element_json(const ast_element* elem, u8*& bin_buffer, size_t& buf_size, Hjson::Value& jval) {
  T val;
  if (elem->array_suffix) {
    assert(elem->type != TYPE_CUSTOM);

    // This is an array
    Hjson::Value arr(Hjson::Value::VECTOR);

    u32 array_size;
    if (elem->is_dynamic_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else if (elem->is_compact_array) {
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }
    for (int i = 0; i < array_size; i++) {
      val = *(T*)bin_buffer;
      bin_buffer += sizeof(val);
      buf_size -= sizeof(val);
      arr.push_back((double)val);
    }
    jval[(const char*)elem->name] = arr;
  } else {
    // This is a single element, not an array
    val = *(T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    jval[(const char*)elem->name] = (double)val;
  }
}

void process_element_string_json(const ast_element* elem, u8*& bin_buffer, size_t& buf_size,
                                 Hjson::Value& jval) {
  char* str;
  if (elem->array_suffix) {
    Hjson::Value arr(Hjson::Value::VECTOR);

    u32 array_size;
    if (elem->is_dynamic_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else if (elem->is_compact_array) {
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size;
      // This is a dynamic array
      str_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(str_size);
      buf_size -= sizeof(str_size);
      str = new char[str_size + 1];
      memset(str, 0, str_size + 1);
      memcpy(str, bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;
      arr.push_back(str);
    }
    jval[(const char*)elem->name] = arr;
    return;
  }
  // This is an array
  u32 str_size;
  // This is a dynamic array
  str_size = *(u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  str = new char[str_size + 1];
  memset(str, 0, str_size + 1);
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  jval[(const char*)elem->name] = str;

  delete[] str;
}

void process_element_short_string_json(const ast_element* elem, u8*& bin_buffer, size_t& buf_size,
                                       Hjson::Value& jval) {
  if (elem->array_suffix) {
    Hjson::Value arr(Hjson::Value::VECTOR);

    u32 array_size;
    if (elem->is_dynamic_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else if (elem->is_compact_array) {
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size = 16;
      char str[16];
      memcpy(str, bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;
      arr.push_back(str);
    }
    jval[(const char*)elem->name] = arr;
    return;
  }

  // This is s static array
  u32 str_size = 16;
  char str[16];
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  jval[(const char*)elem->name] = str;
}

#endif

template <class T>
void process_element(const ast_element* elem, u8*& bin_buffer, size_t& buf_size, const std::string& prefix) {
  T val;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }
    if (array_size > 1000) {
      printf("%s%s[%d] = ...\n", prefix.c_str(), elem->name, array_size);
      bin_buffer += sizeof(val) * array_size;
      buf_size -= sizeof(val) * array_size;
    } else {
      if (elem->is_dynamic_array || elem->is_compact_array)
        printf("%snum_%s = %d\n", prefix.c_str(), elem->name, array_size);
      printf("%s%s[%d] = ", prefix.c_str(), elem->name, array_size);
      for (int i = 0; i < array_size; i++) {
        val = *(T*)bin_buffer;
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        print(val);
        if (i < array_size - 1) printf(", ");
      }
      printf("\n");
    }
  } else {
    // This is a single element, not an array
    val = *(T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    printf("%s%s: ", prefix.c_str(), elem->name);
    print(val);
    printf("\n");
  }
}

template <class T>
void process_element_csv(const ast_element* elem, u8*& bin_buffer, size_t& buf_size) {
  T val;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }
    if (array_size > 5) {
      bin_buffer += sizeof(val) * array_size;
      buf_size -= sizeof(val) * array_size;
    } else {
      for (int i = 0; i < array_size; i++) {
        val = *(T*)bin_buffer;
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        print(val);
        if (i < array_size - 1) printf(",");
      }
    }
  } else {
    // This is a single element, not an array
    val = *(T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    print(val);
  }
}

void process_element_string(const ast_element* elem, u8*& bin_buffer, size_t& buf_size,
                            const std::string& prefix) {
  char* str;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size;
      // This is a dynamic array
      str_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(str_size);
      buf_size -= sizeof(str_size);
      str = new char[str_size + 1];
      memset(str, 0, str_size + 1);
      memcpy(str, bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;

      printf("%s%s[%d] = [ %s ]\n", prefix.c_str(), elem->name, i, str);
      delete[] str;
    }
    return;
  }
  // This is an array
  u32 str_size;
  // This is a dynamic array
  str_size = *(u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  str = new char[str_size + 1];
  memset(str, 0, str_size + 1);
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  printf("%s%s = [ %s ]\n", prefix.c_str(), elem->name, str);
  delete[] str;
}

void process_element_string_csv(const ast_element* elem, u8*& bin_buffer, size_t& buf_size) {
  char* str;
  if (elem->array_suffix) {
    assert(false);
    // Array of strings not implemented for csv. What would the header be? what size of array
    // of strings, given that we would have many messages with different sizes?
  }
  // This is an array
  u32 str_size;
  // This is a dynamic array
  str_size = *(u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  str = new char[str_size + 1];
  memset(str, 0, str_size + 1);
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  printf("%s", str);
  delete[] str;
}

void process_element_short_string(const ast_element* elem, u8*& bin_buffer, size_t& buf_size,
                                  const std::string& prefix) {
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size = 16;
      char str[16];
      memcpy(str, bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;

      printf("%s%s[%d] = [] %s ]\n", prefix.c_str(), elem->name, i, str);
    }
    return;
  }

  // This is s static array
  u32 str_size = 16;
  char str[16];
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  printf("%s%s = [ %s ]\n", prefix.c_str(), elem->name, str);
}

void process_element_short_string_csv(const ast_element* elem, u8*& bin_buffer, size_t& buf_size) {
  if (elem->array_suffix) {
    assert(false);
    // Array of strings not implemented
  }

  // This is s static array
  u32 str_size = 16;
  char str[16];
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  printf("%s", str);
}

template <typename T>
void loop_all_structs(ast_global* ast, SymbolTable* symtable, Interp* interp, T func) {
  for (auto* sp : ast->spaces) {
    for (auto* st : sp->structs) {
      func(st, symtable, interp);
    }
  }

  for (auto* st : ast->global_space.structs) {
    func(st, symtable, interp);
  }
}

bool compute_simple(ast_struct* st, SymbolTable* symtable, Interp* interp) {
  if (st->simple_computed) return st->simple;
  st->simple = true;
  for (auto* elem : st->elements) {
    if (elem->type == TYPE_STRING) {
      st->simple = false;
      st->simple_computed = true;
      return false;
    }
    if (elem->is_dynamic_array) {
      // all dynamic arrays are always not simple!
      st->simple = false;
      st->simple_computed = true;
      return false;
    }
    if (elem->type == TYPE_CUSTOM) {
      if (!symtable->find_symbol(elem)) {
        interp->Error(elem, "Struct %s, element %s was referencing type %s and could not be found\n",
                      st->name, elem->name, elem->custom_name);
        return false;
      }
      auto* inner_st = symtable->find_struct(elem);
      if (inner_st == nullptr) {
        // Must be an enum, it is simple
        continue;
      }
      bool elem_simple = compute_simple(inner_st, symtable, interp);
      if (!elem_simple) {
        st->simple = false;
        st->simple_computed = true;
        return false;
      }
    }
  }
  st->simple_computed = true;
  return true;
}

CBufParser::CBufParser() { pool = new PoolAllocator(); }

CBufParser::~CBufParser() {
  if (sym) {
    delete sym;
    sym = nullptr;
  }

  if (pool) {
    delete pool;
    pool = nullptr;
  }
}

bool CBufParser::ParseMetadata(const std::string& metadata, const std::string& struct_name) {
  Parser parser;
  Interp interp;

  parser.interp = &interp;
  ast = parser.ParseBuffer(metadata.c_str(), metadata.size() - 1, pool, nullptr);
  if (ast == nullptr || !parser.success) {
    fprintf(stderr, "Error during parsing:\n%s\n", interp.getErrorString());
    return false;
  }

  sym = new SymbolTable;
  bool bret = sym->initialize(ast);
  if (!bret) {
    fprintf(stderr, "Error during symbol table parsing:\n%s\n", interp.getErrorString());
    return false;
  }

  loop_all_structs(ast, sym, &interp, compute_simple);

  if (interp.has_error()) {
    fprintf(stderr, "Parsing error: %s\n", interp.getErrorString());
    return false;
  }

  main_struct_name = struct_name;
  return true;
}

bool CBufParser::FillJstrInternal(const ast_struct* st, std::string& val) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    if (elem_idx > 0) {
      val += ",";
    }
    switch (elem->type) {
      case TYPE_U8: {
        process_element_jstr<u8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U16: {
        process_element_jstr<u16>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U32: {
        process_element_jstr<u32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U64: {
        process_element_jstr<u64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S8: {
        process_element_jstr<s8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S16: {
        process_element_jstr<s16>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S32: {
        process_element_jstr<s32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S64: {
        process_element_jstr<s64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_F32: {
        process_element_jstr<f32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_F64: {
        process_element_jstr<f64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_BOOL: {
        process_element_jstr<u8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_STRING: {
        process_element_string_jstr(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_SHORT_STRING: {
        process_element_short_string_jstr(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_CUSTOM: {
        auto* inst = sym->find_struct(elem);
        if (inst != nullptr) {
          if (elem->array_suffix != nullptr) {
            int array_size;
            if (elem->is_dynamic_array) {
              array_size = *(u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
            } else if (elem->is_compact_array) {
              array_size = *(u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
              std::string num_name = "\"num_";
              num_name += elem->name;
              val += num_name + "\":" + ::to_string(array_size) + ",";
            } else {
              array_size = elem->array_suffix->size;
            }
            val += "\"";
            val += elem->name;
            val += "\":[";
            for (int a_idx = 0; a_idx < array_size; a_idx++) {
              if (a_idx > 0) {
                val += ",";
              }
              val += "{";
              FillJstrInternal(inst, val);
              if (val.back() == ',') val.pop_back();
              val += "}";
            }
            val += "]";
          } else {
            val += "\"";
            val += elem->name;
            val += "\":{";
            FillJstrInternal(inst, val);
            if (val.back() == ',') val.pop_back();
            val += "}";
          }
        } else {
          auto* enm = sym->find_enum(elem);
          if (enm == nullptr) {
            fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
            return false;
          }
          process_element_jstr<u32>(elem, buffer, buf_size, val);
        }
        break;
      }
    }
  }
  return true;
}

unsigned int CBufParser::FillJstr(const char* st_name, unsigned char* buffer, size_t buf_size,
                                  std::string& jstr) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  jstr += "{";
  if (!FillJstrInternal(decompose_and_find(st_name), jstr)) {
    return 0;
  }
  if (jstr.back() == ',') jstr.pop_back();
  jstr += "}";
  // val["cbuf_message_type"] = st_name;
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}

#if defined(HJSON_PRESENT)

bool CBufParser::FillHjsonInternal(const ast_struct* st, Hjson::Value& val) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    switch (elem->type) {
      case TYPE_U8: {
        process_element_json<u8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U16: {
        process_element_json<u16>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U32: {
        process_element_json<u32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U64: {
        process_element_json<u64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S8: {
        process_element_json<s8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S16: {
        process_element_json<s16>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S32: {
        process_element_json<s32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S64: {
        process_element_json<s64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_F32: {
        process_element_json<f32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_F64: {
        process_element_json<f64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_BOOL: {
        process_element_json<u8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_STRING: {
        process_element_string_json(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_SHORT_STRING: {
        process_element_short_string_json(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_CUSTOM: {
        auto* inst = sym->find_struct(elem);
        if (inst != nullptr) {
          if (elem->array_suffix != nullptr) {
            int array_size;
            if (elem->is_dynamic_array) {
              array_size = *(u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
            } else if (elem->is_compact_array) {
              array_size = *(u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
              std::string num_name = "num_";
              num_name += elem->name;
              val[num_name.c_str()] = array_size;
            } else {
              array_size = elem->array_suffix->size;
            }
            Hjson::Value arr(Hjson::Value::VECTOR);
            for (int a_idx = 0; a_idx < array_size; a_idx++) {
              Hjson::Value inner;
              FillHjsonInternal(inst, inner);
              arr.push_back(inner);
            }
            val[(const char*)elem->name] = arr;
          } else {
            Hjson::Value inner;
            FillHjsonInternal(inst, inner);
            val[(const char*)elem->name] = inner;
          }
        } else {
          auto* enm = sym->find_enum(elem);
          if (enm == nullptr) {
            fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
            return false;
          }
          process_element_json<u32>(elem, buffer, buf_size, val);
        }
        break;
      }
    }
  }
  return true;
}

unsigned int CBufParser::FillHjson(const char* st_name, unsigned char* buffer, size_t buf_size,
                                   Hjson::Value& val, bool print_body) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  Hjson::Value body;
  if (!FillHjsonInternal(decompose_and_find(st_name), body)) {
    return 0;
  }
  if (print_body) {
    val["message_body"] = body;
  } else {
    val = body;
  }
  val["cbuf_message_type"] = st_name;
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}
#endif

bool CBufParser::PrintCSVHeaderInternal(const ast_struct* st, const std::string& prefix) {
  if (st == nullptr) return false;

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    if (elem->array_suffix) {
      if (elem->is_dynamic_array) {
        printf("ERROR: Dynamic arrays are not supported on CSV mode\n");
        return false;
      }
      // In the compact array case, have a header will the fields, later they'll be empty
      int array_size = elem->array_suffix->size;
      if (array_size <= 100) {
        for (int i = 0; i < array_size; i++) {
          if (elem->type == TYPE_CUSTOM) {
            auto* inst = sym->find_struct(elem);
            if (inst == nullptr) {
              printf("ERROR, struct %s could not be found\n", elem->custom_name);
              return false;
            }
            std::string prefix_new = prefix + std::string(elem->name) + "[" + ::to_string(i) + "].";
            bool bret = PrintCSVHeaderInternal(inst, prefix_new);
            if (!bret) return false;
          } else {
            printf("%s%s[%d]", prefix.c_str(), elem->name, i);
          }
          if (i + 1 < array_size) printf(",");
        }
      }
    } else {
      if (elem->type == TYPE_CUSTOM) {
        auto* inst = sym->find_struct(elem);
        if (inst == nullptr) {
          printf("ERROR, struct %s could not be found\n", elem->custom_name);
          return false;
        }
        std::string prefix_new = prefix + std::string(elem->name) + ".";
        bool bret = PrintCSVHeaderInternal(inst, prefix_new);
        if (!bret) return false;
      } else {
        printf("%s%s", prefix.c_str(), elem->name);
      }
    }
    if (elem_idx + 1 < st->elements.size()) printf(",");
  }
  return true;
}

bool CBufParser::PrintCSVHeader() {
  ast_struct* st = decompose_and_find(main_struct_name.c_str());
  if (st == nullptr) {
    fprintf(stderr, "Could not find struct %s on the symbol table\n", main_struct_name.c_str());
    return false;
  }

  PrintCSVHeaderInternal(st, "");

  printf("\n");
  return true;
}

// This function is doing filling for compact arrays in CSV
bool CBufParser::PrintCSVInternalEmpty(const ast_struct* st) {
  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    switch (elem->type) {
      case TYPE_CUSTOM: {
        if (elem->array_suffix) {
          // This is an array
          u32 array_size = array_size = elem->array_suffix->size;
          assert(!elem->is_dynamic_array);

          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            for (u32 i = 0; i < array_size; i++) {
              PrintCSVInternalEmpty(inst);
              if (i + 1 < array_size) printf(",");
            }
          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            for (u32 i = 0; i < array_size; i++) {
              if (i + 1 < array_size) printf(",");
            }
          }
        } else {
          // This is a single element, not an array
          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            PrintCSVInternal(inst);
          }
        }
        break;
      }
      default: {
        // Nothing to do for empty case
        break;
      }
    }
    if (elem_idx + 1 < st->elements.size()) printf(",");
  }
  return true;
}

bool CBufParser::PrintCSVInternal(const ast_struct* st) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    switch (elem->type) {
      case TYPE_U8: {
        process_element_csv<u8>(elem, buffer, buf_size);
        break;
      }
      case TYPE_U16: {
        process_element_csv<u16>(elem, buffer, buf_size);
        break;
      }
      case TYPE_U32: {
        process_element_csv<u32>(elem, buffer, buf_size);
        break;
      }
      case TYPE_U64: {
        process_element_csv<u64>(elem, buffer, buf_size);
        break;
      }
      case TYPE_S8: {
        process_element_csv<s8>(elem, buffer, buf_size);
        break;
      }
      case TYPE_S16: {
        process_element_csv<s16>(elem, buffer, buf_size);
        break;
      }
      case TYPE_S32: {
        process_element_csv<s32>(elem, buffer, buf_size);
        break;
      }
      case TYPE_S64: {
        process_element_csv<s64>(elem, buffer, buf_size);
        break;
      }
      case TYPE_F32: {
        process_element_csv<f32>(elem, buffer, buf_size);
        break;
      }
      case TYPE_F64: {
        process_element_csv<f64>(elem, buffer, buf_size);
        break;
      }
      case TYPE_BOOL: {
        process_element_csv<u8>(elem, buffer, buf_size);
        break;
      }
      case TYPE_STRING: {
        process_element_string_csv(elem, buffer, buf_size);
        break;
      }
      case TYPE_SHORT_STRING: {
        process_element_short_string_csv(elem, buffer, buf_size);
        break;
      }
      case TYPE_CUSTOM: {
        if (elem->array_suffix) {
          // This is an array
          u32 array_size;
          if (elem->is_dynamic_array || elem->is_compact_array) {
            // This is a dynamic array
            array_size = *(u32*)buffer;
            buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
          } else {
            // this is a static array
            array_size = elem->array_suffix->size;
          }

          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            for (u32 i = 0; i < array_size; i++) {
              PrintCSVInternal(inst);
              if (i + 1 < array_size) printf(",");
            }
            if (elem->is_compact_array) {
              if (array_size < elem->array_suffix->size) {
                printf(",");
              }
              for (u32 i = array_size; i < elem->array_suffix->size; i++) {
                PrintCSVInternalEmpty(inst);
                if (i + 1 < elem->array_suffix->size) printf(",");
              }
            }

          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            for (u32 i = 0; i < array_size; i++) {
              process_element_csv<u32>(elem, buffer, buf_size);
              if (i + 1 < array_size) printf(",");
            }
            if (elem->is_compact_array) {
              if (array_size < elem->array_suffix->size) {
                printf(",");
              }
              for (u32 i = array_size; i < elem->array_suffix->size; i++) {
                if (i + 1 < elem->array_suffix->size) printf(",");
              }
            }
          }

        } else {
          // This is a single element, not an array
          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            PrintCSVInternal(inst);
          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            process_element_csv<u32>(elem, buffer, buf_size);
          }
        }

        break;
      }
    }
    if (elem_idx + 1 < st->elements.size()) printf(",");
  }
  return true;
}

bool CBufParser::PrintInternal(const ast_struct* st, const std::string& prefix) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  for (auto& elem : st->elements) {
    switch (elem->type) {
      case TYPE_U8: {
        process_element<u8>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_U16: {
        process_element<u16>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_U32: {
        process_element<u32>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_U64: {
        process_element<u64>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S8: {
        process_element<s8>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S16: {
        process_element<s16>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S32: {
        process_element<s32>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S64: {
        process_element<s64>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_F32: {
        process_element<f32>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_F64: {
        process_element<f64>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_BOOL: {
        process_element<u8>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_STRING: {
        process_element_string(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_SHORT_STRING: {
        process_element_short_string(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_CUSTOM: {
        if (elem->array_suffix) {
          // This is an array
          u32 array_size;
          if (elem->is_dynamic_array || elem->is_compact_array) {
            // This is a dynamic array
            array_size = *(u32*)buffer;
            buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
          } else {
            // this is a static array
            array_size = elem->array_suffix->size;
          }
          if (elem->is_compact_array) {
            printf("%snum_%s = %d\n", prefix.c_str(), elem->name, array_size);
          }

          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            for (u32 i = 0; i < array_size; i++) {
              std::string new_prefix = prefix + elem->name + "[" + ::to_string(i) + "].";
              PrintInternal(inst, new_prefix);
            }
          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            for (u32 i = 0; i < array_size; i++) {
              process_element<u32>(elem, buffer, buf_size, prefix);
            }
          }

        } else {
          // This is a single element, not an array
          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            std::string new_prefix = prefix + elem->name + ".";
            PrintInternal(inst, new_prefix);
          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            process_element<u32>(elem, buffer, buf_size, prefix);
          }
        }

        break;
      }
    }
  }
  return true;
}

ast_struct* CBufParser::decompose_and_find(const char* st_name) {
  char namesp[128] = {};
  auto* sep = strchr(st_name, ':');
  if (sep == nullptr) {
    auto tname = CreateTextType(pool, st_name);
    return sym->find_struct(tname);
  }

  for (int i = 0; st_name[i] != ':'; i++) namesp[i] = st_name[i];
  auto tname = CreateTextType(pool, sep + 2);
  return sym->find_struct(tname, namesp);
}

// Returns the number of bytes consumed
unsigned int CBufParser::Print(const char* st_name, unsigned char* buffer, size_t buf_size) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  std::string prefix = std::string(st_name) + ".";
  if (!PrintInternal(decompose_and_find(st_name), prefix)) {
    return 0;
  }
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}

// Print only the data, no header information, and CSV friendly
unsigned int CBufParser::PrintCSV(const char* st_name, unsigned char* buffer, size_t buf_size) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  if (!PrintCSVInternal(decompose_and_find(st_name))) {
    return 0;
  }
  printf("\n");
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}
