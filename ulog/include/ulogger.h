#pragma once

#include <queue>
#include <mutex>
#include <thread>
#include <string>
#include <atomic>
#include "cbuf_preamble.h"
#include "cbuf_stream.h"



class ULogger {
  ULogger() : quit_thread(false) { }
  ~ULogger() = default;

  struct Packet
  {
    const char *metadata = nullptr;
    const char *type_name = nullptr;
    void *data = nullptr;
    int size = 0;

    Packet(const char *md, const char *tn, void *dt, int sz) :
        metadata(md), type_name(tn), data(dt), size(sz) {}
    Packet() {}
  };

  std::queue<Packet> packetQueue;
  std::mutex mutexQueue;
  std::thread* loggerThread = nullptr;

  bool initialize();
  cbuf_ostream cos;
  bool quit_thread;

  void fillFilename();
  bool openFile();
  void closeFile();

  void processPacket(Packet& pq);

  void *getBuffer(unsigned int size);
  void freeBuffer(void *buffer);
  

public:
  static bool isInitialized();

  void setOutputDir(const char* outputdir);
  std::string getSessionToken();
  std::string getSessionPath();
  std::string getFilename();

  // No public constructors, this is a singleton
  static ULogger* getULogger();

  /// function to stop all logging, threads, and terminate the app
  static void endLogging();

  /// Function to queue packets for file writing
  void queuePacket(void *data, unsigned int size, const char *metadata, const char *type_name);


  /// This function will serialize to a buffer and then queue for the thread to write to disk
  template <class cbuf_struct>
  bool serialize(cbuf_struct * member)
  {
    if (quit_thread) 
      return false;

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
