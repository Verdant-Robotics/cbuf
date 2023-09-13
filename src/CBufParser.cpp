#include "CBufParser.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <cmath>
// Vector is here only for conversions
#include <vector>

#include "Interp.h"
#include "Parser.h"
#include "StdStringBuffer.h"
#include "SymbolTable.h"
#include "cbuf_preamble.h"
#include "computefuncs.h"
#include "vstring.h"

static bool computeSizes(ast_struct* st, SymbolTable* symtable);
const char* get_str_for_elem_type(ElementType t);

// Compute basic element type size, does not take into account arrays
static bool computeElementTypeSize(ast_element* elem, SymbolTable* symtable, u32& csize) {
  switch (elem->type) {
    case TYPE_BOOL:
    case TYPE_U8:
    case TYPE_S8:
      csize = 1;
      break;
    case TYPE_U16:
    case TYPE_S16:
      csize = 2;
      break;
    case TYPE_F32:
    case TYPE_U32:
    case TYPE_S32:
      csize = 4;
      break;
    case TYPE_F64:
    case TYPE_U64:
    case TYPE_S64:
      csize = 8;
      break;
    case TYPE_STRING:
      csize = (u32)sizeof(std::string);
      break;
    case TYPE_SHORT_STRING:
      // Short string is always 16 chars
      csize = 16;
      break;
    case TYPE_CUSTOM: {
      if (symtable->find_enum(elem) != nullptr) {
        csize = 4;
      } else {
        auto* inner_st = symtable->find_struct(elem);
        if (!inner_st) return false;
        bool bret = computeSizes(inner_st, symtable);
        if (!bret) return false;
        csize = inner_st->csize;
      }
    }
  }
  return true;
}

// This function assumes packed structs. If packing is left to default,
// this would be not right
static bool computeSizes(ast_struct* st, SymbolTable* symtable) {
  if (st->csize > 0) return true;
  if (!st->naked) {
    // All structs have the preamble if not naked
    st->csize = sizeof(cbuf_preamble);
  }

  for (auto* elem : st->elements) {
    u32 csize;
    if (!computeElementTypeSize(elem, symtable, csize)) {
      return false;
    }
    if (elem->array_suffix) {
      // Do not try to support multi dimensional arrays!
      if (elem->array_suffix->next != nullptr) {
        fprintf(stderr, "Found a non supported multidimensional array at elem %s\n", elem->name);
        return false;
      }
      // if it is an array, dynamic arrays are std::vector
      if (elem->is_dynamic_array) {
        elem->csize = sizeof(std::vector<size_t>);
        elem->typesize = 0;
      } else {
        u32 num_elem_size = elem->is_compact_array ? 4 : 0;  // for the num_elements
        elem->csize = num_elem_size + elem->array_suffix->size * csize;
        elem->typesize = csize;
      }
    } else {
      elem->csize = csize;
      elem->typesize = csize;
    }
    elem->coffset = st->csize;
    st->csize += elem->csize;
  }
  return true;
}

static ast_element* find_elem_by_name(const ast_struct* dst_st, const char* elem_name) {
  for (auto* elem : dst_st->elements) {
    if (!strcmp(elem->name, elem_name)) {
      return elem;
    }
  }
  return nullptr;
}

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

static bool processArraySize(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                             u32& array_size) {
  array_size = 1;
  if (elem->array_suffix) {
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      array_size = *(u32*)bin_buffer;
      bin_buffer += sizeof(array_size);
      buf_size -= sizeof(array_size);
    } else {
      // this is a static array
      array_size = elem->array_suffix->size;
    }
    if (elem->is_compact_array && array_size > elem->array_suffix->size) {
      return false;
    }
  }
  return true;
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
bool process_element_jstr(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                          std::string& jstr) {
  T val;
  u32 array_size;
  if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
    return false;
  }

  if (elem->array_suffix) {
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":[";

    assert(elem->type != TYPE_CUSTOM);
    for (int i = 0; i < array_size; i++) {
      val = *(const T*)bin_buffer;
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
    val = *(const T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":";
    jstr += ::to_string(val);
  }
  return true;
}

static bool match_elem_name(const char* elem_name, const char* ename, const char** nextname) {
  const char* s1 = elem_name;
  const char* s2 = ename;
  // if we have no ename, everything matches
  if (s2 == nullptr) return true;
  while (*s1 != 0 && *s2 != 0) {
    if (*s2 == '.') return false;
    if (*s1 != *s2) return false;
    s1++;
    s2++;
  }
  if (*s1 == 0 && *s2 == '.') {
    if (nextname != nullptr) *nextname = s2 + 1;
    return true;
  }
  if (*s1 == 0 && *s2 == 0) return true;
  return false;
}

bool process_element_string_jstr(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                 std::string& jstr) {
  u32 array_size;
  if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
    return false;
  }

  if (elem->array_suffix) {
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":[";

    for (int i = 0; i < array_size; i++) {
      u32 str_size;
      // This is a dynamic array
      str_size = *(const u32*)bin_buffer;
      bin_buffer += sizeof(str_size);
      buf_size -= sizeof(str_size);
      if (i > 0) {
        jstr += ",";
      }
      jstr += "\"";
      insert_with_quotes(jstr, (const char*)bin_buffer, str_size);
      jstr += "\"";

      bin_buffer += str_size;
      buf_size -= str_size;
    }
    jstr += "]";
    return true;
  }

  jstr += "\"";
  jstr += elem->name;
  jstr += "\":\"";

  // This is a string
  u32 str_size;
  // This is a dynamic array
  str_size = *(const u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  insert_with_quotes(jstr, (const char*)bin_buffer, str_size);
  jstr += "\"";
  bin_buffer += str_size;
  buf_size -= str_size;
  return true;
}

bool process_element_short_string_jstr(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                       std::string& jstr) {
  char str[16];
  u32 array_size;
  if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
    return false;
  }
  if (elem->array_suffix) {
    jstr += "\"";
    jstr += elem->name;
    jstr += "\":[";

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
    return true;
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
  return true;
}

#if defined(HJSON_PRESENT)
template <class T>
bool process_element_json(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                          Hjson::Value& jval, bool dofill) {
  T val;
  u32 array_size;
  if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
    return false;
  }
  if (elem->array_suffix) {
    assert(elem->type != TYPE_CUSTOM);

    // This is an array
    Hjson::Value arr(Hjson::Value::VECTOR);

    for (int i = 0; i < array_size; i++) {
      val = *(const T*)bin_buffer;
      bin_buffer += sizeof(val);
      buf_size -= sizeof(val);
      arr.push_back((double)val);
    }
    if (dofill) {
      jval.insert(std::string{(const char*)elem->name}, std::move(arr));
    }
  } else {
    // This is a single element, not an array
    val = *(const T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    if (dofill) {
      jval.insert(std::string{(const char*)elem->name}, (double)val);
    }
  }
  return true;
}

bool process_element_string_json(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                 Hjson::Value& jval, bool dofill) {
  if (elem->array_suffix) {
    Hjson::Value arr(Hjson::Value::VECTOR);
    u32 array_size;
    if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size;
      // This is a dynamic array
      str_size = *(const u32*)bin_buffer;
      bin_buffer += sizeof(str_size);
      buf_size -= sizeof(str_size);
      std::string str((char*)bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;
      arr.push_back(std::move(str));
    }
    if (dofill) {
      jval.insert(std::string{(const char*)elem->name}, std::move(arr));
    }
    return true;
  }
  // This is an array
  u32 str_size;
  // This is a dynamic array
  str_size = *(const u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  std::string str((char*)bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  if (dofill) {
    jval.insert(std::string{(const char*)elem->name}, std::move(str));
  }
  return true;
}

bool process_element_short_string_json(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                       Hjson::Value& jval, bool dofill) {
  if (elem->array_suffix) {
    Hjson::Value arr(Hjson::Value::VECTOR);
    u32 array_size;
    if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size = 16;
      char str[16];
      memcpy(str, bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;
      arr.push_back(str);
    }
    if (dofill) {
      jval.insert(std::string{(const char*)elem->name}, std::move(arr));
    }
    return true;
  }

  // This is s static array
  u32 str_size = 16;
  char str[16];
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  if (dofill) {
    jval.insert(std::string{(const char*)elem->name}, std::string{str});
  }
  return true;
}

#endif

template <class T>
bool process_element(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                     const std::string& prefix) {
  T val;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
      return false;
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
        val = *(const T*)bin_buffer;
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        print(val);
        if (i < array_size - 1) printf(", ");
      }
      printf("\n");
    }
  } else {
    // This is a single element, not an array
    val = *(const T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    printf("%s%s: ", prefix.c_str(), elem->name);
    print(val);
    printf("\n");
  }
  return true;
}

static void printEnumVal(u32 val, ast_enum* en) {
  for (auto& e : en->elements) {
    if (e.item_value == val) {
      printf("%s", e.item_name);
      return;
    }
  }
  printf("%d", val);
}

static bool process_element_enum(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                 const std::string& prefix, ast_enum* en) {
  u32 val;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
      return false;
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
        val = *(const u32*)bin_buffer;
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        printEnumVal(val, en);
        if (i < array_size - 1) printf(", ");
      }
      printf("\n");
    }
  } else {
    // This is a single element, not an array
    val = *(const u32*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    printf("%s%s: ", prefix.c_str(), elem->name);
    printEnumVal(val, en);
    printf("\n");
  }
  return true;
}

template <class T>
bool skip_element(const u8*& bin_buffer, size_t& buf_size, u32 array_size) {
  bin_buffer += sizeof(T) * array_size;
  buf_size -= sizeof(T) * array_size;
  return true;
}

bool skip_string(const u8*& bin_buffer, size_t& buf_size, u32 array_size) {
  for (u32 i = 0; i < array_size; i++) {
    // Read the size of the string
    u32 str_size = *(const u32*)bin_buffer;
    bin_buffer += sizeof(u32);
    buf_size -= sizeof(u32);
    // Read the characters
    bin_buffer += str_size;
    buf_size -= str_size;
  }
  return true;
}

bool skip_short_string(const u8*& bin_buffer, size_t& buf_size, u32 array_size) {
  bin_buffer += sizeof(char) * 16 * array_size;
  buf_size -= sizeof(char) * 16 * array_size;
  return true;
}

template <class SrcType, class DstType>
void convert_element(SrcType val, ast_element* dst_elem, u8* dst_buf) {
  if (dst_elem->array_suffix && dst_elem->is_dynamic_array) {
    std::vector<DstType>& v = *(std::vector<DstType>*)dst_buf;
    v.push_back((DstType)val);
  } else {
    *(DstType*)dst_buf = (DstType)val;
  }
}

template <class T>
bool process_element_conversion(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                CBufParser& dst_parser, ast_element* dst_elem, u8* dst_buf, u32 dst_size) {
  T val;
  u32 array_size = 1;  // for those cases without an array
  if ((elem->array_suffix != nullptr) ^ (dst_elem->array_suffix != nullptr)) {
    // We do not support conversions from non array to array or viceversa
    return false;
  }

  if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
    return false;
  }

  u32 dst_array_size = 0;
  bool check_dst_array = false;
  u8* dst_elem_buf = dst_buf;

  if (dst_elem->array_suffix) {
    if (dst_elem->is_compact_array) {
      // For compact arrays, write the num
      *(u32*)dst_elem_buf = array_size;
      dst_elem_buf += sizeof(array_size);
    }
    if (!dst_elem->is_dynamic_array) {
      check_dst_array = true;
      dst_array_size = dst_elem->array_suffix->size;
    }
  }

  for (int i = 0; i < array_size; i++) {
    // Handle the case where the dest array is compact/fixed and smaller than the source
    if (check_dst_array && i >= dst_array_size) {
      return skip_element<T>(bin_buffer, buf_size, array_size - i);
    }
    val = *(const T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);

    // Now figure out how to stuff val into the dst_st, their elem
    switch (dst_elem->type) {
      case TYPE_U8: {
        convert_element<T, u8>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_U16: {
        convert_element<T, u16>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_U32: {
        convert_element<T, u32>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_U64: {
        convert_element<T, u64>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_S8: {
        convert_element<T, s8>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_S16: {
        convert_element<T, s16>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_S32: {
        convert_element<T, s32>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_S64: {
        convert_element<T, s64>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_F32: {
        convert_element<T, f32>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_F64: {
        convert_element<T, f64>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_BOOL: {
        convert_element<T, bool>(val, dst_elem, dst_elem_buf);
        break;
      }
      case TYPE_STRING: {
        // We do not support converting from a number to string
        return false;
      }
      case TYPE_SHORT_STRING: {
        // We do not support converting from a number to string
        return false;
      }
      case TYPE_CUSTOM: {
        if (dst_parser.isEnum(dst_elem)) {
          convert_element<T, u32>(val, dst_elem, dst_elem_buf);
        } else {
          // We cannot convert from number to struct
          return false;
        }
        break;
      }
    }
    dst_elem_buf += dst_elem->typesize;
  }

  return true;
}

template <class T>
bool process_element_csv(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size, bool doprint) {
  T val;
  if (elem->array_suffix) {
    // This is an array
    u32 num_elements;
    u32 array_size = elem->array_suffix->size;
    if (elem->is_dynamic_array || elem->is_compact_array) {
      // This is a dynamic array
      num_elements = *reinterpret_cast<const u32*>(bin_buffer);
      bin_buffer += sizeof(num_elements);
      buf_size -= sizeof(num_elements);
    } else {
      // this is a static array
      num_elements = array_size;
    }
    if (elem->is_compact_array && array_size > elem->array_suffix->size) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      if (i < num_elements) {
        val = *reinterpret_cast<const T*>(bin_buffer);
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        if (doprint) {
          print(val);
        }
      }
      if (doprint && i < array_size - 1) {
        printf(",");
      }
    }
  } else {
    // This is a single element, not an array
    val = *(T*)bin_buffer;
    bin_buffer += sizeof(val);
    buf_size -= sizeof(val);
    if (doprint) print(val);
  }
  return true;
}

bool process_element_string(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                            const std::string& prefix) {
  const char* str;
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size;
      // This is a dynamic array
      str_size = *(const u32*)bin_buffer;
      bin_buffer += sizeof(str_size);
      buf_size -= sizeof(str_size);
      str = (const char*)bin_buffer;
      bin_buffer += str_size;
      buf_size -= str_size;

      printf("%s%s[%d] = [ %.*s ]\n", prefix.c_str(), elem->name, i, str_size, str);
    }
    return true;
  }
  // This is an array
  u32 str_size;
  // This is a dynamic array
  str_size = *(u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  str = (const char*)bin_buffer;
  bin_buffer += str_size;
  buf_size -= str_size;

  printf("%s%s = [ %.*s ]\n", prefix.c_str(), elem->name, str_size, str);
  return true;
}

bool convert_element_string(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                            CBufParser& dst_parser, ast_element* dst_elem, u8* dst_buf, size_t dst_size) {
  const char* str;
  u32 array_size = 1;
  if ((elem->array_suffix != nullptr) ^ (dst_elem->array_suffix != nullptr)) {
    // We do not support conversions from non array to array or viceversa
    return false;
  }
  if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
    return false;
  }

  u32 dst_array_size = 0;
  bool check_dst_array = false;
  u8* dst_elem_buf = dst_buf;

  if (elem->array_suffix) {
    if (dst_elem->is_compact_array) {
      // For compact arrays, write the num
      *(u32*)dst_elem_buf = array_size;
      dst_elem_buf += sizeof(array_size);
    }
    if (!dst_elem->is_dynamic_array) {
      check_dst_array = true;
      dst_array_size = dst_elem->array_suffix->size;
    }
  }

  for (int i = 0; i < array_size; i++) {
    // Handle the case where the dest array is compact/fixed and smaller than the source
    if (check_dst_array && i >= dst_array_size) {
      return skip_string(bin_buffer, buf_size, array_size - i);
    }

    u32 str_size;
    // This is a dynamic array
    str_size = *(const u32*)bin_buffer;
    bin_buffer += sizeof(str_size);
    buf_size -= sizeof(str_size);
    str = (const char*)bin_buffer;
    bin_buffer += str_size;
    buf_size -= str_size;

    switch (dst_elem->type) {
      case TYPE_STRING: {
        if (dst_elem->array_suffix && dst_elem->is_dynamic_array) {
          std::vector<std::string>& v = *(std::vector<std::string>*)dst_elem_buf;
          v.push_back(std::string{str, str_size});
        } else {
          // Use placement new because the memory in dst_elem_buf might not be correct
          new (dst_elem_buf) std::string{str, str_size};
        }
        break;
      }
      case TYPE_SHORT_STRING: {
        if (dst_elem->array_suffix && dst_elem->is_dynamic_array) {
          std::vector<VString<15>>& v = *(std::vector<VString<15>>*)dst_elem_buf;
          v.push_back(std::string{str, str_size});
        } else {
          strncpy((char*)dst_elem_buf, str, 16);
        }
        break;
      }
      default:
        return false;
    }
    dst_elem_buf += dst_elem->typesize;
  }

  return true;
}

bool process_element_string_csv(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                bool doprint) {
  const char* str;
  if (elem->array_suffix) {
    assert(false);
    // Array of strings not implemented for csv. What would the header be? what size of array
    // of strings, given that we would have many messages with different sizes?
  }
  // This is an array
  u32 str_size;
  // This is a dynamic array
  str_size = *(const u32*)bin_buffer;
  bin_buffer += sizeof(str_size);
  buf_size -= sizeof(str_size);
  str = (const char*)bin_buffer;
  bin_buffer += str_size;
  buf_size -= str_size;

  if (doprint) printf("%.*s", str_size, str);
  return true;
}

bool process_element_short_string(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                  const std::string& prefix) {
  if (elem->array_suffix) {
    // This is an array
    u32 array_size;
    if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
      return false;
    }

    for (int i = 0; i < array_size; i++) {
      u32 str_size = 16;
      char str[16];
      memcpy(str, bin_buffer, str_size);
      bin_buffer += str_size;
      buf_size -= str_size;

      printf("%s%s[%d] = [] %s ]\n", prefix.c_str(), elem->name, i, str);
    }
    return true;
  }

  // This is s static array
  u32 str_size = 16;
  char str[16];
  memcpy(str, bin_buffer, str_size);
  bin_buffer += str_size;
  buf_size -= str_size;

  printf("%s%s = [ %s ]\n", prefix.c_str(), elem->name, str);
  return true;
}

bool convert_element_short_string(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                  CBufParser& dst_parser, ast_element* dst_elem, u8* dst_buf,
                                  size_t dst_size) {
  u32 array_size = 1;
  if ((elem->array_suffix != nullptr) ^ (dst_elem->array_suffix != nullptr)) {
    // We do not support conversions from non array to array or viceversa
    return false;
  }
  if (!processArraySize(elem, bin_buffer, buf_size, array_size)) {
    return false;
  }
  u32 dst_array_size = 0;
  bool check_dst_array = false;
  u8* dst_elem_buf = dst_buf;

  if (elem->array_suffix) {
    if (dst_elem->is_compact_array) {
      // For compact arrays, write the num
      *(u32*)dst_elem_buf = array_size;
      dst_elem_buf += sizeof(array_size);
    }
    if (!dst_elem->is_dynamic_array) {
      check_dst_array = true;
    }
  }

  for (int i = 0; i < array_size; i++) {
    // Handle the case where the dest array is compact/fixed and smaller than the source
    if (check_dst_array && i >= dst_array_size) {
      return skip_short_string(bin_buffer, buf_size, array_size - i);
    }

    u32 str_size = 16;
    char str[16];
    memcpy(str, bin_buffer, str_size);
    bin_buffer += str_size;
    buf_size -= str_size;

    switch (dst_elem->type) {
      case TYPE_STRING: {
        if (dst_elem->array_suffix && dst_elem->is_dynamic_array) {
          std::vector<std::string>& v = *(std::vector<std::string>*)dst_elem_buf;
          v.push_back(std::string{str, str_size});
        } else {
          new (dst_elem_buf) std::string{str, str_size};
        }
        break;
      }
      case TYPE_SHORT_STRING: {
        if (dst_elem->array_suffix && dst_elem->is_dynamic_array) {
          std::vector<VString<15>>& v = *(std::vector<VString<15>>*)dst_elem_buf;
          v.push_back(std::string{str, str_size});
        } else {
          strncpy((char*)dst_elem_buf, str, 16);
        }
        break;
      }
      default:
        return false;
    }
    dst_elem_buf += dst_elem->typesize;
  }

  return true;
}

bool process_element_short_string_csv(const ast_element* elem, const u8*& bin_buffer, size_t& buf_size,
                                      bool doprint) {
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

  if (doprint) printf("%s", str);
  return true;
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

bool compute_compact(ast_struct* st, SymbolTable* symtable, Interp* interp) {
  if (st->compact_computed) return st->has_compact;
  st->has_compact = false;
  for (auto* elem : st->elements) {
    if (elem->type == TYPE_STRING) {
      continue;
    }
    if (elem->is_compact_array) {
      st->has_compact = true;
      st->compact_computed = true;
      return true;
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
      compute_compact(inner_st, symtable, interp);
      if (inner_st->has_compact) {
        st->has_compact = true;
        st->simple_computed = true;
        return true;
      }
    }
  }
  st->compact_computed = true;
  return true;
}

u64 hash(const unsigned char* str) {
  u64 hash = 5381;
  int c;

  while ((c = *str++)) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

bool compute_hash(ast_struct* st, SymbolTable* symtable, Interp* interp) {
  StdStringBuffer buf;
  if (st->hash_computed) return true;

  buf.print("struct ");
  if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buf.print_no("%s::", st->space->name);
  buf.print("%s \n", st->name);
  for (auto* elem : st->elements) {
    if (elem->array_suffix) {
      buf.print("[%" PRIu64 "] ", elem->array_suffix->size);
    }
    if (elem->type == TYPE_CUSTOM) {
      auto* enm = symtable->find_enum(elem);
      if (enm != nullptr) {
        buf.print("%s %s;\n", elem->custom_name, elem->name);
        continue;
      }
      auto* inner_st = symtable->find_struct(elem);
      if (!inner_st) {
        interp->Error(elem, "Could not find this element for hash\n");
        return false;
      }
      assert(inner_st);
      bool bret = compute_hash(inner_st, symtable, interp);
      if (!bret) return false;
      buf.print("%" PRIX64 " %s;\n", inner_st->hash_value, elem->name);
    } else {
      buf.print("%s %s; \n", get_str_for_elem_type(elem->type), elem->name);
    }
  }

  st->hash_value = hash((const unsigned char*)buf.get_buffer());
  st->hash_computed = true;
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

size_t CBufParser::StructSize(const char* st_name) {
  auto* st = decompose_and_find(st_name);
  computeSizes(st, sym);
  return st->csize;
}

bool CBufParser::ParseMetadata(const std::string& metadata, const std::string& struct_name) {
  Parser parser;
  Interp interp;

  if (metadata.empty()) {
    fprintf(stderr, "Error, empty metadata for type %s\n", struct_name.c_str());
    return false;
  }

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

  loop_all_structs(ast, sym, &interp, compute_hash);
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
    if (!success) return false;
    switch (elem->type) {
      case TYPE_U8: {
        success = process_element_jstr<u8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U16: {
        success = process_element_jstr<u16>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U32: {
        success = process_element_jstr<u32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_U64: {
        success = process_element_jstr<u64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S8: {
        success = process_element_jstr<s8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S16: {
        success = process_element_jstr<s16>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S32: {
        success = process_element_jstr<s32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_S64: {
        success = process_element_jstr<s64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_F32: {
        success = process_element_jstr<f32>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_F64: {
        success = process_element_jstr<f64>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_BOOL: {
        success = process_element_jstr<u8>(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_STRING: {
        success = process_element_string_jstr(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_SHORT_STRING: {
        success = process_element_short_string_jstr(elem, buffer, buf_size, val);
        break;
      }
      case TYPE_CUSTOM: {
        auto* inst = sym->find_struct(elem);
        if (inst != nullptr) {
          if (elem->array_suffix != nullptr) {
            int array_size;
            if (elem->is_dynamic_array) {
              array_size = *(const u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
            } else if (elem->is_compact_array) {
              array_size = *(const u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
              std::string num_name = "\"num_";
              num_name += elem->name;
              val += num_name + "\":" + ::to_string(array_size) + ",";
            } else {
              array_size = elem->array_suffix->size;
            }
            if (elem->is_compact_array && array_size > elem->array_suffix->size) {
              success = false;
              return false;
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
              if (!success) return false;
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
          success = process_element_jstr<u32>(elem, buffer, buf_size, val);
        }
        break;
      }
    }
  }
  return success;
}

unsigned int CBufParser::FillJstr(const char* st_name, const unsigned char* buffer, size_t buf_size,
                                  std::string& jstr) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  jstr += "{";
  success = true;
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

bool CBufParser::FillHjsonInternal(const ast_struct* st, Hjson::Value& val, const char* ename, bool do_fill) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  const char* next_ename = nullptr;

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    bool dofill = do_fill && match_elem_name(elem->name, ename, &next_ename);
    if (!success) return false;
    switch (elem->type) {
      case TYPE_U8: {
        success = process_element_json<u8>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_U16: {
        success = process_element_json<u16>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_U32: {
        success = process_element_json<u32>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_U64: {
        success = process_element_json<u64>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_S8: {
        success = process_element_json<s8>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_S16: {
        success = process_element_json<s16>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_S32: {
        success = process_element_json<s32>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_S64: {
        success = process_element_json<s64>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_F32: {
        success = process_element_json<f32>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_F64: {
        success = process_element_json<f64>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_BOOL: {
        success = process_element_json<u8>(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_STRING: {
        success = process_element_string_json(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_SHORT_STRING: {
        success = process_element_short_string_json(elem, buffer, buf_size, val, dofill);
        break;
      }
      case TYPE_CUSTOM: {
        auto* inst = sym->find_struct(elem);
        if (inst != nullptr) {
          if (elem->array_suffix != nullptr) {
            int array_size;
            if (elem->is_dynamic_array) {
              array_size = *(const u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
            } else if (elem->is_compact_array) {
              array_size = *(const u32*)buffer;
              buffer += sizeof(array_size);
              buf_size -= sizeof(array_size);
              std::string num_name = "num_";
              num_name += elem->name;
              val.insert(std::move(num_name), array_size);
            } else {
              array_size = elem->array_suffix->size;
            }
            if (elem->is_compact_array && array_size > elem->array_suffix->size) {
              success = false;
              return false;
            }
            Hjson::Value arr(Hjson::Value::VECTOR);
            for (int a_idx = 0; a_idx < array_size; a_idx++) {
              Hjson::Value inner;
              FillHjsonInternal(inst, inner, next_ename, dofill);
              if (!success) return false;
              arr.push_back(std::move(inner));
            }
            val[(const char*)elem->name] = arr;
          } else {
            Hjson::Value inner;
            FillHjsonInternal(inst, inner, next_ename, dofill);
            val.insert(std::string{(const char*)elem->name}, std::move(inner));
          }
        } else {
          auto* enm = sym->find_enum(elem);
          if (enm == nullptr) {
            fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
            return false;
          }
          process_element_json<u32>(elem, buffer, buf_size, val, dofill);
        }
        break;
      }
    }
  }
  return success;
}

unsigned int CBufParser::FillHjson(const char* st_name, const unsigned char* buffer, size_t buf_size,
                                   Hjson::Value& val, bool print_body, const char* ename) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  Hjson::Value body;
  success = true;
  if (!FillHjsonInternal(decompose_and_find(st_name), body, ename)) {
    return 0;
  }
  if (print_body) {
    val.insert(std::string{"message_body"}, std::move(body));
  } else {
    val = body;
  }
  std::string cbuf_message_type{st_name};
  if (ename != nullptr) {
    cbuf_message_type.append(".");
    cbuf_message_type.append(ename);
  }
  val.insert(std::string{"cbuf_message_type"}, cbuf_message_type);
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}
#endif

bool CBufParser::PrintCSVHeaderInternal(const ast_struct* st, const std::string& prefix, const char* ename) {
  if (st == nullptr) return false;
  char* sdot = nullptr;

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    if (ename != nullptr) {
      // we might have to skip headers if we want only one element of the struct
      // first, there might be sub-names,
      const char* en = ename;
      char ebuf[256];
      strncpy(ebuf, ename, sizeof(ebuf));
      sdot = strchr(ebuf, '.');
      if (sdot != nullptr) {
        *sdot = 0;
        sdot++;
        en = ebuf;
      }
      if (strcmp(elem->name, en)) {
        continue;
      }
    }
    if (elem->array_suffix) {
      if (ename != nullptr) {
        // treat arrays with element name different since we would expand them on the CSV
        if (elem->type == TYPE_CUSTOM) {
          auto* inst = sym->find_struct(elem);
          if (inst == nullptr) {
            printf("ERROR, struct %s could not be found\n", elem->custom_name);
            return false;
          }
          std::string prefix_new = prefix + std::string(elem->name) + ".";
          return PrintCSVHeaderInternal(inst, prefix_new, sdot);
        } else {
          // print header for an array of single element, not expanded
          // A bit weird to use this path with ename, might as well use normal csv with inline
          printf("%s%s", prefix.c_str(), elem->name);
        }
        return true;
      }
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
            bool bret = PrintCSVHeaderInternal(inst, prefix_new, sdot);
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
        bool bret = PrintCSVHeaderInternal(inst, prefix_new, sdot);
        if (!bret) return false;
      } else {
        printf("%s%s", prefix.c_str(), elem->name);
      }
    }
    if (ename == nullptr && elem_idx + 1 < st->elements.size()) printf(",");
  }
  return true;
}

bool CBufParser::PrintCSVHeader(const char* st_name, const char* ename) {
  const char* sn = st_name;
  if (sn == nullptr) {
    sn = main_struct_name.c_str();
  }
  ast_struct* st = decompose_and_find(sn);
  if (st == nullptr) {
    fprintf(stderr, "Could not find struct %s on the symbol table\n", main_struct_name.c_str());
    return false;
  }

  PrintCSVHeaderInternal(st, "", ename);

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
          u32 array_size = elem->array_suffix->size;
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
            PrintCSVInternal(inst, nullptr);
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

bool CBufParser::PrintCSVInternal(const ast_struct* st, const char* ename, bool do_print) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  const char* next_ename = nullptr;

  for (int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    bool doprint = do_print && match_elem_name(elem->name, ename, &next_ename);
    if (!success) return false;
    switch (elem->type) {
      case TYPE_U8: {
        success = process_element_csv<u8>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_U16: {
        success = process_element_csv<u16>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_U32: {
        success = process_element_csv<u32>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_U64: {
        success = process_element_csv<u64>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_S8: {
        success = process_element_csv<s8>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_S16: {
        success = process_element_csv<s16>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_S32: {
        success = process_element_csv<s32>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_S64: {
        success = process_element_csv<s64>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_F32: {
        success = process_element_csv<f32>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_F64: {
        success = process_element_csv<f64>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_BOOL: {
        success = process_element_csv<u8>(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_STRING: {
        success = process_element_string_csv(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_SHORT_STRING: {
        success = process_element_short_string_csv(elem, buffer, buf_size, doprint);
        break;
      }
      case TYPE_CUSTOM: {
        if (elem->array_suffix) {
          // This is an array
          u32 array_size;
          if (elem->is_dynamic_array || elem->is_compact_array) {
            // This is a dynamic array
            array_size = *(const u32*)buffer;
            buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
          } else {
            // this is a static array
            array_size = elem->array_suffix->size;
          }
          if (elem->is_compact_array && array_size > elem->array_suffix->size) {
            success = false;
            return false;
          }

          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            for (u32 i = 0; i < array_size; i++) {
              PrintCSVInternal(inst, next_ename, doprint);
              if (doprint && i + 1 < array_size) {
                if (ename != nullptr)
                  printf("\n");
                else
                  printf(",");
              }
            }
            if (elem->is_compact_array && ename == nullptr) {
              if (doprint && (array_size < elem->array_suffix->size)) {
                printf(",");
              }
              for (u32 i = array_size; i < elem->array_suffix->size; i++) {
                PrintCSVInternalEmpty(inst);
                if (doprint && (i + 1 < elem->array_suffix->size)) printf(",");
              }
            }

          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            for (u32 i = 0; i < array_size; i++) {
              process_element_csv<u32>(elem, buffer, buf_size, doprint);
              if (doprint && i + 1 < array_size) printf(",");
            }
            if (elem->is_compact_array) {
              if (doprint && array_size < elem->array_suffix->size) {
                printf(",");
              }
              for (u32 i = array_size; i < elem->array_suffix->size; i++) {
                if (doprint && i + 1 < elem->array_suffix->size) printf(",");
              }
            }
          }

        } else {
          // This is a single element, not an array
          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            PrintCSVInternal(inst, next_ename, doprint);
          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            process_element_csv<u32>(elem, buffer, buf_size, doprint);
          }
        }

        break;
      }
    }
    if (doprint && ename == nullptr && elem_idx + 1 < st->elements.size()) printf(",");
  }
  return success;
}

bool CBufParser::PrintInternal(const ast_struct* st, const std::string& prefix) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  for (auto& elem : st->elements) {
    if (!success) return false;
    switch (elem->type) {
      case TYPE_U8: {
        success = process_element<u8>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_U16: {
        success = process_element<u16>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_U32: {
        success = process_element<u32>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_U64: {
        success = process_element<u64>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S8: {
        success = process_element<s8>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S16: {
        success = process_element<s16>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S32: {
        success = process_element<s32>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_S64: {
        success = process_element<s64>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_F32: {
        success = process_element<f32>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_F64: {
        success = process_element<f64>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_BOOL: {
        success = process_element<u8>(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_STRING: {
        success = process_element_string(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_SHORT_STRING: {
        success = process_element_short_string(elem, buffer, buf_size, prefix);
        break;
      }
      case TYPE_CUSTOM: {
        if (elem->array_suffix) {
          // This is an array
          u32 array_size;
          if (elem->is_dynamic_array || elem->is_compact_array) {
            // This is a dynamic array
            array_size = *(const u32*)buffer;
            buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
          } else {
            // this is a static array
            array_size = elem->array_suffix->size;
          }
          if (elem->is_compact_array && array_size > elem->array_suffix->size) {
            success = false;
            return false;
          }
          if (elem->is_compact_array) {
            printf("%snum_%s = %d\n", prefix.c_str(), elem->name, array_size);
          }

          auto* inst = sym->find_struct(elem);
          if (inst != nullptr) {
            for (u32 i = 0; i < array_size; i++) {
              std::string new_prefix = prefix + elem->name + "[" + ::to_string(i) + "].";
              PrintInternal(inst, new_prefix);
              if (!success) return false;
            }
          } else {
            auto* enm = sym->find_enum(elem);
            if (enm == nullptr) {
              fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
              return false;
            }
            // This function can handle arrays
            process_element_enum(elem, buffer, buf_size, prefix, enm);
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
            process_element_enum(elem, buffer, buf_size, prefix, enm);
          }
        }

        break;
      }
    }
  }
  return success;
}

bool CBufParser::SkipElementInternal(const ast_element* elem) {
  u32 array_size = 1;
  if (!processArraySize(elem, buffer, buf_size, array_size)) {
    return false;
  }
  switch (elem->type) {
    case TYPE_U8: {
      // We cannot pass dst_buf here, as the order of dst_buf might not be linear, we have to compute it
      success = skip_element<u8>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_U16: {
      success = skip_element<u16>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_U32: {
      success = skip_element<u32>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_U64: {
      success = skip_element<u64>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_S8: {
      success = skip_element<s8>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_S16: {
      success = skip_element<s16>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_S32: {
      success = skip_element<s32>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_S64: {
      success = skip_element<s64>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_F32: {
      success = skip_element<f32>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_F64: {
      success = skip_element<f64>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_BOOL: {
      success = skip_element<bool>(buffer, buf_size, array_size);
      break;
    }
    case TYPE_STRING: {
      success = skip_string(buffer, buf_size, array_size);
      break;
    }
    case TYPE_SHORT_STRING: {
      success = skip_short_string(buffer, buf_size, array_size);
      break;
    }
    case TYPE_CUSTOM: {
      auto* enm = sym->find_enum(elem);
      if (enm != nullptr) {
        success = skip_element<u32>(buffer, buf_size, array_size);
        break;
      }

      auto* inst = sym->find_struct(elem);
      if (inst == nullptr) {
        // Failed to find the struct, inconsistent metadata
        return false;
      }

      for (u32 i = 0; i < array_size; i++) {
        success = SkipStructInternal(inst);
        if (!success) return false;
      }

      break;
    }
  }
  return success;
}

bool CBufParser::SkipStructInternal(const ast_struct* st) {
  // All structs have a preamble, skip it
  if (!st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  for (const auto& elem : st->elements) {
    if (!success) return false;
    success = SkipElementInternal(elem);
  }
  return success;
}

bool CBufParser::FastConversionInternal(const ast_struct* src_st, CBufParser& dst_parser,
                                        const ast_struct* dst_st, unsigned char* dst_buf, size_t dst_size) {
  // All structs have a preamble, skip it
  if (!src_st->naked) {
    u32 sizeof_preamble = sizeof(cbuf_preamble);  // 8 bytes hash, 4 bytes size
    if (!dst_st->naked) {
      *(cbuf_preamble*)dst_buf = *(const cbuf_preamble*)buffer;
    }
    buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;
  }

  for (auto& elem : src_st->elements) {
    if (!success) return false;
    ast_element* dst_elem = find_elem_by_name(dst_st, elem->name);
    if (dst_elem == nullptr) {
      // we have a non corresponding element, skip those bytes right now
      success = SkipElementInternal(elem);
      continue;
    }
    switch (elem->type) {
      case TYPE_U8: {
        // We cannot pass dst_buf here, as the order of dst_buf might not be linear, we have to compute it
        success = process_element_conversion<u8>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                 dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_U16: {
        success = process_element_conversion<u16>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_U32: {
        success = process_element_conversion<u32>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_U64: {
        success = process_element_conversion<u64>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_S8: {
        success = process_element_conversion<s8>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                 dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_S16: {
        success = process_element_conversion<s16>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_S32: {
        success = process_element_conversion<s32>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_S64: {
        success = process_element_conversion<s64>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_F32: {
        success = process_element_conversion<f32>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_F64: {
        success = process_element_conversion<f64>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                  dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_BOOL: {
        success = process_element_conversion<bool>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                   dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_STRING: {
        success = convert_element_string(elem, buffer, buf_size, dst_parser, dst_elem,
                                         dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_SHORT_STRING: {
        success = convert_element_short_string(elem, buffer, buf_size, dst_parser, dst_elem,
                                               dst_buf + dst_elem->coffset, dst_elem->csize);
        break;
      }
      case TYPE_CUSTOM: {
        auto* enm = sym->find_enum(elem);
        if (enm != nullptr) {
          success = process_element_conversion<u32>(elem, buffer, buf_size, dst_parser, dst_elem,
                                                    dst_buf + dst_elem->coffset, dst_elem->csize);
          break;
        }

        auto* inst = sym->find_struct(elem);
        if (inst == nullptr) {
          // Failed to find the struct, inconsistent metadata
          return false;
        }

        auto* dst_elem_st = dst_parser.sym->find_struct(dst_elem);
        if (dst_elem_st == nullptr) {
          // We have a dst elem, but it is not a struct. Struct to non struct is not a supported conversion
          // We could just skip it, for now we fail
          return false;
        }

        if ((elem->array_suffix != nullptr) ^ (dst_elem->array_suffix != nullptr)) {
          // We do not support conversions from non array to array or viceversa
          return false;
        }

        u32 array_size = 1;
        u32 dst_array_size = 0;
        bool check_dst_array = false;
        if (!processArraySize(elem, buffer, buf_size, array_size)) {
          return false;
        }

        unsigned char* elem_dst_buf = dst_buf + dst_elem->coffset;
        u32 elem_dst_size = dst_elem->csize;
        // By how much to increment the memory pointer as we read
        u32 typesize = dst_elem->typesize;

        if (elem->array_suffix) {
          if (dst_elem->is_compact_array) {
            // For compact arrays, write the num here on dst
            *(u32*)elem_dst_buf = array_size;
            elem_dst_buf += sizeof(array_size);
            elem_dst_size -= sizeof(array_size);
          }
          if (!dst_elem->is_dynamic_array) {
            check_dst_array = true;
            dst_array_size = dst_elem->array_suffix->size;
          } else {
            // we have a dynamic array, of structures. We need to allocate memory here
            // We use the knowledge std::vector is implemented as 3 pointers:
            //  start of vector, end of vector, end of allocation
            u32 array_total_bytes = dst_elem_st->csize * array_size;
            unsigned char* array_mem_ptr = (unsigned char*)malloc(array_total_bytes);
            memset(array_mem_ptr, 0, array_total_bytes);
            unsigned char** vector_ptr = (unsigned char**)elem_dst_buf;
            *vector_ptr = array_mem_ptr;                            // start of vector
            *(vector_ptr + 1) = array_mem_ptr + array_total_bytes;  // end of vector elements
            *(vector_ptr + 2) = array_mem_ptr + array_total_bytes;  // end of vector allocation
            // Now overwrite the variables for the loop below
            elem_dst_buf = array_mem_ptr;
            elem_dst_size = array_total_bytes;
            typesize = dst_elem_st->csize;
          }
        }

        // For arrays, how do we increment elem_dst_buf? Dynamic arrays need not increment,
        // but defined arrays do
        for (u32 i = 0; i < array_size; i++) {
          if (check_dst_array && i >= dst_array_size) {
            // This is the case where the source array is bigger than the dest array
            success = SkipStructInternal(inst);
          } else {
            success = FastConversionInternal(inst, dst_parser, dst_elem_st, elem_dst_buf, elem_dst_size);
            elem_dst_buf += typesize;
            elem_dst_size -= typesize;
          }
          if (!success) return false;
        }

        break;
      }
    }
  }
  return success;
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
unsigned int CBufParser::Print(const char* st_name, const unsigned char* buffer, size_t buf_size) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  std::string prefix = std::string(st_name) + ".";
  success = true;
  if (!PrintInternal(decompose_and_find(st_name), prefix)) {
    return 0;
  }
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}

// Print only the data, no header information, and CSV friendly
unsigned int CBufParser::PrintCSV(const char* st_name, const unsigned char* buffer, size_t buf_size,
                                  const char* ename) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  success = true;
  if (!PrintCSVInternal(decompose_and_find(st_name), ename)) {
    return 0;
  }
  printf("\n");
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}

unsigned int CBufParser::FastConversion(const char* st_name, const unsigned char* buffer, size_t buf_size,
                                        CBufParser& dst_parser, const char* dst_name, unsigned char* dst_buf,
                                        size_t dst_size) {
  this->buffer = buffer;
  this->buf_size = buf_size;
  success = true;
  auto* dst_st = dst_parser.decompose_and_find(dst_name);
  if (dst_st == nullptr) {
    success = false;
    return 0;
  }

  // Ensure we have computed all the sizes we need
  computeSizes(dst_st, dst_parser.sym);

  if (!FastConversionInternal(decompose_and_find(st_name), dst_parser, dst_st, dst_buf, dst_size)) {
    return 0;
  }
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}

bool CBufParser::isEnum(const ast_element* elem) const { return sym->find_enum(elem) != nullptr; }
