#include "StdStringBuffer.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

StdStringBuffer::StdStringBuffer() {
  buffer.reserve(4096);
  ident = 0;
}

StdStringBuffer::~StdStringBuffer() { buffer.clear(); }

void StdStringBuffer::print(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (ident != 0) {
    for (int i = 0; i < ident; i++) buffer += ' ';
  }

  char small[10];

  va_list vacount;
  va_copy(vacount, args);
  int nsize = vsnprintf(small, 0, fmt, vacount) + 5;
  va_end(vacount);
  // note: we are counting on vsnprintf's behavior of returning the number of characters that
  // would be written, regardless of buffer size.

  assert(nsize >= 0);
  char* intermediate = (char*)malloc(nsize);

  vsnprintf(intermediate, nsize, fmt, args);
  va_end(args);

  buffer += intermediate;
  free(intermediate);
}

void StdStringBuffer::print_no(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  char small[10];

  va_list vacount;
  va_copy(vacount, args);
  int nsize = vsnprintf(small, 0, fmt, vacount) + 5;
  va_end(vacount);
  // note: we are counting on vsnprintf's behavior of returning the number of characters that
  // would be written, regardless of buffer size.

  assert(nsize >= 0);
  char* intermediate = (char*)malloc(nsize);

  vsnprintf(intermediate, nsize, fmt, args);
  va_end(args);

  buffer += intermediate;
  free(intermediate);
}

const char* StdStringBuffer::get_buffer() { return buffer.c_str(); }

void StdStringBuffer::reset() { buffer.clear(); }

void StdStringBuffer::prepend(const StdStringBuffer* buf) { buffer = buf->buffer + buffer; }
