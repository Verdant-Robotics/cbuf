#pragma once
#include <unistd.h>
#include <map>
#include <error.h>
#include <assert.h>
#include "cbuf_preamble.h"

class ULogger;

class cbuf_ostream
{
  std::map<uint64_t, std::string> dictionary;
  int stream = -1;

  double now() const ;

  friend class ULogger;

  void serialize_metadata(const char *msg_meta, uint64_t hash, const char *msg_name);

public:
  cbuf_ostream() {}
  ~cbuf_ostream() { close(); }

  void close();

  bool is_open() const { return stream != -1; }

  bool open_file(const char *fname);

  bool open_socket(const char *ip, int port);

  template <class cbuf_struct>
  bool serialize(cbuf_struct * member)
  {
    // check if we have serialized this type before or not.
    if (!dictionary.count(member->hash())) {
      // If not, serialize its metadata
      serialize_metadata(member->cbuf_string, member->hash(), member->TYPE_STRING);
    }

    member->preamble.packet_timest = now();
    // Serialize the data of the member itself
    char* ptr = member->encode();
    write(stream, ptr, member->encode_size());
    member->free_encode(ptr);

    return true;
  }
};


class cbuf_istream
{
  std::map<uint64_t, std::string> dictionary;
  std::map<uint64_t, std::string> metadictionary;
  int stream = -1;
  unsigned char *memmap_ptr = nullptr;
  unsigned char *ptr = nullptr;
  size_t rem_size = 0;
  size_t filesize = 0;

  uint64_t __get_next_hash()
  {
    cbuf_preamble *pre = (cbuf_preamble *)ptr;
    return pre->hash;
  }

  uint32_t __get_next_size()
  {
    cbuf_preamble *pre = (cbuf_preamble *)ptr;
    return pre->size;
  }


  /// Try to consume metadata packets, not exposed to clients
  /// Return true if a packet was consumed, false otherwise
  bool consume_internal();

public:
  cbuf_istream() {}
  ~cbuf_istream() { close(); }

  void close();

  bool open_file(const char *fname);

  bool open_socket(const char *ip, int port);

  template <class cbuf_struct>
  bool deserialize(cbuf_struct * member)
  {
    bool ret;

    if (empty()) return false;
    if (consume_internal()) {
        return deserialize(member);
    }

    auto nsize = get_next_size();
    ret = member->decode((char *)ptr, rem_size);
    if (!ret) return false;
    ptr += nsize;
    rem_size -= nsize;
    return true;
  }

  std::string get_string_for_hash(uint64_t hash)
  {
    if (dictionary.count(hash) > 0) {
        return dictionary[hash];
    }
    return std::string();
  }

  std::string get_meta_string_for_hash(uint64_t hash)
  {
    if (metadictionary.count(hash) > 0) {
        return metadictionary[hash];
    }
    return std::string();
  }

  uint64_t get_next_hash()
  {
    if (consume_internal()) {
      return get_next_hash();
    }
    cbuf_preamble *pre = (cbuf_preamble *)ptr;
    return pre->hash;
  }

  uint32_t get_next_size()
  {
    if (consume_internal()) {
      return get_next_size();
    }
    cbuf_preamble *pre = (cbuf_preamble *)ptr;
    return pre->size;
  }

  // Are there more objects on the stream?
  bool empty()
  {
    return rem_size <= 0;
  }

  bool skip_message()
  {
      if (empty()) return false;
      if (consume_internal()) {
          return skip_message();
      }

      auto nsize = get_next_size();
      ptr += nsize;
      rem_size -= nsize;
      return true;
  }

  unsigned char * get_current_ptr()
  {
    return ptr;
  }

};
