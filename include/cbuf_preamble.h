#pragma once

#include <stdint.h>

struct __attribute__ ((__packed__)) cbuf_preamble
{
    uint64_t hash;
    uint32_t size;
};
