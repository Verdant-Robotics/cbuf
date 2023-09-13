#pragma once

#include <atomic>
#include <cinttypes>
#include <climits>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "cbuf_preamble.h"
#include "cbuf_stream.h"
#include "ringbuffer.h"

// Ulogger is a singleton class to log to a file, using cbuf serialization.
// Example usage:
//   ULogger::getULogger()->setLogPath(path);
//   ULogger::getULogger()->serialize(img1);
//   ULogger::getULogger()->serialize(img2);
//   ULogger::endLogging();
class ULogger {
  ULogger()
      : quit_thread(false) {}
  ~ULogger() = default;

  static constexpr uint64_t SPLIT_FILE_SIZE = 200 * 1024 * 1024;  // 200MB

  RingBuffer<1024 * 1024 * 100> ringbuffer;
  std::thread* loggerThread = nullptr;
  std::function<void(const std::string&)> file_close_callback_;
  std::function<void(const std::string&)> file_open_callback_;
  std::function<void(const void*, size_t)> file_write_callback_;
  std::function<void(const std::string&)> error_callback_;

  // this is the folder where all the log files go to
  std::string outputdir;

  // this is the current cbuf filename we are writing to
  std::string ulogfilename;

  uint64_t current_file_size = 0;

  void initialize();
  cbuf_ostream cos;
  bool quit_thread;
  bool logging_enabled = true;

  void processPacket(void* data, int size, const char* metadata, const char* type_name,
                     const uint64_t topic_name_hash);

  void fillUlogFilename();
  bool openFile();
  void closeFile();
  void endLoggingThread();

  double time_now();

  void reportError(const std::string& error);

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
  // Set the write callback but return the current (if existent) ulog filename
  void setFileWriteCallback(std::function<void(const void*, size_t)> cb, std::string& file_path,
                            size_t& offset);
  auto getFileWriteCallback() { return file_write_callback_; }
  void setFileOpenCallback(std::function<void(const std::string&)> cb) { file_open_callback_ = cb; }
  void resetFileCallbacks();
  void setErrorCallback(std::function<void(const std::string&)> cb) { error_callback_ = cb; }

  // gets a topic variant if it exists or adds one and then gets the variant if it does not exist
  int getOrMakeTopicVariant(const uint64_t& message_hash, const uint64_t& topic_name_hash);

  /// Serialize to disk when we only have the bytes of the message, this is mainly used
  /// during replay
  bool serialize_bytes(const uint8_t* msg_bytes, size_t message_size, const char* type_name,
                       const char* metadata, const uint64_t topic_name_hash);

  /// This function will serialize to a buffer and then queue for the thread to
  /// write to disk
  template <class cbuf_struct>
  bool serialize(cbuf_struct* member, const uint64_t topic_name_hash = 0) {
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
    /* member->preamble.setVariant(1); */

    uint64_t buffer_handle =
        ringbuffer.alloc(stsize, member->cbuf_string, member->TYPE_STRING, topic_name_hash);
    if (member->preamble.magic != CBUF_MAGIC) {
      // Some packets skip default initialization, ensure it here
      member->preamble.magic = CBUF_MAGIC;
      member->preamble.hash = member->hash();
    }

    cbuf_preamble* pre = &member->preamble;
    if (pre->magic != CBUF_MAGIC) {
      reportError("Expected magic to be " + std::to_string(CBUF_MAGIC) + ", but it is " +
                  std::to_string(pre->magic));
      return false;
    }
    if (pre->hash != member->hash()) {
      reportError("Expected hash to be " + std::to_string(member->hash()) + ", but it is " +
                  std::to_string(pre->hash));
      return false;
    }
    if (pre->size() == 0) {
      reportError("Expected size to be non-zero");
      return false;
    }

    member->preamble.packet_timest = time_now();
    char* ringbuffer_mem = (char*)ringbuffer.handleToAddress(buffer_handle);

    if (member->supports_compact()) {
      if (!member->encode_net(ringbuffer_mem, stsize)) {
        ringbuffer.populate(buffer_handle);
        reportError("encode_net() failed for message " + std::string(member->TYPE_STRING));
        return false;
      }
    } else {
      if (!member->encode(ringbuffer_mem, stsize)) {
        ringbuffer.populate(buffer_handle);
        reportError("encode() failed for message " + std::string(member->TYPE_STRING));
        return false;
      }
    }

    // Check again
    pre = (cbuf_preamble*)ringbuffer_mem;
    if (pre->magic != CBUF_MAGIC) {
      reportError("Expected magic to be " + std::to_string(CBUF_MAGIC) + ", but it is " +
                  std::to_string(pre->magic));
      return false;
    }
    if (pre->hash != member->hash()) {
      reportError("Expected hash to be " + std::to_string(member->hash()) + ", but it is " +
                  std::to_string(pre->hash));
      return false;
    }
    if (pre->size() == 0) {
      reportError("Expected size to be non-zero");
      return false;
    }

    ringbuffer.populate(buffer_handle);
    return true;
  }

  template <class cbuf_struct>
  bool serialize(cbuf_struct& member, const uint64_t topic_name_hash = 0) {
    return serialize(&member, topic_name_hash);
  }
};
