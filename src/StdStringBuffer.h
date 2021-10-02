#pragma once
#include <stddef.h>

#include <string>

class StdStringBuffer {
  std::string buffer;
  int ident;

public:
  StdStringBuffer();
  ~StdStringBuffer();

  void print(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  void print_no(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
  const char* get_buffer();
  void reset();
  void increase_ident() { ident += 4; }
  void decrease_ident() { ident -= 4; }
  int get_ident() { return ident; }
  void set_ident(int i) { ident = i; }
  void prepend(const StdStringBuffer* buf);
};
