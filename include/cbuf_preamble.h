#pragma once

#include <assert.h>
#include <stdint.h>

#define CBUF_MAGIC uint32_t(('V' << 24) | ('D' << 16) | ('N' << 8) | 'T')

#ifndef ATTR_PACKED
#define ATTR_PACKED __attribute__((__packed__))
#endif

///
/// Cbuf preamble stores 2 values on the size. On the top
/// 4 bits, a 'variant' number is stored to differentiate
/// messages of the same type.
/// The lower 28 bits are the size of a packet
///
struct ATTR_PACKED cbuf_preamble {
  uint32_t magic = 0;
  uint32_t size_ = 0;
  uint64_t hash = 0;
  double packet_timest;

  bool _hasVariant() const { return (size_ & 0x80000000) == 0x80000000; }

  uint32_t size() const {
    if (_hasVariant()) {
      return size_ & 0x07FFFFFF;
    } else {
      return size_ & 0x7FFFFFFF;
    }
  }

  void setSize(uint32_t sz) {
    if (_hasVariant()) {
      assert(sz < 0x07FFFFFF);
      size_ = (0xF8000000 & size_) | (sz & 0x07FFFFFF);
    } else {
      assert(sz < 0x7FFFFFFF);
      size_ = sz & 0x7FFFFFFF;
    }
  }

  uint8_t variant() const {
    if (_hasVariant()) {
      return (size_ >> 27) & 0x0F;
    }
    return 0;
  }

  void setVariant(uint8_t v) {
    if (v == 0) {
      if (!_hasVariant()) return;
      size_ &= 0x07FFFFFF;
    } else {
      uint32_t tmp = uint32_t((v & 0x0F) << 27);
      if (_hasVariant()) {
        size_ = 0x80000000 | tmp | size();
      } else {
        assert((size_ & 0x7FFFFFFF) < 0x07FFFFFF);
        size_ = 0x80000000 | tmp | (size_ & 0x07FFFFFF);
      }
    }
  }
};

typedef void (*cbuf_metadata_fn)(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx);

// USe this to debug possible encoding errors
// #define CBUF_ASSERT_DECODE_ERROR

#if defined(CBUF_ASSERT_DECODE_ERROR)
#define CBUF_RETURN_FALSE() \
  do {                      \
    assert(false);          \
    return false;           \
  } while (false)
#else
#define CBUF_RETURN_FALSE() \
  do {                      \
    return false;           \
  } while (false)
#endif
