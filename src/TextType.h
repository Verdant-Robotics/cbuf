#pragma once

#include "Allocator.h"

typedef char* TextType;

TextType CreateTextType(Allocator* p, const char* src);
