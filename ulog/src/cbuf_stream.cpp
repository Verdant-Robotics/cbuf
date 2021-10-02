#include "cbuf_stream.h"

#include <assert.h>
#include <fcntl.h>
#include <metadata.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <map>

#include "cbuf_preamble.h"

static double now() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  double now = double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
  return now;
}

void serialize_metadata_cbuf(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx) {
  cbuf_ostream* cos = static_cast<cbuf_ostream*>(ctx);
  cos->serialize_metadata(msg_meta, hash, msg_name);
}

double cbuf_ostream::now() const { return ::now(); }

void cbuf_ostream::serialize_metadata(const char* msg_meta, uint64_t hash, const char* msg_name) {
  if (dictionary.count(hash) > 0) return;
  assert(hash != 0);

  cbufmsg::metadata mdata;
  mdata.preamble.packet_timest = now();
  mdata.msg_meta = msg_meta;
  mdata.msg_hash = hash;
  mdata.msg_name = msg_name;
  char* ptr = mdata.encode();
  char* write_ptr = ptr;
  int bytes_to_write = mdata.encode_size();
  int error_count = 0;
  do {
    int result = write(stream, write_ptr, bytes_to_write);
    if (result > 0) {
      bytes_to_write -= result;
      write_ptr += result;
    } else {
      if (errno != EAGAIN) {
        perror("Cbuf serialize metadata writing error");
      }
      error_count++;
      if (error_count > 10) {
        assert(false);
      }
    }
  } while (bytes_to_write > 0);
  mdata.free_encode(ptr);
  dictionary[hash] = msg_name;
}

void cbuf_ostream::close() {
  if (stream != -1) {
    ::close(stream);
  }
  dictionary.clear();
  stream = -1;
}

bool cbuf_ostream::open_file(const char* fname) {
  stream = open(fname, O_WRONLY | O_APPEND | O_CREAT, S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (stream == -1) {
    fprintf(stderr, "Could not open file %s for writing\n", fname);
    perror("Error opening file ");
    fname_.clear();
  } else {
    fname_ = fname;
  }
  return stream != -1;
}

bool cbuf_ostream::open_socket(const char* ip, int port) { return false; }

static void split_namespace(const std::string& full_name, std::string& spname, std::string& name) {
  std::string::size_type pos = 0;

  pos = full_name.find("::", pos);
  if (pos == std::string::npos) {
    spname = "";
    name = full_name;
  } else {
    spname = full_name.substr(0, pos);
    name = full_name.substr(pos + 2);
  }
}

// Partial name here is a string entered by a user in the command line, can be missing the namespace
// Full_name comes from the cbuf parser and is fully qualified
static bool match_names(const std::string& full_name, const std::string& partial_name) {
  std::string full_spname, full_regname, partial_spname, partial_regname;
  split_namespace(full_name, full_spname, full_regname);
  split_namespace(partial_name, partial_spname, partial_regname);

  if (!partial_spname.empty()) {
    if (partial_spname != full_spname) {
      // If we have namespaces, they have to match
      return false;
    }
  }

  // If we did not provide a namespace, only try to match the regular name
  return partial_regname == full_regname;
}

static bool name_passes_filter(const std::vector<std::string>& filter, bool filter_positive,
                               const std::string& msg_name) {
  // no filter, everything goes
  if (filter.empty()) return true;

  bool name_matched = false;

  for (const auto& filter_name : filter) {
    if (match_names(msg_name, filter_name)) {
      name_matched = true;
      break;
    }
  }

  return filter_positive == name_matched;
}

bool cbuf_ostream::merge_packet(cbuf_istream* cis, const std::vector<std::string>& filter,
                                bool filter_positive) {
  bool ret;
  auto hash = cis->__get_next_hash();
  auto nsize = cis->__get_next_size();

  if (hash == cbufmsg::metadata::TYPE_HASH) {
    // we merge all metadata, checking that there are no dupes or mismatches
    cbufmsg::metadata mdata;
    ret = mdata.decode((char*)cis->ptr, cis->rem_size);
    if (!ret) return false;

    if (dictionary.count(mdata.msg_hash) == 0) {
      dictionary[mdata.msg_hash] = mdata.msg_name;
      metadictionary[mdata.msg_hash] = mdata.msg_meta;
    } else {
      if (metadictionary[mdata.msg_hash] != mdata.msg_meta) {
        fprintf(stderr, "Packet type %s redefinition!\n", mdata.msg_name.c_str());
        return false;
      }
    }
  } else {
    // This is a normal packet, apply filters
    if (dictionary.count(hash) == 0) {
      fprintf(stderr, "processing packet with hash 0x%lX does not have metadata\n", hash);
    } else {
      auto& msg_name = dictionary.at(hash);
      if (!name_passes_filter(filter, filter_positive, msg_name)) {
        // not an error, just skip
        cis->updatePtrAndSize(nsize);
        return true;
      }
    }
  }

  write(stream, cis->ptr, nsize);
  cis->updatePtrAndSize(nsize);
  return true;
}

bool cbuf_ostream::merge(const std::vector<cbuf_istream*>& inputs, const std::vector<std::string>& filter,
                         bool filter_positive) {
  bool ret;
  if (!is_open()) {
    return false;
  }

  if (inputs.size() == 0) {
    return false;
  }

  while (true) {
    // find the input with the earliest timestamp
    cbuf_istream* cis = nullptr;
    double early_ts = -1;
    for (auto& ci : inputs) {
      // skip inputs that have no more data
      if (ci->empty()) continue;
      double ci_ts = ci->__get_next_timestamp();
      if ((early_ts < 0) || (ci_ts < early_ts)) {
        early_ts = ci_ts;
        cis = ci;
      }
    }
    if (cis == nullptr) {
      // no more work, exit
      break;
    }
    // process the earliest packet
    ret = merge_packet(cis, filter, filter_positive);
    if (!ret) {
      return false;
    }
  }

  return true;
}

/// Try to consume metadata packets, not exposed to clients
/// Return true if a packet was consumed, false otherwise
bool cbuf_istream::consume_internal() {
  bool ret;
  if (empty()) return false;

  auto hash = __get_next_hash();
  auto nsize = __get_next_size();
  if (!__check_next_preamble()) return false;

  if (hash == cbufmsg::metadata::TYPE_HASH) {
    cbufmsg::metadata mdata;
    ret = mdata.decode((char*)ptr, rem_size);
    if (!ret) return false;
    ptr += nsize;
    rem_size -= nsize;
    dictionary[mdata.msg_hash] = mdata.msg_name;
    metadictionary[mdata.msg_hash] = mdata.msg_meta;

    return true;
  }
  return false;
}

void cbuf_istream::close() {
  if (memmap_ptr != nullptr) {
    munmap(memmap_ptr, filesize);
  }
  if (stream != -1) {
    ::close(stream);
  }
  stream = -1;
}

bool cbuf_istream::open_file(const char* fname) {
  stream = open(fname, O_RDONLY);
  if (stream == -1) {
    perror("Error opening file ");
    return false;
  }
  struct stat st;
  stat(fname, &st);
  filesize = st.st_size;
  memmap_ptr = (unsigned char*)mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, stream, 0);
  if (memmap_ptr == MAP_FAILED) {
    return false;
  }

  rem_size = filesize;
  ptr = start_ptr = memmap_ptr;
  return true;
}

bool cbuf_istream::open_memory(unsigned char* data, size_t length) {
  rem_size = filesize = length;
  ptr = start_ptr = data;
  return true;
}

bool cbuf_istream::open_socket(const char* ip, int port) { return false; }
