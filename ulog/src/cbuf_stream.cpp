#include "cbuf_stream.h"

#include <assert.h>
#include <cbuf_preamble.h>
#include <fcntl.h>
#include <metadata.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ulogger.h"

static double now() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  double now = double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
  return now;
}

void serialize_metadata_cbuf_ostream(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx) {
  cbuf_ostream* cos = static_cast<cbuf_ostream*>(ctx);
  cos->serialize_metadata(msg_meta, hash, msg_name);
}

void serialize_metadata_cbuf_cstream(const char* msg_meta, uint64_t hash, const char* msg_name, void* ctx) {
  cbuf_cstream* ccs = static_cast<cbuf_cstream*>(ctx);
  ccs->serialize_metadata(msg_meta, hash, msg_name);
}

double cbuf_ostream::now() const { return ::now(); }

ssize_t cbuf_ostream::file_offset() const {
  if (stream < 0) return -1;
#if defined(__linux__)
  return lseek64(stream, 0, SEEK_CUR);
#else
  return lseek(stream, 0, SEEK_CUR);
#endif
}

int cbuf_ostream::serialize_metadata(const char* msg_meta, uint64_t hash, const char* msg_name) {
  if (dictionary.count(hash) > 0) return 0;
  assert(hash != 0);

  cbufmsg::metadata mdata;
  mdata.preamble.packet_timest = now();
  mdata.msg_meta = msg_meta;
  mdata.msg_hash = hash;
  mdata.msg_name = msg_name;
  char* ptr = mdata.encode();
  char* write_ptr = ptr;
  int bytes_to_write = mdata.encode_size();
  int total_bytes_to_write = bytes_to_write;
  int error_count = 0;
  if (pre_file_write_callback_) {
    pre_file_write_callback_(FileWriteType::METADATA);
  }
  do {
    int result = write(stream, write_ptr, bytes_to_write);
    if (result > 0) {
      bytes_to_write -= result;
      write_ptr += result;
    } else {
      if (errno != EAGAIN) {
        perror("Cbuf serialize metadata writing error");
      }
      if (exit_early_on_write_failure) {
        mdata.free_encode(ptr);
        return errno;
      }
      error_count++;
      if (error_count > 10) {
        assert(false);
      }
    }
  } while (bytes_to_write > 0);
  if (file_write_callback_) {
    file_write_callback_(ptr, total_bytes_to_write, write_callback_usr_ptr_);
  }
  mdata.free_encode(ptr);
  dictionary[hash] = msg_name;
  return 0;
}

void cbuf_ostream::close() {
  if (stream != -1) {
    ::close(stream);
  }
  dictionary.clear();
  stream = -1;
}

bool cbuf_ostream::open_file(const char* fname) {
  stream =
      open(fname, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (stream == -1) {
    fprintf(stderr, "Could not open file %s for writing\n", fname);
    perror("Error opening file ");
    fname_.clear();
  } else {
    fname_ = fname;
  }
  return stream != -1;
}

bool cbuf_ostream::open_socket(const char* ip, int port) {
  (void)ip;
  (void)port;
  return false;
}

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
                                bool filter_positive, double earlytime, double latetime) {
  bool ret;
  bool isMeta = false;
  auto hash = cis->__get_next_hash();
  auto nsize = cis->__get_next_size();

  if (hash == cbufmsg::metadata::TYPE_HASH) {
    cbufmsg::metadata mdata;
    ret = mdata.decode((char*)cis->ptr, cis->rem_size);
    if (!ret) return false;

    isMeta = true;
    // Important, redefine the hash here so the filter works later on
    hash = mdata.msg_hash;

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
    if (dictionary.count(hash) == 0) {
      fprintf(stderr, "processing packet with hash 0x%" PRIX64 " does not have metadata\n", hash);
    }
  }

  auto& msg_name = dictionary.at(hash);
  // Here we filter packets, metadata and normal ones. If we do not want a type,
  // we skip its metadata and packets
  if (!name_passes_filter(filter, filter_positive, msg_name)) {
    // not an error, just skip
    cis->updatePtrAndSize(nsize);
    return true;
  }

  if (!isMeta) {
    // Time filters do not apply to metadata messages
    double packet_time = cis->__get_next_timestamp();
    if ((packet_time < earlytime) || (packet_time > latetime)) {
      // not an error, just skip
      cis->updatePtrAndSize(nsize);
      return true;
    }
  }
  auto num = write(stream, cis->ptr, nsize);
  if (num != nsize) {
    fprintf(stderr, "Error writing packet, wanted to write %d bytes but wrote %zd\n", nsize, num);
    return false;
  }
  if (file_write_callback_) {
    file_write_callback_(cis->ptr, nsize, write_callback_usr_ptr_);
  }
  cis->updatePtrAndSize(nsize);
  return true;
}

bool cbuf_ostream::merge(const std::vector<cbuf_istream*>& inputs, const std::vector<std::string>& filter,
                         bool filter_positive, double earlytime, double latetime) {
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
    ret = merge_packet(cis, filter, filter_positive, earlytime, latetime);
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

const char* cbuf_istream::get_or_search_string_for_hash(uint64_t hash) {
  if (metadictionary.count(hash) > 0) {
    return metadictionary[hash].c_str();
  }
  auto old_ptr = ptr;
  auto old_size = rem_size;
  while (!empty()) {
    auto msghash = __get_next_hash();
    auto nsize = __get_next_size();

    if (msghash == cbufmsg::metadata::TYPE_HASH) {
      cbufmsg::metadata mdata;
      bool ret = mdata.decode((char*)ptr, rem_size);
      assert(ret);
      (void)ret;
      ptr += nsize;
      rem_size -= nsize;
      dictionary[mdata.msg_hash] = mdata.msg_name;
      metadictionary[mdata.msg_hash] = mdata.msg_meta;

      if (mdata.msg_hash == hash) {
        ptr = old_ptr;
        rem_size = old_size;
        return metadictionary[mdata.msg_hash].c_str();
      }
    }
    // Consunme the current message since it was not what we looked for
    skip_message();
  }
  ptr = old_ptr;
  rem_size = old_size;
  return nullptr;
}

void cbuf_istream::close() {
  if (memmap_ptr != nullptr) {
    munmap((void*)memmap_ptr, filesize);
  }
  if (stream != -1) {
    ::close(stream);
  }
  stream = -1;
}

bool cbuf_istream::open_file(const char* fname) {
  // copy file name
  stream = open(fname, O_RDONLY);
  if (stream == -1) {
    perror("Error opening file ");
    return false;
  }
  struct stat st;
  stat(fname, &st);
  filesize = st.st_size;
  int flags = MAP_PRIVATE;
#if defined(__linux__)
  flags |= MAP_POPULATE;
#endif
  memmap_ptr = (unsigned char*)mmap(nullptr, filesize, PROT_READ, flags, stream, 0);
  if (memmap_ptr == MAP_FAILED) {
    return false;
  }

  rem_size = filesize;
  ptr = start_ptr = memmap_ptr;
  fname_ = fname;
  return true;
}

bool cbuf_istream::open_memory(const unsigned char* data, size_t length) {
  rem_size = filesize = length;
  ptr = start_ptr = data;
  return true;
}

bool cbuf_istream::open_socket(const char* ip, int port) {
  (void)ip;
  (void)port;
  return false;
}

double cbuf_cstream::now() const { return ::now(); }

void cbuf_cstream::serialize_metadata(const char* msg_meta, uint64_t hash, const char* msg_name) {
  if (dictionary.count(hash) > 0) return;
  assert(hash != 0);

  cbufmsg::metadata mdata;
  mdata.preamble.packet_timest = now();
  mdata.msg_meta = msg_meta;
  mdata.msg_hash = hash;
  mdata.msg_name = msg_name;
  int bytes_to_write = mdata.encode_size();
  unsigned char* write_ptr = mem_alloc_callback_(bytes_to_write, usr_ptr_);
  mdata.encode((char*)write_ptr, bytes_to_write);
  write_complete_callback_(write_ptr, bytes_to_write, usr_ptr_);
  dictionary[hash] = msg_name;
}
