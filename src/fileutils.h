#pragma once

#include <string>

/**
 * @brief Returns the canonical path of the given path. Similar to the
 *   `canonicalize_file_name()` GNU extension on Linux.
 * @param path The path to canonicalize.
 * @returns The canonical path of the given path on success, or an empty string
 *   on failure.
 */
std::string getCanonicalPath(const char *path);
