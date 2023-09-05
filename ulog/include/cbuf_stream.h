#pragma once
#include <assert.h>
#include <unistd.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "cbuf_preamble.h"

class ULogger;
class cbuf_istream;

void serialize_metadata_cbuf_ostream(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx);
void serialize_metadata_cbuf_cstream(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx);

using file_write_callback_t = std::function<void(const void*, size_t, void*)>;

enum class FileWriteType {
  METADATA,
  DATA,
};

using pre_file_write_callback_t = std::function<void(FileWriteType)>;

// Note: these classes will compact whenever possible, to work with ulogger
class cbuf_ostream {
  // This is a dictionary which maps the message type hash to message type string
  std::map<uint64_t, std::string> dictionary;
  // This is a dictionary which maps the topic type hash to its list of sorted topics with same type
  std::map<uint64_t, std::vector<uint64_t>> variant_dictionary;
  std::map<uint64_t, std::string> metadictionary;  // used when merging

  pre_file_write_callback_t pre_file_write_callback_ = nullptr;
  file_write_callback_t file_write_callback_ = nullptr;
  void* write_callback_usr_ptr_ = nullptr;

  std::string fname_;
  int stream = -1;

  double now() const;

  friend class ULogger;

  friend void serialize_metadata_cbuf_ostream(const char* msg_meta, uint64_t hash, const char* msg_name,
                                              void* ctx);

  bool merge_packet(cbuf_istream* cis, const std::vector<std::string>& filter, bool filter_positive,
                    double earlytime, double latetime);

public:
  cbuf_ostream() {}
  ~cbuf_ostream() { close(); }

  // Sets the handle used by the ostream to an externally created handle.
  // Can be a file that was previously opened, a network socket, or something else (that can be written to
  // with write() ) Assumes "ownership" over the open call to the handle - ie:
  //   code wanting to discard the handle afterwards might look like
  //          int fd = open('my_file.cbuf', O_APPEND);
  //          cbuf_serializer_.attach_handle(upload_socket_.get_raw_socket());
  //          fd = -1; // no longer using this handle
  //   but code wanting to continue to use the handle may look like
  //          int fd = open('my_file.cbuf', O_APPEND);
  //          cbuf_serializer_.attach_handle(dup(upload_socket_.get_raw_socket()));
  //          ... // do something with the handle
  //          close(fd);
  //          fd = -1;
  void attach_handle(int handle) {
    assert(stream == -1);
    stream = handle;
  }

  void close();

  bool is_open() const { return stream != -1; }

  bool open_file(const char* fname);

  bool open_socket(const char* ip, int port);

  const std::string& filename() const { return fname_; }

  ssize_t file_offset() const;

  void setFileWriteCallback(file_write_callback_t cb, void* user_ptr) {
    file_write_callback_ = cb;
    write_callback_usr_ptr_ = user_ptr;
  }

  void setPreFileWriteCallback(pre_file_write_callback_t cb) { pre_file_write_callback_ = cb; }

  template <class cbuf_struct>
  bool serialize(cbuf_struct* member) {
    // check if we have serialized this type before or not.
    if (!dictionary.count(member->hash())) {
      // If not, serialize its metadata
      member->handle_metadata(serialize_metadata_cbuf_ostream, this);
    }

    member->preamble.packet_timest = now();

    if (pre_file_write_callback_) {
      pre_file_write_callback_(FileWriteType::DATA);
    }
    // Serialize the data of the member itself
    if (member->supports_compact()) {
      auto ns = member->encode_net_size();
      char* ptr = (char*)malloc(ns);
      member->encode_net(ptr, ns);
      auto n = write(stream, ptr, ns);
      (void)n;
      free(ptr);
    } else {
      auto* ptr = member->encode();
      auto n = write(stream, ptr, member->encode_size());
      (void)n;
      member->free_encode(ptr);
    }

    return true;
  }

  // serialize_metadata:
  //  writes the metadata for a message type to file
  // msg_meta:
  //  the serialized metadata to write
  // hash:
  //  the hash of the metadata (used to identify it)
  // msg_name:
  //  the message type name corresponding to the metadata
  // returns:
  //  0 in case of success
  //  0 if the metadata has already been written to this stream
  //  a standard errno code in case of failure
  int serialize_metadata(const char* msg_meta, uint64_t hash, const char* msg_name);

  bool merge(const std::vector<cbuf_istream*>& inputs, const std::vector<std::string>& filter,
             bool filter_positive = true, double earlytime = -1, double latetime = 10E12);

  bool exit_early_on_write_failure = false;
};

// function definition to allocate a number of bytes, with a user context
typedef unsigned char* (*mem_alloc_callback_t)(size_t, void*);
// Function definition to notify a write has completed
typedef void (*write_complete_callback_t)(unsigned char*, size_t, void*);

// This class serializes cbuf into a callback stream
class cbuf_cstream {
  // This is a dictionary which maps the message type hash to message type string
  std::map<uint64_t, std::string> dictionary;
  // This is a dictionary which maps the topic type hash to its list of sorted topics with same type
  std::map<uint64_t, std::vector<uint64_t>> variant_dictionary;

  mem_alloc_callback_t mem_alloc_callback_ = nullptr;
  write_complete_callback_t write_complete_callback_ = nullptr;
  void* usr_ptr_ = nullptr;

  double now() const;

  friend void serialize_metadata_cbuf_cstream(const char* msg_meta, uint64_t hash, const char* msg_name,
                                              void* ctx);
  void serialize_metadata(const char* msg_meta, uint64_t hash, const char* msg_name);

public:
  cbuf_cstream() {}
  ~cbuf_cstream() {}

  void setCallbacks(mem_alloc_callback_t memcb, write_complete_callback_t completecb, void* user_ptr) {
    mem_alloc_callback_ = memcb;
    write_complete_callback_ = completecb;
    usr_ptr_ = user_ptr;
  }

  template <class cbuf_struct>
  bool serialize(cbuf_struct* member) {
    // check if we have serialized this type before or not.
    if (!dictionary.count(member->hash())) {
      // If not, serialize its metadata
      member->handle_metadata(serialize_metadata_cbuf_cstream, this);
    }

    unsigned int stsize;
    if (member->supports_compact()) {
      stsize = (unsigned int)member->encode_net_size();
    } else {
      stsize = (unsigned int)member->encode_size();
    }
    // Always set the size, dynamic cbufs usually do not have it set
    member->preamble.setSize(stsize);
    member->preamble.packet_timest = now();

    // Serialize the data of the member itself
    char* ptr = (char*)mem_alloc_callback_(stsize, usr_ptr_);
    if (member->supports_compact()) {
      member->encode_net(ptr, stsize);
    } else {
      member->encode(ptr, stsize);
    }
    write_complete_callback_((unsigned char*)ptr, stsize, usr_ptr_);

    return true;
  }
};

class cbuf_istream {
  friend class cbuf_ostream;
  std::map<uint64_t, std::string> dictionary;
  std::map<uint64_t, std::string> metadictionary;
  int stream = -1;
  const unsigned char* memmap_ptr = nullptr;
  const unsigned char* start_ptr = nullptr;
  const unsigned char* ptr = nullptr;
  size_t rem_size = 0;
  size_t filesize = 0;
  bool consume_on_deserialize = true;
  std::string fname_ = "";

  uint64_t __get_next_hash() const {
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
    return pre->hash;
  }

  uint32_t __get_next_size() const {
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
    return pre->size();
  }

  uint8_t __get_next_variant() const {
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
    return pre->variant();
  }

  double __get_next_timestamp() const {
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
    return pre->packet_timest;
  }

  bool __check_next_preamble() const {
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
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

  void disable_consume_on_deserialize() { consume_on_deserialize = false; }
  void close();

  bool open_file(const char* fname);

  const std::string& filename() const { return fname_; }
  // Mainly used in memory open case
  void set_filename(const char* fname) { fname_ = fname; }

  bool open_memory(const unsigned char* data, size_t length);

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
    if (consume_on_deserialize) updatePtrAndSize(nsize);
    return true;
  }

  std::string get_string_for_hash(uint64_t hash) {
    const auto it = dictionary.find(hash);
    if (it != dictionary.end()) {
      return it->second;
    }
    return std::string();
  }

  const char* get_cstring_for_hash(uint64_t hash) {
    const auto it = dictionary.find(hash);
    if (it != dictionary.end()) {
      return it->second.c_str();
    }
    return nullptr;
  }

  std::string get_meta_string_for_hash(uint64_t hash) {
    const auto it = metadictionary.find(hash);
    if (it != metadictionary.end()) {
      return it->second;
    }
    return std::string();
  }

  const char* get_meta_cstring_for_hash(uint64_t hash) {
    const auto it = metadictionary.find(hash);
    if (it != metadictionary.end()) {
      return it->second.c_str();
    }
    return nullptr;
  }

  // Find the meta string for a hash, search forward if we
  // have not seen this yet (could happen on replayer)
  const char* get_or_search_string_for_hash(uint64_t hash);

  uint64_t get_next_hash() {
    if (consume_internal()) {
      return get_next_hash();
    }
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
    return pre->hash;
  }

  uint32_t get_next_size() {
    if (consume_internal()) {
      return get_next_size();
    }
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
    return pre->size();
  }

  double get_next_timestamp() {
    if (consume_internal()) {
      return get_next_timestamp();
    }
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
    return pre->packet_timest;
  }

  uint8_t get_next_variant() {
    if (consume_internal()) {
      return get_next_variant();
    }
    const cbuf_preamble* pre = (const cbuf_preamble*)ptr;
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
  // Return if there is any data after internal consumption
  bool empty_no_internal() {
    consume_internal();
    return empty();
  }

  size_t get_remaining_size() const { return rem_size; }

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

  const unsigned char* get_current_ptr() const { return ptr; }
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

  bool jump_to_offset(size_t offset) {
    if (offset <= filesize) {
      ptr = start_ptr + offset;
      rem_size = filesize - offset;

      return true;
    }
    return false;
  }
};
