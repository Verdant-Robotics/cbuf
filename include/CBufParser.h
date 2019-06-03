#pragma once

#include <string>
#include <stddef.h>

struct ast_global;
struct PoolAllocator;
struct SymbolTable;

class CBufParser
{
  ast_global *ast = nullptr;
  unsigned char *buffer = nullptr;
  size_t buf_size = 0;
  PoolAllocator *pool = nullptr;
  SymbolTable *sym = nullptr;

  bool PrintInternal(const char* st_name);

public:
  CBufParser();
  ~CBufParser();

  bool ParseMetadata(const std::string& metadata, const std::string& struct_name);
  bool isParsed() const { return ast != nullptr; }

  bool PrintCSVHeader();
  // Returns the number of bytes consumed
  unsigned int Print(const char* st_name, unsigned char *buffer, size_t buf_size);
  // Print only the data, no header information, and CSV friendly
  unsigned int PrintCSV(unsigned char *buffer, size_t buf_size);
};