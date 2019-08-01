#pragma once

#include <stdint.h>

#define CBUF_MAGIC uint32_t(('V'<<24) | ('D'<<16) | ('N'<<8) | 'T')

#ifndef ATTR_PACKED
#define ATTR_PACKED __attribute__ ((__packed__))
#endif

struct ATTR_PACKED cbuf_preamble
{
    uint32_t magic;
    uint32_t size;
    uint64_t hash;
    double packet_timest;
};
