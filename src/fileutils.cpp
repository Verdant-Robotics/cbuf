#include "fileutils.h"

#include <string>

#include "mytypes.h"

#if defined(PLATFORM_WINDOWS)
#include <Windows.h>
#else
#include <limits.h>
#include <stdlib.h>
#endif

std::string getCanonicalPath(const char* path) {
#if defined(PLATFORM_WINDOWS)
  char buffer[MAX_PATH];
  if (GetFullPathName(path, MAX_PATH, buffer, nullptr)) {
    return std::string(buffer);
  } else {
    return std::string();
  }
#else
  char* realPath = realpath(path, nullptr);
  if (realPath) {
    std::string canonicalPath(realPath);
    free(realPath);
    return canonicalPath;
  } else {
    return std::string();
  }
#endif
}
