#pragma once

#include <stdint.h>

struct cbuf_preamble
{
    uint64_t hash;
    uint32_t size;
};
