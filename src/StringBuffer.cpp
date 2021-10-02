#include "StringBuffer.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StringBuffer::StringBuffer() {
  size_t initial_size = 4096 * 16;
  buffer = (char*)calloc(1, initial_size);
  end = buffer;
  end[0] = 0;
  ident = 0;
  buf_size = rem_size = initial_size;
}

StringBuffer::~StringBuffer() {
  free(buffer);
  buffer = nullptr;
  end = nullptr;
  buf_size = 0;
  rem_size = 0;
}

void StringBuffer::realloc_buffer(int nsize) {
  if (nsize >= (int)rem_size) {
    // reallocate buffer
    unsigned int new_size = buf_size * 4;  // fast growth
    int end_offset = buf_size - rem_size;
    assert(end[0] == 0);
    assert(end == buffer + end_offset);
    assert(buffer[end_offset] == 0);
    buffer = (char*)realloc(buffer, new_size);
    assert(buffer != nullptr);
    // memcpy(new_buffer, buffer, buf_size);
    end = buffer + end_offset;
    end[0] = 0;

    assert(end[0] == 0);
    assert(end == buffer + end_offset);
    assert(buffer[end_offset] == 0);

    rem_size = new_size - end_offset;
    buf_size = new_size;
    // free(buffer);
    // buffer = new_buffer;
  }
}

void StringBuffer::print(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if (ident != 0) {
    realloc_buffer(ident + 5);
    for (int i = 0; i < ident; i++) *end++ = ' ';
    rem_size -= ident;
  }

  assert(end + rem_size == buffer + buf_size);

  va_list vacount;
  va_copy(vacount, args);
  int nsize = vsnprintf(end, 0, fmt, vacount);
  va_end(vacount);

  assert(nsize >= 0);
  realloc_buffer(nsize + 1);

  end += vsnprintf(end, rem_size - 1, fmt, args);
  rem_size -= nsize;
  va_end(args);

  assert(end <= buffer + buf_size);
  assert(&buffer[buf_size - rem_size] == end);
  assert(end >= buffer);
}

void StringBuffer::print_no(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  assert(end + rem_size == buffer + buf_size);

  va_list vacount;
  va_copy(vacount, args);
  int nsize = vsnprintf(end, 0, fmt, vacount);
  va_end(vacount);

  assert(nsize >= 0);
  realloc_buffer(nsize + 1);

  end += vsnprintf(end, rem_size - 1, fmt, args);
  rem_size -= nsize;
  va_end(args);

  assert(end <= buffer + buf_size);
  assert(&buffer[buf_size - rem_size] == end);
  assert(end >= buffer);
}

const char* StringBuffer::get_buffer() { return buffer; }

void StringBuffer::reset() {
  end = buffer;
  end[0] = 0;
  ident = 0;
  rem_size = buf_size;
}

void StringBuffer::prepend(const StringBuffer* buf) {
  size_t prefix_size = buf->buf_size - buf->rem_size;
  // Make enough space for the new buffer
  realloc_buffer(prefix_size + 1);

  // Move the old data
  memmove(buffer + prefix_size, buffer, (buf_size - rem_size));

  memcpy(buffer, buf->buffer, prefix_size);
  end += prefix_size;
  rem_size -= prefix_size;
}
