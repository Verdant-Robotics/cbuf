#pragma once
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include <metadata.h>
#include <error.h>
#include "cbuf_preamble.h"

class cbuf_ostream
{
  std::map<uint64_t, std::string> dictionary;
  int stream = -1;

  double now() const
  {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    double now = double(ts.tv_sec) + double(ts.tv_nsec)/1e9;
    return now;
  }

public:
  cbuf_ostream() {}
  ~cbuf_ostream() { close(); }

  void close()
  {
    if (stream != -1) {
      ::close(stream);
    }
    stream = -1;
  }

  bool open_file(const char *fname)
  {
    stream = open(fname, O_WRONLY | O_APPEND | O_CREAT, S_IRWXU);
    if (stream == -1) {
      perror("Error opening file ");
    }
    return stream != -1;
  }

  bool open_socket(const char *ip, int port)
  {
    return false;
  }

  template <class cbuf_struct>
  bool serialize(cbuf_struct * member)
  {
    // check if we have serialized this type before or not.
    if (!dictionary.count(member->hash())) {
      // If not, serialize its metadata
      cbufmsg::metadata mdata;
      mdata.preamble.packet_timest = now();
      mdata.msg_meta = member->cbuf_string;
      mdata.msg_hash = member->hash();
      mdata.msg_name = member->TYPE_STRING;
      char *ptr = mdata.encode();
      write(stream, ptr, mdata.encode_size());
      mdata.free_encode(ptr);
      dictionary[member->hash()] = member->TYPE_STRING;
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
  bool consume_internal()
  {
      bool ret;
      if (empty()) return false;

      auto hash = __get_next_hash();
      auto nsize = __get_next_size();

      if (hash == cbufmsg::metadata::TYPE_HASH ) {
        cbufmsg::metadata mdata;
        ret = mdata.decode((char *)ptr, rem_size);
        if (!ret) return false;
        ptr += nsize;
        rem_size -= nsize;
        dictionary[mdata.msg_hash] = mdata.msg_name;
        metadictionary[mdata.msg_hash] = mdata.msg_meta;

        return true;
      }
      return false;
  }

public:
  cbuf_istream() {}
  ~cbuf_istream() { close(); }

  void close()
  {
    if (memmap_ptr != nullptr) {
      munmap(memmap_ptr, filesize);
    }
    if (stream != -1) {
      ::close(stream);
    }
    stream = -1;
  }

  bool open_file(const char *fname)
  {
    stream = open(fname, O_RDONLY );
    if (stream == -1) {
      perror("Error opening file ");
      return false;
    }
    struct stat st;
    stat(fname, &st);
    filesize = st.st_size;
    memmap_ptr = (unsigned char *)mmap(NULL, filesize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, stream, 0);
    if (memmap_ptr == MAP_FAILED) {
      return false;
    }

    rem_size = filesize;
    ptr = memmap_ptr;
    return true;
  }

  bool open_socket(const char *ip, int port)
  {
    return false;
  }

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
