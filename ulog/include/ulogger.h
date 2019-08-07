#pragma once

#include <mutex>
#include <queue>
#include <thread>
#include "ringbuffer.h"
#include <string>
#include <atomic>
#include "cbuf_preamble.h"
#include "cbuf_stream.h"

class ULogger {
  ULogger() : quit_thread(false) { }
  ~ULogger() = default;

  RingBuffer<1024 * 1024 * 10> ringbuffer;
  std::thread* loggerThread = nullptr;

  std::string outputdir;
  std::string sessiontoken;
  std::string filename;

  bool initialize();
  cbuf_ostream cos;
  bool quit_thread;

  void processPacket(void* data,
                     int size,
                     const char* metadata,
                     const char* type_name);

  void fillFilename();
  bool openFile();
  void closeFile();
  void endLoggingThread();

public:
  void setOutputDir(const char* outputdir);
  std::string getSessionToken();
  std::string getFilename();
  std::string getSessionPath();
  void setSessionToken(const std::string& token);

  // No public constructors, this is a singleton
  static ULogger* getULogger();

  /// function to stop all logging, threads, and terminate the app
  static void endLogging();

  /// This function will serialize to a buffer and then queue for the thread to
  /// write to disk
  template <class cbuf_struct>
  bool serialize(cbuf_struct * member)
  {
    if (quit_thread) 
      return false;

    auto stsize = (unsigned int)member->encode_size();
    uint64_t buffer_handle =
        ringbuffer.alloc(stsize, member->cbuf_string, member->TYPE_STRING);
    if (member->preamble.magic != CBUF_MAGIC) {
      // Some packets skip default initialization, ensure it here
      member->preamble.magic = CBUF_MAGIC;
      member->preamble.size = stsize;
      member->preamble.hash = member->hash();
    }

    if (!member->encode((char*)ringbuffer.handleToAddress(buffer_handle), stsize)) {
      ringbuffer.populate(buffer_handle);
      return false;
    }
    ringbuffer.populate(buffer_handle);
    return true;
  }

  template <class cbuf_struct>
  bool serialize(cbuf_struct& member) {
    return serialize(&member);
  }
};
