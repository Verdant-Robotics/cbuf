#include <stdio.h>
#include <inttypes.h>

#include "CBufParser.h"
#include "Parser.h"
#include "SymbolTable.h"

#include <string.h>

template<class T>
void print(T elem)
{
    printf("%d", elem);
}

template<>
void print<u64>(u64 elem)
{
    printf("%" PRIu64, elem);
}

template<>
void print<s64>(s64 elem)
{
    printf("%" PRId64, elem);
}

template<>
void print<f64>(f64 elem)
{
    printf("%.10f", elem);
}

template<>
void print<f32>(f32 elem)
{
    printf("%.10f", elem);
}

template<class T>
void process_element(const ast_element* elem, u8* &bin_buffer, u64 &buf_size)
{
    T val;
    if (elem->array_suffix) {
        // This is an array
        u32 array_size;
        if (elem->is_dynamic_array) {
            // This is a dynamic array
            array_size = *(u32 *)bin_buffer;
            bin_buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
        } else {
            // this is a static array
            array_size = elem->array_suffix->size;
        }
        if (array_size > 5) {
            printf("%s[%d] = ...\n", elem->name, array_size);
            bin_buffer += sizeof(val)*array_size;
            buf_size -= sizeof(val)*array_size;
        } else {
            printf("%s[%d] = ", elem->name, array_size);
            for(int i=0; i<array_size; i++) {
                val = *(T *)bin_buffer;
                bin_buffer += sizeof(val);
                buf_size -= sizeof(val);
                print(val);
                if (i<array_size-1) printf(", ");
            }
            printf("\n");
        }
    } else {
        // This is a single element, not an array
        val = *(T *)bin_buffer;
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        printf("%s: ", elem->name);
        print(val); printf("\n");
    }
}

template<class T>
void process_element_csv(const ast_element* elem, u8* &bin_buffer, u64 &buf_size)
{
    T val;
    if (elem->array_suffix) {
        // This is an array
        u32 array_size;
        if (elem->is_dynamic_array) {
            // This is a dynamic array
            array_size = *(u32 *)bin_buffer;
            bin_buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
        } else {
            // this is a static array
            array_size = elem->array_suffix->size;
        }
        if (array_size > 5) {
            bin_buffer += sizeof(val)*array_size;
            buf_size -= sizeof(val)*array_size;
        } else {
            for(int i=0; i<array_size; i++) {
                val = *(T *)bin_buffer;
                bin_buffer += sizeof(val);
                buf_size -= sizeof(val);
                print(val);
                if (i<array_size-1) printf(",");
            }
        }
    } else {
        // This is a single element, not an array
        val = *(T *)bin_buffer;
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        print(val);
    }
}


void process_element_string(const ast_element* elem, u8* &bin_buffer, u64 &buf_size)
{
    char *str;
    if (elem->array_suffix) {
        assert(false);
        // Array of strings not implemented
    }
        // This is an array
    u32 str_size;
    // This is a dynamic array
    str_size = *(u32 *)bin_buffer;
    bin_buffer += sizeof(str_size);
    buf_size -= sizeof(str_size);
    str = new char[str_size+1];
    memset(str, 0, str_size+1);
    memcpy(str, bin_buffer, str_size);
    bin_buffer += str_size;
    buf_size -= str_size;

    printf("%s = [ %s ]\n", elem->name, str);
    delete[] str;
}

void process_element_string_csv(const ast_element* elem, u8* &bin_buffer, u64 &buf_size)
{
    char *str;
    if (elem->array_suffix) {
        assert(false);
        // Array of strings not implemented
    }
        // This is an array
    u32 str_size;
    // This is a dynamic array
    str_size = *(u32 *)bin_buffer;
    bin_buffer += sizeof(str_size);
    buf_size -= sizeof(str_size);
    str = new char[str_size+1];
    memset(str, 0, str_size+1);
    memcpy(str, bin_buffer, str_size);
    bin_buffer += str_size;
    buf_size -= str_size;

    printf("%s", str);
    delete[] str;
}


template< typename T >
void loop_all_structs(ast_global *ast, SymbolTable *symtable, T func)
{
    for(auto *sp: ast->spaces) {
        for(auto *st: sp->structs) {
            func(st, symtable);
        }
    }

    for(auto *st: ast->global_space.structs) {
        func(st, symtable);
    }
}

bool compute_simple(ast_struct *st, SymbolTable *symtable)
{
    if (st->simple_computed) return st->simple;
    st->simple = true;
    for (auto *elem: st->elements) {
        if (elem->type == TYPE_STRING) {
            st->simple = false;
            st->simple_computed = true;
            return false;
        }
        if (elem->type == TYPE_CUSTOM) {
            if (!symtable->find_symbol(elem->custom_name)) {
                fprintf(stderr, "Struct %s, element %s was referencing type %s and could not be found\n",
                    st->name, elem->name, elem->custom_name);
                exit(-1);
            }
            auto *inner_st = symtable->find_struct(elem->custom_name);
            if (inner_st == nullptr) {
                // Must be an enum, it is simple
                continue;
            }
            bool elem_simple = compute_simple(inner_st, symtable);
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


CBufParser::CBufParser()
{
  pool = new PoolAllocator();
}

CBufParser::~CBufParser()
{
  if (sym) {
    delete sym;
    sym = nullptr;
  }

  if (pool) {
    delete pool;
    pool = nullptr;
  }
}

bool CBufParser::ParseMetadata(const std::string& metadata, const std::string& struct_name)
{
  Parser parser;
  ast = parser.ParseBuffer(metadata.c_str(), metadata.size()-1, pool);
  if (ast == nullptr || !parser.success) {
    fprintf(stderr, "Error during parsing:\n%s\n", parser.getErrorString());
    return false;
  }

  sym = new SymbolTable;
  bool bret = sym->initialize(ast);
  if (!bret) {
      fprintf(stderr, "Error during symbol table parsing:\n%s\n", parser.getErrorString());
      return false;
  }

  loop_all_structs(ast, sym, compute_simple);

  main_struct_name = struct_name;
  return true;
}

bool CBufParser::PrintCSVHeader()
{
  auto tname = CreateTextType(pool, main_struct_name.c_str());
  ast_struct *st = sym->find_struct(tname);

  for(int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    if (elem->array_suffix) {
      if (elem->is_dynamic_array) {
        printf("ERROR: Dynamic arrays are not supported on CSV mode\n");
        return false;
      }
      if (elem->array_suffix->size <= 5) {
        for(int i =0; i<elem->array_suffix->size; i++) {
          printf("%s[%d]", elem->name, i);
          if (i+1<elem->array_suffix->size) printf(",");
        }
      }
    } else {
      printf("%s", elem->name);
    }
    if (elem_idx +1 < st->elements.size()) printf(",");
  }
  printf("\n");
  return true;
}

bool CBufParser::PrintCSVInternal(const char *st_name)
{
  auto tname = CreateTextType(pool, st_name);
  ast_struct *st = sym->find_struct(tname);

  // All structs have a preamble, skip it
  u32 sizeof_preamble = 12; // 8 bytes hash, 4 bytes size
  buffer += sizeof_preamble;
  buf_size -= sizeof_preamble;

  for(int elem_idx = 0; elem_idx < st->elements.size(); elem_idx++) {
    auto& elem = st->elements[elem_idx];
    switch(elem->type) {
      case TYPE_U8:  { process_element_csv< u8>(elem, buffer, buf_size); break; }
      case TYPE_U16: { process_element_csv<u16>(elem, buffer, buf_size); break; }
      case TYPE_U32: { process_element_csv<u32>(elem, buffer, buf_size); break; }
      case TYPE_U64: { process_element_csv<u64>(elem, buffer, buf_size); break; }
      case TYPE_S8:  { process_element_csv< s8>(elem, buffer, buf_size); break; }
      case TYPE_S16: { process_element_csv<s16>(elem, buffer, buf_size); break; }
      case TYPE_S32: { process_element_csv<s32>(elem, buffer, buf_size); break; }
      case TYPE_S64: { process_element_csv<s64>(elem, buffer, buf_size); break; }
      case TYPE_F32: { process_element_csv<f32>(elem, buffer, buf_size); break; }
      case TYPE_F64: { process_element_csv<f64>(elem, buffer, buf_size); break; }
      case TYPE_BOOL:{ process_element_csv< u8>(elem, buffer, buf_size); break; }
      case TYPE_STRING:
      {
          process_element_string_csv(elem, buffer, buf_size);
          break;
      }
      case TYPE_CUSTOM:
      {
          auto *inst = sym->find_struct(elem->custom_name);
          if (inst != nullptr) {
              PrintCSVInternal(elem->custom_name);
          } else {
              auto *enm = sym->find_enum(elem->custom_name);
              if (enm == nullptr) {
                  fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
                  return false;
              }
              process_element_csv<u32>(elem, buffer, buf_size);
          }
          break;
      }

    }
    if (elem_idx +1 < st->elements.size()) printf(",");
  }
  printf("\n");
  return true;
}

bool CBufParser::PrintInternal(const char* st_name)
{
  auto tname = CreateTextType(pool, st_name);
  ast_struct *st = sym->find_struct(tname);

  // All structs have a preamble, skip it
  u32 sizeof_preamble = 12; // 8 bytes hash, 4 bytes size
  buffer += sizeof_preamble;
  buf_size -= sizeof_preamble;

  for(auto& elem: st->elements) {
      switch(elem->type) {
      case TYPE_U8:  { process_element< u8>(elem, buffer, buf_size); break; }
      case TYPE_U16: { process_element<u16>(elem, buffer, buf_size); break; }
      case TYPE_U32: { process_element<u32>(elem, buffer, buf_size); break; }
      case TYPE_U64: { process_element<u64>(elem, buffer, buf_size); break; }
      case TYPE_S8:  { process_element< s8>(elem, buffer, buf_size); break; }
      case TYPE_S16: { process_element<s16>(elem, buffer, buf_size); break; }
      case TYPE_S32: { process_element<s32>(elem, buffer, buf_size); break; }
      case TYPE_S64: { process_element<s64>(elem, buffer, buf_size); break; }
      case TYPE_F32: { process_element<f32>(elem, buffer, buf_size); break; }
      case TYPE_F64: { process_element<f64>(elem, buffer, buf_size); break; }
      case TYPE_BOOL:{ process_element< u8>(elem, buffer, buf_size); break; }
      case TYPE_STRING:
      {
          process_element_string(elem, buffer, buf_size);
          break;
      }
      case TYPE_CUSTOM:
      {
          auto *inst = sym->find_struct(elem->custom_name);
          if (inst != nullptr) {
              PrintInternal(elem->custom_name);
          } else {
              auto *enm = sym->find_enum(elem->custom_name);
              if (enm == nullptr) {
                  fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
                  return false;
              }
              process_element<u32>(elem, buffer, buf_size);
          }
          break;
      }

      }
  }
  return true;
}

// Returns the number of bytes consumed
unsigned int CBufParser::Print(const char* st_name, unsigned char *buffer, size_t buf_size)
{
  this->buffer = buffer;
  this->buf_size = buf_size;
  if (!PrintInternal(st_name)) {
    return 0;
  }
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}

// Print only the data, no header information, and CSV friendly
unsigned int CBufParser::PrintCSV(const char* st_name, unsigned char *buffer, size_t buf_size)
{
  this->buffer = buffer;
  this->buf_size = buf_size;
  if (!PrintCSVInternal(st_name)) {
    return 0;
  }
  this->buffer = nullptr;
  return buf_size - this->buf_size;
}
