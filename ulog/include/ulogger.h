#pragma once

#include <queue>
#include <mutex>
#include <thread>

#include "cbuf_preamble.h"
#include "cbuf_stream.h"

class ULogger {
  ULogger();
  ~ULogger();

  struct PacketQueue
  {
    const char *metadata = nullptr;
    const char *type_name = nullptr;
    void *data = nullptr;
    int size = 0;

    PacketQueue(const char *md, const char *tn, void *dt, int sz) :
        metadata(md), type_name(tn), data(dt), size(sz) {}
    PacketQueue() {}
  };

  std::queue<PacketQueue> packetQueue;
  std::mutex mutexQueue;
  std::thread* loggerThread = nullptr;

  bool initialize();
  cbuf_ostream cos;
  bool logging = true;

  void processPacket(PacketQueue& pq);
  void freeBuffer(void *buffer);

  char filename_buffer[128];
  void fillFilename();

public:
  // No public constructors, this is a singleton
  static ULogger* getULogger();

  /// function to stop all logging, threads, and terminate the app
  void endLogging();

  /// Functions to get memory and queue packets for logging
  void *getBuffer(unsigned int size);
  void queuePacket(void *data, unsigned int size, const char *metadata, const char *type_name);

  /// This function will serialize to a buffer and then queue for the thread to write to disk
  template <class cbuf_struct>
  bool serialize(cbuf_struct * member)
  {
    if (!logging) return false;

    auto stsize = (unsigned int)member->encode_size();
    char *buffer = (char *)getBuffer(stsize);
    if (buffer == nullptr) {
        return false;
    }

    if (!member->encode(buffer, stsize)) {
        return false;
    }

    queuePacket(buffer, stsize, member->cbuf_string, member->TYPE_STRING);
    return true;
  }

  template <class cbuf_struct>
  bool serialize(cbuf_struct& member)
  {
    return serialize(&member);
  }
};
