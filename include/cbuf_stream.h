#pragma once
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include <metadata.h>
#include <error.h>

class cbuf_ostream
{
  std::map<uint64_t, std::string> dictionary;
  int stream = -1;

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
      mdata.msg_meta = member->cbuf_string;
      mdata.msg_hash = member->hash();
      mdata.msg_name = member->TYPE_STRING;
      char *ptr = mdata.encode();
      write(stream, ptr, mdata.encode_size());
      mdata.free_encode(ptr);
      dictionary[member->hash()] = member->TYPE_STRING;
    }

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
  int stream = -1;
  unsigned char *memmap_ptr = nulltpr;
  unsigned char *ptr = nullptr;
  size_t rem_size = 0;
  size_t filesize = 0;

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


    // Serialize the data of the member itself
    char* ptr = member->encode();
    write(stream, ptr, member->encode_size());
    member->free_encode(ptr);

    return true;
  }

  std::string get_next_string()
  {
    std::string str;
    return str;
  }

  uint64_t get_next_hash()
  {
    uint64_t hash;
    return hash;
  }

  // Are there more objects on the stream?
  bool empty()
  {
    return rem_size == 0;
  }
};
