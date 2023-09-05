#pragma once

#include <stddef.h>

#include <string>

#if defined(HJSON_PRESENT)
#include <hjson.h>
#endif

struct ast_global;
struct ast_struct;
struct ast_element;

class PoolAllocator;
class SymbolTable;

class CBufParser {
protected:
  ast_global* ast = nullptr;
  const unsigned char* buffer = nullptr;
  size_t buf_size = 0;
  PoolAllocator* pool = nullptr;
  SymbolTable* sym = nullptr;
  bool success = true;

  bool PrintInternal(const ast_struct* st, const std::string& prefix);
  bool PrintCSVInternal(const ast_struct* st, const char* ename, bool doprint = true);
  bool PrintCSVInternalEmpty(const ast_struct* st);
  bool PrintCSVHeaderInternal(const ast_struct* st, const std::string& prefix, const char* ename);
  bool FillJstrInternal(const ast_struct* st, std::string& jstr);

  bool FastConversionInternal(const ast_struct* src_st, CBufParser& dst_parser, const ast_struct* dst_st,
                              unsigned char* dst_buf, size_t dst_size);
  bool SkipElementInternal(const ast_element* elem);
  bool SkipStructInternal(const ast_struct* st);

#if defined(HJSON_PRESENT)
  bool FillHjsonInternal(const ast_struct* st, Hjson::Value& val, const char* ename, bool dofill = true);
#endif

  std::string main_struct_name;

  ast_struct* decompose_and_find(const char* st_name);

public:
  CBufParser();
  ~CBufParser();

  bool ParseMetadata(const std::string& metadata, const std::string& struct_name);
  bool isParsed() const { return ast != nullptr; }
  bool isEnum(const ast_element* elem) const;
  size_t StructSize(const char* st_name);

  // st_name: optional name of the struct to print the header on, by default the main one on the parser
  // ename: optional member or sub_member of the struct
  bool PrintCSVHeader(const char* st_name = nullptr, const char* ename = nullptr);
  // Returns the number of bytes consumed
  unsigned int Print(const char* st_name, const unsigned char* buffer, size_t buf_size);
  // Print only the data, no header information, and CSV friendly
  unsigned int PrintCSV(const char* st_name, const unsigned char* buffer, size_t buf_size,
                        const char* ename = nullptr);
  unsigned int FillJstr(const char* st_name, const unsigned char* buffer, size_t buf_size, std::string& jstr);
  // Read a cbuf by name st_name (should have been parsed) from input stream buffer and write it to cbuf
  // parsed on dst_parser with name dst_name, on the bytes represented by dst_buf and dst_size
  unsigned int FastConversion(const char* st_name, const unsigned char* buffer, size_t buf_size,
                              CBufParser& dst_parser, const char* dst_name, unsigned char* dst_buf,
                              size_t dst_size);

#if defined(HJSON_PRESENT)
  unsigned int FillHjson(const char* st_name, const unsigned char* buffer, size_t buf_size, Hjson::Value& val,
                         bool print_body, const char* ename = nullptr);
#endif
};

#if defined(HJSON_PRESENT)

template <typename T>
Hjson::Value CBufToHjson(const T& cb_msg) {
  CBufParser parser;
  Hjson::Value json;
  parser.ParseMetadata(T::cbuf_string, T::TYPE_STRING);
  size_t buf_size = cb_msg.encode_net_size();
  char* buf = new char[buf_size];
  cb_msg.encode_net(buf, buf_size);
  parser.FillHjson(cb_msg.TYPE_STRING, (unsigned char*)buf, buf_size, json, false);
  delete[] buf;
  return json;
}

template <typename T>
std::string CBufToHjsonString(const T& cb_msg) {
  return Hjson::Marshal(CBufToHjson(cb_msg));
}

#endif
