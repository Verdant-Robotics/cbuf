#include <string>
#if defined(_WIN32)
#include <windows.h>
#else
#include <limits.h>
#include <stdlib.h>
#endif

#include "fileutils.h"

std::string getCanonicalPath(const char *path) {
#if defined(_WIN32)
  char buffer[MAX_PATH];
  if (GetFullPathName(path, MAX_PATH, buffer, nullptr)) {
    return std::string(buffer);
  } else {
    return std::string();
  }
#else
  char *realPath = realpath(path, nullptr);
  if (realPath) {
    std::string canonicalPath(realPath);
    free(realPath);
    return canonicalPath;
  } else {
    return std::string();
  }
#endif
}
