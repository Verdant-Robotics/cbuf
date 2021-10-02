#pragma once

#include <stddef.h>

#include <string>

#if defined(HJSON_PRESENT)
#include <hjson.h>
#endif

struct ast_global;
struct ast_struct;

class PoolAllocator;
class SymbolTable;

class CBufParser {
  ast_global* ast = nullptr;
  unsigned char* buffer = nullptr;
  size_t buf_size = 0;
  PoolAllocator* pool = nullptr;
  SymbolTable* sym = nullptr;

  bool PrintInternal(const ast_struct* st, const std::string& prefix);
  bool PrintCSVInternal(const ast_struct* st);
  bool PrintCSVInternalEmpty(const ast_struct* st);
  bool PrintCSVHeaderInternal(const ast_struct* st, const std::string& prefix);
  bool FillJstrInternal(const ast_struct* st, std::string& jstr);

#if defined(HJSON_PRESENT)
  bool FillHjsonInternal(const ast_struct* st, Hjson::Value& val);
#endif

  std::string main_struct_name;

  ast_struct* decompose_and_find(const char* st_name);

public:
  CBufParser();
  ~CBufParser();

  bool ParseMetadata(const std::string& metadata, const std::string& struct_name);
  bool isParsed() const { return ast != nullptr; }

  bool PrintCSVHeader();
  // Returns the number of bytes consumed
  unsigned int Print(const char* st_name, unsigned char* buffer, size_t buf_size);
  // Print only the data, no header information, and CSV friendly
  unsigned int PrintCSV(const char* st_name, unsigned char* buffer, size_t buf_size);
  unsigned int FillJstr(const char* st_name, unsigned char* buffer, size_t buf_size, std::string& jstr);

#if defined(HJSON_PRESENT)
  unsigned int FillHjson(const char* st_name, unsigned char* buffer, size_t buf_size, Hjson::Value& val,
                         bool print_body);
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
