#include "cbuf_stream.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>
#include <metadata.h>
#include <error.h>
#include <assert.h>
#include "cbuf_preamble.h"

class ULogger;

static double now()
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  double now = double(ts.tv_sec) + double(ts.tv_nsec)/1e9;
  return now;
}

double cbuf_ostream::now() const
{
  return ::now();
}

void cbuf_ostream::serialize_metadata(const char *msg_meta, uint64_t hash, const char *msg_name)
{
  assert(dictionary.count(hash) == 0);

  cbufmsg::metadata mdata;
  mdata.preamble.packet_timest = now();
  mdata.msg_meta = msg_meta;
  mdata.msg_hash = hash;
  mdata.msg_name = msg_name;
  char *ptr = mdata.encode();
  write(stream, ptr, mdata.encode_size());
  mdata.free_encode(ptr);
  dictionary[hash] = msg_name;
}

void cbuf_ostream::close()
{
  if (stream != -1) {
    ::close(stream);
  }
  stream = -1;
}

bool cbuf_ostream::open_file(const char *fname)
{
  stream = open(fname, O_WRONLY | O_APPEND | O_CREAT,
                S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
  if (stream == -1) {
    perror("Error opening file ");
  }
  return stream != -1;
}

bool cbuf_ostream::open_socket(const char *ip, int port)
{
  return false;
}




/// Try to consume metadata packets, not exposed to clients
/// Return true if a packet was consumed, false otherwise
bool cbuf_istream::consume_internal()
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

void cbuf_istream::close()
{
  if (memmap_ptr != nullptr) {
    munmap(memmap_ptr, filesize);
  }
  if (stream != -1) {
    ::close(stream);
  }
  stream = -1;
}

bool cbuf_istream::open_file(const char *fname)
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

bool cbuf_istream::open_socket(const char *ip, int port)
{
  return false;
}
