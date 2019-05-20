#pragma once
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>

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
    stream = open(fname, O_WRONLY | O_APPEND, O_CREAT);
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
    // If not, serialize its metadata

    // Serialize the data of the member itself
    char* ptr = member->encode();
    write(stream, ptr, member->encode_size());
    member->free_encode(ptr);

    return true;
  }
};
