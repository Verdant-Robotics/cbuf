#include "Interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FileData.h"
#include "ast.h"

Interp::Interp() {
  memset(errorStringBuffer, 0, sizeof(errorStringBuffer));
  errorString = errorStringBuffer;
}

Interp::~Interp() {}

void Interp::ErrorWithLoc(const SrcLocation& loc, const FileData* file, const char* msg, va_list args) {
  s32 off = sprintf(errorString, "%s:%d:%d: error : ", file->getFilename(), loc.line, loc.col);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
  off += vsprintf(errorString + off, msg, args);
#pragma clang diagnostic pop
  has_error_ = true;
  errorString += off;
  errorString = file->printLocation(loc, errorString);
}

void Interp::Error(const char* msg, ...) {
  va_list args;
  va_start(args, msg);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
  s32 off = vsprintf(errorString, msg, args);
#pragma clang diagnostic pop
  errorString += off;
  has_error_ = true;
  va_end(args);
}

void Interp::ErrorWithLoc(const SrcLocation& loc, const FileData* file, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  ErrorWithLoc(loc, file, msg, args);
  va_end(args);
}

void Interp::Error(const ast_element* elem, const char* msg, ...) {
  va_list args;
  const SrcLocation& loc = elem->loc;
  const FileData* file = elem->enclosing_struct->file;

  va_start(args, msg);
  ErrorWithLoc(loc, file, msg, args);
  va_end(args);
}

void Interp::Error(const ast_struct* st, const char* msg, ...) {
  va_list args;
  const SrcLocation& loc = st->loc;
  const FileData* file = st->file;

  va_start(args, msg);
  ErrorWithLoc(loc, file, msg, args);
  va_end(args);
}

void Interp::Error(const ast_enum* en, const char* msg, ...) {
  va_list args;
  const SrcLocation& loc = en->loc;
  const FileData* file = en->file;

  va_start(args, msg);
  ErrorWithLoc(loc, file, msg, args);
  va_end(args);
}
