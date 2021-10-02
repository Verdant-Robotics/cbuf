#pragma once
#include <assert.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include "cbuf_preamble.h"

class ULogger;
class cbuf_istream;

void serialize_metadata_cbuf(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx);

// Note: these classes will compact whenever possible, to work with ulogger
class cbuf_ostream {
  std::map<uint64_t, std::string> dictionary;
  std::map<uint64_t, std::string> metadictionary;  // used when merging
  std::string fname_;
  int stream = -1;

  double now() const;

  friend class ULogger;

  friend void serialize_metadata_cbuf(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx);
  void serialize_metadata(const char* msg_meta, uint64_t hash, const char* msg_name);

  bool merge_packet(cbuf_istream* cis, const std::vector<std::string>& filter, bool filter_positive);

public:
  cbuf_ostream() {}
  ~cbuf_ostream() { close(); }

  void close();

  bool is_open() const { return stream != -1; }

  bool open_file(const char* fname);

  bool open_socket(const char* ip, int port);

  const std::string& filename() const { return fname_; }

  template <class cbuf_struct>
  bool serialize(cbuf_struct* member) {
    // check if we have serialized this type before or not.
    if (!dictionary.count(member->hash())) {
      // If not, serialize its metadata
      member->handle_metadata(serialize_metadata_cbuf, this);
    }

    member->preamble.packet_timest = now();
    // Serialize the data of the member itself
    if (member->supports_compact()) {
      auto ns = member->encode_net_size();
      char* ptr = (char*)malloc(ns);
      member->encode_net(ptr, ns);
      write(stream, ptr, ns);
      free(ptr);
    } else {
      auto* ptr = member->encode();
      write(stream, ptr, member->encode_size());
      member->free_encode(ptr);
    }

    return true;
  }

  bool merge(const std::vector<cbuf_istream*>& inputs, const std::vector<std::string>& filter,
             bool filter_positive = true);
};

class cbuf_istream {
  friend class cbuf_ostream;
  std::map<uint64_t, std::string> dictionary;
  std::map<uint64_t, std::string> metadictionary;
  int stream = -1;
  unsigned char* memmap_ptr = nullptr;
  unsigned char* start_ptr = nullptr;
  unsigned char* ptr = nullptr;
  size_t rem_size = 0;
  size_t filesize = 0;

  uint64_t __get_next_hash() {
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->hash;
  }

  uint32_t __get_next_size() {
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->size();
  }

  uint8_t __get_next_variant() {
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->variant();
  }

  double __get_next_timestamp() {
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->packet_timest;
  }

  bool __check_next_preamble() {
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->magic == CBUF_MAGIC;
  }

  void updatePtrAndSize(size_t nsize) {
    // This is to handle corrupted cb files
    if (nsize > rem_size) {
      nsize = rem_size;
    }
    ptr += nsize;
    rem_size -= nsize;
  }

  /// Try to consume metadata packets, not exposed to clients
  /// Return true if a packet was consumed, false otherwise
  bool consume_internal();

public:
  cbuf_istream() {}
  ~cbuf_istream() { close(); }

  void close();

  bool open_file(const char* fname);

  bool open_memory(unsigned char* data, size_t length);

  bool open_socket(const char* ip, int port);

  template <class cbuf_struct>
  bool deserialize(cbuf_struct* member) {
    bool ret;

    if (empty()) return false;
    if (consume_internal()) {
      return deserialize(member);
    }

    auto nsize = get_next_size();
    if (member->supports_compact()) {
      ret = member->decode_net((char*)ptr, rem_size);
    } else {
      ret = member->decode((char*)ptr, rem_size);
    }
    if (!ret) return false;
    updatePtrAndSize(nsize);
    return true;
  }

  std::string get_string_for_hash(uint64_t hash) {
    if (dictionary.count(hash) > 0) {
      return dictionary[hash];
    }
    return std::string();
  }

  std::string get_meta_string_for_hash(uint64_t hash) {
    if (metadictionary.count(hash) > 0) {
      return metadictionary[hash];
    }
    return std::string();
  }

  uint64_t get_next_hash() {
    if (consume_internal()) {
      return get_next_hash();
    }
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->hash;
  }

  uint32_t get_next_size() {
    if (consume_internal()) {
      return get_next_size();
    }
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->size();
  }

  double get_next_timestamp() {
    if (consume_internal()) {
      return get_next_timestamp();
    }
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->packet_timest;
  }

  uint8_t get_next_variant() {
    if (consume_internal()) {
      return get_next_variant();
    }
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->variant();
  }

  // This function will check for valid preamble
  bool check_next_preamble() {
    if (consume_internal()) {
      return check_next_preamble();
    }
    return __check_next_preamble();
  }

  // Are there more objects on the stream?
  bool empty() const { return rem_size <= 0; }

  bool skip_message() {
    if (empty()) return false;
    if (consume_internal()) {
      return skip_message();
    }

    auto nsize = get_next_size();
    // corrupt cbuf
    if (nsize == 0) return false;
    updatePtrAndSize(nsize);
    return true;
  }

  bool skip_corrupted() {
    if (empty()) return true;
    if (__check_next_preamble() && (__get_next_size() > 0)) return true;

    while (rem_size >= sizeof(cbuf_preamble)) {
      ptr += 1;
      rem_size -= 1;
      if (__check_next_preamble() && (__get_next_size() > 0)) return true;
    }
    ptr += rem_size;
    rem_size = 0;
    return true;
  }

  unsigned char* get_current_ptr() { return ptr; }
  size_t get_filesize() const { return filesize; }
  size_t get_current_offset() const { return filesize - rem_size; }
  unsigned int get_next_magic() const {
    cbuf_preamble* pre = (cbuf_preamble*)ptr;
    return pre->magic;
  }

  void reset_ptr() {
    ptr = start_ptr;
    rem_size = filesize;
  }
};
