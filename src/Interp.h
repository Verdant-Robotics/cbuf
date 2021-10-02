#pragma once
#include <stdarg.h>

#include "SrcLocation.h"

class FileData;
struct ast_element;
struct ast_struct;
struct ast_enum;

class Interp {
  char errorStringBuffer[4096];
  char* errorString = nullptr;

  bool has_error_ = false;

public:
  Interp();
  ~Interp();

  void ErrorWithLoc(const SrcLocation& loc, const FileData* file, const char* msg, va_list args);
  void ErrorWithLoc(const SrcLocation& loc, const FileData* file, const char* msg, ...);
  void Error(const ast_element* elem, const char* msg, ...);
  void Error(const ast_struct* st, const char* msg, ...);
  void Error(const ast_enum* en, const char* msg, ...);

  // Generic error, no location
  void Error(const char* msg, ...);
  bool has_error() const { return has_error_; }

  const char* getErrorString() { return errorStringBuffer; }
};
