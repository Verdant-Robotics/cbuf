#include "ulogger.h"

#include <memory.h>
#include <pthread.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <streambuf>
#include <unordered_map>

#if defined(__linux__)
#include <linux/limits.h>
#endif

static std::recursive_mutex g_file_mutex;
static std::mutex g_ulogger_mutex;
static bool initialized = false;
static ULogger* g_ulogger = nullptr;

namespace fs = std::filesystem;

int ULogger::getOrMakeTopicVariant(const uint64_t& message_hash, const uint64_t& topic_name_hash) {
  // get the vector for the give message hash
  auto topic_vector_it = cos.variant_dictionary.find(message_hash);
  if (topic_vector_it == cos.variant_dictionary.end()) {
    auto& topic_vector = cos.variant_dictionary[message_hash];
    topic_vector.push_back(topic_name_hash);
    return 1;
  }
  int iter;
  for (iter = 0; iter < topic_vector_it->second.size(); iter++) {
    if (topic_vector_it->second[iter] == topic_name_hash) break;
  }
  if (iter == topic_vector_it->second.size()) {
    topic_vector_it->second.push_back(topic_name_hash);
    return topic_vector_it->second.size();
  }
  // variant numbering starts from 1
  return iter + 1;
}

void ULogger::setLogPath(const std::string& path) {
  std::lock_guard guard(g_file_mutex);
  if (outputdir != path) {
    // Create the output directory if it does not exist
    std::error_code ec;
    if (!fs::exists(path.c_str())) {
      if (!fs::create_directories(path.c_str(), ec)) {
        reportError("Error: output directory does not exist and could not create it: " + path +
                    " Error: " + ec.message());
        return;
      }
    }

    outputdir = path;

    if (cos.is_open()) {
      closeFile();
    }
  }
}

std::string ULogger::getLogPath() const { return outputdir; }

// Glibc provides this
extern char* __progname;

static void name_thread() {
  char thread_name[16] = {};
  snprintf(thread_name, sizeof(thread_name), "ulog_%s", __progname);
#if defined(__APPLE__)
  pthread_setname_np(thread_name);
#else
  pthread_setname_np(pthread_self(), thread_name);
#endif
}

double ULogger::time_now() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  double now = double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
  return now;
}

void ULogger::reportError(const std::string& error) {
  if (error_callback_) {
    error_callback_(error);
  }
}

void ULogger::fillUlogFilename() {
  time_t rawtime;
  struct tm* info;
  time(&rawtime);
  info = localtime(&rawtime);

  char hostname[128] = {};
  gethostname(hostname, sizeof(hostname));
  char buffer[PATH_MAX];
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "%s.%s.%d.%02d.%02d.%02d_%02d_%02d.cb", __progname, hostname, info->tm_year + 1900,
          info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);

  if (outputdir.empty()) {
    outputdir = ".";
  }

  // Issue a warning here if outputdir is empty
  ulogfilename = getLogPath() + "/" + buffer;
  unsigned int suffix = 1;
  while (fs::exists(ulogfilename)) {
    sprintf(buffer, "%s.%s.%d.%02d.%02d.%02d_%02d_%02d_%d.cb", __progname, hostname, info->tm_year + 1900,
            info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec, suffix);
    suffix++;
    ulogfilename = getLogPath() + "/" + buffer;
  }
}

// Process packet here:
// Check the dictionary if needed for metadata
// Write the packet otherwise
void ULogger::processPacket(void* data, int size, const char* metadata, const char* type_name,
                            const uint64_t topic_name_hash) {
  cbuf_preamble* pre = (cbuf_preamble*)data;
  if (!cos.is_open()) {
    if (!openFile()) {
      reportError("Could not open the next ulog for writing");
      return;
    }
  }

  // Basic integrity checks
  if (pre->magic != CBUF_MAGIC) {
    reportError("Expected magic to be " + std::to_string(CBUF_MAGIC) + ", but it is " +
                std::to_string(pre->magic) + " for packet of size " + std::to_string(size) + ", type " +
                type_name + ", metadata: [[ " + metadata + " ]] ");
    return;
  }
  if (pre->hash == 0) {
    reportError("Expected hash to be non-zero for packet of size " + std::to_string(size) + ", type " +
                type_name + ", metadata: [[ " + metadata + " ]] ");
    return;
  }
  if (pre->size() == 0) {
    reportError("Expected size to be non-zero for packet of size " + std::to_string(size) + ", type " +
                type_name + ", metadata: [[ " + metadata + " ]] ");
    return;
  }
  if (pre->size() != size) {
    reportError("Ulogger, writing " + std::to_string(size) + " bytes but the cbuf reports it has " +
                std::to_string(pre->size()) + " bytes");
    return;
  }

  // if metadata for this type of message not already serialized then serialize it
  if (cos.dictionary.count(pre->hash) == 0) {
    cos.serialize_metadata(metadata, pre->hash, type_name);
  }

  // set the variant of this message add a item to dictionary if it does not already exist
  if (topic_name_hash != 0) {
    pre->setVariant(getOrMakeTopicVariant(pre->hash, topic_name_hash));
  } else {
    pre->setVariant(0);
  }

  int bytes_to_write = size;
  char* write_ptr = (char*)data;
  int error_count = 0;
  do {
    int result = write(cos.stream, write_ptr, bytes_to_write);
    if (result > 0) {
      bytes_to_write -= result;
      write_ptr += result;
    } else {
      if (errno != EAGAIN) {
        reportError("Cbuf writing error " + std::to_string(errno) + ": " + strerror(errno));
      }
      error_count++;
      if (error_count > 10) {
        assert(false);
      }
    }
  } while (bytes_to_write > 0);

  if (file_write_callback_) {
    file_write_callback_(data, size);
  }

  current_file_size += size;

  // Paranoia
  if (pre->magic != CBUF_MAGIC) {
    reportError("Expected magic to be " + std::to_string(CBUF_MAGIC) + ", but it is " +
                std::to_string(pre->magic) + " for packet of size " + std::to_string(size) + ", type " +
                type_name + ", metadata: [[ " + metadata + " ]] ");
    return;
  }
  if (pre->hash == 0) {
    reportError("Expected hash to be non-zero for packet of size " + std::to_string(size) + ", type " +
                type_name + ", metadata: [[ " + metadata + " ]] ");
    return;
  }
  if (pre->size() == 0) {
    reportError("Expected size to be non-zero for packet of size " + std::to_string(size) + ", type " +
                type_name + ", metadata: [[ " + metadata + " ]] ");
    return;
  }

  if (current_file_size > SPLIT_FILE_SIZE) {
    closeFile();
    bool r = openFile();
    if (!r) {
      reportError("Could not open the next ulog for writing");
      return;
    }
  }
}

// return a copy of the filename, not a reference
std::string ULogger::getCurrentUlogPath() {
  std::lock_guard guard(g_file_mutex);
  std::string result = ulogfilename;
  return result;
}

static void write_callback(const void* ptr, size_t bytes, void* usr_ptr) {
  ULogger* ulogger = (ULogger*)usr_ptr;
  if (ulogger->getFileWriteCallback()) {
    ulogger->getFileWriteCallback()(ptr, bytes);
  }
}

bool ULogger::openFile() {
  std::lock_guard guard(g_file_mutex);

  // Create the output directory if it does not exist
  std::error_code ec;
  if (!fs::exists(outputdir.c_str())) {
    if (!fs::create_directories(outputdir.c_str(), ec)) {
      reportError("Error: output directory does not exist and could not create it: " + outputdir +
                  " Error: " + ec.message());
      return false;
    }
  }

  fillUlogFilename();

  // Open the serialization file
  bool bret = cos.open_file(ulogfilename.c_str());
  if (!bret) {
    reportError("Could not open the ulog file for logging: " + ulogfilename);
    return false;
  }
  current_file_size = 0;
  if (file_write_callback_) {
    cos.setFileWriteCallback(write_callback, this);
  }
  if (file_open_callback_) {
    file_open_callback_(ulogfilename);
  }

  return true;
}

void ULogger::setFileWriteCallback(std::function<void(const void*, size_t)> cb, std::string& file_path,
                                   size_t& offset) {
  std::lock_guard guard(g_file_mutex);
  file_write_callback_ = cb;
  cos.setFileWriteCallback(write_callback, this);
  if (cos.is_open()) {
    fsync(cos.stream);
    file_path = cos.filename();
    offset = static_cast<size_t>(cos.file_offset());
  }
}

void ULogger::resetFileCallbacks() {
  std::lock_guard guard(g_file_mutex);
  file_close_callback_ = std::function<void(const std::string&)>();
  file_open_callback_ = std::function<void(const std::string&)>();
  file_write_callback_ = std::function<void(const void*, size_t)>();
}

void ULogger::closeFile() {
  std::string fname;
  {
    std::lock_guard guard(g_file_mutex);
    fname = cos.filename();
    cos.close();
  }
  if (file_close_callback_) {
    file_close_callback_(fname);
  }
}

void ULogger::endLoggingThread() {
  quit_thread = true;
  // uint64_t buffer_handle = ringbuffer.alloc(0, nullptr, nullptr);
  // ringbuffer.populate(buffer_handle);
  loggerThread->join();
  delete loggerThread;
  loggerThread = nullptr;
}

void ULogger::initialize() {
  loggerThread = new std::thread([this]() {
    name_thread();
    while (!this->quit_thread) {
      if (ringbuffer.size() == 0) {
        usleep(1000);
        continue;
      }
      auto r = ringbuffer.lastUnread();

      if (!r) {
        usleep(1000);
        continue;
      }

      if (!cos.is_open()) {
        if (!openFile()) {
          return;
        }
      }

      if (r->size > 0) {
        processPacket(r->loc, r->size, r->metadata, r->type_name, r->topic_name_hash);
      }
      ringbuffer.dequeue();
    }

    // Continue processing the queue until it is empty
    while (ringbuffer.size() > 0) {
      auto r = ringbuffer.lastUnread();

      if (!r) {
        usleep(1000);
        continue;
      }

      if (r->size > 0) {
        processPacket(r->loc, r->size, r->metadata, r->type_name, r->topic_name_hash);
      }
      ringbuffer.dequeue();
    }

    closeFile();
  });
}

// No public constructors, this is a singleton
ULogger* ULogger::getULogger() {
  if (!initialized) {
    std::lock_guard guard(g_ulogger_mutex);
    if (initialized) return g_ulogger;

    g_ulogger = new ULogger();
    g_ulogger->quit_thread = false;
    g_ulogger->initialize();
    initialized = true;
  }
  return g_ulogger;
}

/// function to stop all logging, threads, and terminate the app
void ULogger::endLogging() {
  if (g_ulogger == nullptr) {
    return;
  }
  g_ulogger->endLoggingThread();
  delete g_ulogger;
  g_ulogger = nullptr;
  initialized = false;
}

bool ULogger::serialize_bytes(const uint8_t* msg_bytes, size_t message_size, const char* type_name,
                              const char* metadata, const uint64_t topic_name_hash) {
  if (quit_thread) return false;

  if (!logging_enabled) return true;
  uint64_t buffer_handle = ringbuffer.alloc(message_size, metadata, type_name, topic_name_hash);
  char* ringbuffer_mem = (char*)ringbuffer.handleToAddress(buffer_handle);
  memcpy(ringbuffer_mem, msg_bytes, message_size);
  cbuf_preamble* pre = (cbuf_preamble*)ringbuffer_mem;
  pre->packet_timest = time_now();
  ringbuffer.populate(buffer_handle);
  return true;
}
