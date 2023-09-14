#include "TextType.h"

#include <string.h>

#include "Array.h"

// @TODO: protect this array with a mutex
static Array<TextType> string_intern;

TextType CreateTextType(Allocator* p, const char* src) {
  u64 size = strlen(src) + 1;

  // try to find the string in our array
  // @TODO: Make this a hash in a future
  for (auto s : string_intern) {
    if (!strcmp(s, src)) {
      return s;
    }
  }

  // if we get here, we need to allocate one
  TextType text = (TextType)p->alloc(size);
#if defined(PLATFORM_WINDOWS)
  strncpy_s(text, size, src, size);
#else
  strncpy(text, src, size);
#endif
  string_intern.push_back(text);
  return text;
}
