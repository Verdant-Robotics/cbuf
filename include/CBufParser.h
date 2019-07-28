#pragma once

#include <string>
#include <stddef.h>

struct ast_global;
struct ast_struct;

class PoolAllocator;
class SymbolTable;

class CBufParser
{
  ast_global *ast = nullptr;
  unsigned char *buffer = nullptr;
  size_t buf_size = 0;
  PoolAllocator *pool = nullptr;
  SymbolTable *sym = nullptr;

  bool PrintInternal(const ast_struct* st);
  bool PrintCSVInternal(const ast_struct* st);
  std::string main_struct_name;

  ast_struct* decompose_and_find(const char *st_name);

public:
  CBufParser();
  ~CBufParser();

  bool ParseMetadata(const std::string& metadata, const std::string& struct_name);
  bool isParsed() const { return ast != nullptr; }

  bool PrintCSVHeader();
  // Returns the number of bytes consumed
  unsigned int Print(const char* st_name, unsigned char *buffer, size_t buf_size);
  // Print only the data, no header information, and CSV friendly
  unsigned int PrintCSV(const char* st_name, unsigned char *buffer, size_t buf_size);
};
