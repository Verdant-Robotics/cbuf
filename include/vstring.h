#pragma once

#include <string>

#ifndef ATTR_PACKED
#define ATTR_PACKED __attribute__((__packed__))
#endif

/// This class supports some interactions with the std string,
/// but it has static allocation so we can easily serialize it
template <int nchars>
class ATTR_PACKED VString {
  char buffer[nchars + 1];

  void fill_buffer(const char* s) {
    const char* is;
    int used_chars = 0;
    for (is = s; (*is != 0) && (used_chars < nchars); is++, used_chars++) {
      buffer[used_chars] = *is;
    }
    buffer[used_chars] = 0;
  }

  bool equals(const char* s) const {
    int used_chars = 0;
    for (; (used_chars <= nchars); used_chars++) {
      if (buffer[used_chars] != s[used_chars]) return false;
      if ((buffer[used_chars] == 0) && (s[used_chars] == 0)) return true;
      if ((buffer[used_chars] == 0) || (s[used_chars] == 0)) return false;
    }
    if (used_chars <= nchars)
      if ((buffer[used_chars] == 0) && (s[used_chars] == 0)) return true;
    return false;
  }

public:
  VString() { buffer[0] = 0; }

  VString(const char* s) { fill_buffer(s); }

  VString(const std::string& s) { fill_buffer(s.c_str()); }

  VString& operator=(const char* s) {
    fill_buffer(s);
    return *this;
  }

  VString& operator=(const std::string& s) {
    fill_buffer(s.c_str());
    return *this;
  }

  bool operator==(const char* s) const { return equals(s); }

  bool operator==(const std::string& s) const { return equals(s.c_str()); }

  bool operator==(const VString& s) const { return equals(s.c_str()); }

  bool operator!=(const char* s) const { return !equals(s); }

  bool operator!=(const std::string& s) const { return !equals(s.c_str()); }

  bool operator!=(const VString& s) const { return !equals(s.c_str()); }

  operator const std::string_view() const { return buffer; }

  const char* c_str() const { return buffer; }

  bool empty() const { return buffer[0] == 0; }

  size_t size() const {
    size_t used_chars = 0;
    for (; buffer[used_chars] != 0; used_chars++)
      ;

    if (used_chars > nchars) used_chars = nchars;
    return used_chars;
  }
};

template class VString<15>;
