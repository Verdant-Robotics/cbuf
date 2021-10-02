#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "cbuf_preamble.h"
#include "cbuf_stream.h"
#include "ringbuffer.h"
#include "vlog.h"

class ULogger {
  ULogger()
      : quit_thread(false) {}
  ~ULogger() = default;

  static constexpr uint64_t SPLIT_FILE_SIZE = 1024 * 1024 * 1024;  // 1GB

  RingBuffer<1024 * 1024 * 100> ringbuffer;
  std::thread* loggerThread = nullptr;
  std::function<void(const std::string&)> file_close_callback_;

  // this is the folder where all the log files go to
  std::string outputdir;

  // this is the current cbuf filename we are writing to
  std::string ulogfilename;

  uint64_t current_file_size = 0;
  uint32_t file_check_count = 0;

  bool initialize();
  cbuf_ostream cos;
  bool quit_thread;
  bool logging_enabled = true;

  void processPacket(void* data, int size, const char* metadata, const char* type_name);

  void fillUlogFilename();
  bool openFile();
  void closeFile();
  void endLoggingThread();

  double time_now();

public:
  std::string getCurrentUlogPath();
  std::string getLogPath() const;

  void setLogPath(const std::string& path);
  void setUlogFilename(const std::string& path);

  // No public constructors, this is a singleton
  static ULogger* getULogger();

  /// function to stop all logging, threads, and terminate the app
  static void endLogging();

  bool getLoggingEnabled() const { return logging_enabled; }
  void setLoggingEnabled(bool enable) { logging_enabled = enable; }
  void setFileCloseCallback(std::function<void(const std::string&)> cb) { file_close_callback_ = cb; }

  /// This function will serialize to a buffer and then queue for the thread to
  /// write to disk
  template <class cbuf_struct>
  bool serialize(cbuf_struct* member) {
    if (quit_thread) return false;

    if (!logging_enabled) return true;

    unsigned int stsize;
    if (member->supports_compact()) {
      stsize = (unsigned int)member->encode_net_size();
    } else {
      stsize = (unsigned int)member->encode_size();
    }
    // Always set the size, dynamic cbufs usually do not have it set
    member->preamble.setSize(stsize);

    uint64_t buffer_handle = ringbuffer.alloc(stsize, member->cbuf_string, member->TYPE_STRING);
    if (member->preamble.magic != CBUF_MAGIC) {
      // Some packets skip default initialization, ensure it here
      member->preamble.magic = CBUF_MAGIC;
      member->preamble.hash = member->hash();
    }

    cbuf_preamble* pre = &member->preamble;
    VLOG_ASSERT(pre->magic == CBUF_MAGIC, "Expected magic to be %X, but it is %X", CBUF_MAGIC, pre->magic);
    VLOG_ASSERT(pre->hash == member->hash(), "Expected hash to be %lX, but it is %lX", member->hash(),
                pre->hash);
    VLOG_ASSERT(pre->size() != 0);

    member->preamble.packet_timest = time_now();
    char* ringbuffer_mem = (char*)ringbuffer.handleToAddress(buffer_handle);

    if (member->supports_compact()) {
      if (!member->encode_net(ringbuffer_mem, stsize)) {
        ringbuffer.populate(buffer_handle);
        VLOG_ASSERT(false, "Encode net failed for message : %s ", member->TYPE_STRING);
        return false;
      }
    } else {
      if (!member->encode(ringbuffer_mem, stsize)) {
        ringbuffer.populate(buffer_handle);
        VLOG_ASSERT(false, "Encode failed for message : %s ", member->TYPE_STRING);
        return false;
      }
    }

    // Check again
    pre = (cbuf_preamble*)ringbuffer_mem;
    VLOG_ASSERT(pre->magic == CBUF_MAGIC, "Expected magic to be %X, but it is %X", CBUF_MAGIC, pre->magic);
    VLOG_ASSERT(pre->hash == member->hash(), "Expected hash to be %lX, but it is %lX", member->hash(),
                pre->hash);
    VLOG_ASSERT(pre->size() != 0);

    ringbuffer.populate(buffer_handle);
    return true;
  }

  template <class cbuf_struct>
  bool serialize(cbuf_struct& member) {
    return serialize(&member);
  }
};
