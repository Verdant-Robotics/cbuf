#include "ulogger.h"
#include <mutex>
#include "vlog.h"

ULogger::ULogger() {}

ULogger::~ULogger() {}

void ULogger::fillFilename() {
  time_t rawtime;
  struct tm* info;
  time(&rawtime);
  info = localtime(&rawtime);

  sprintf(filename_buffer, "vdnt.%d.%d.%d.%d.%d.ulog", info->tm_year,
          info->tm_mon, info->tm_mday, info->tm_hour, info->tm_min);
}

// Process packet here:
// Check the dictionary if needed for metadata
// Write the packet otherwise
void ULogger::processPacket(void* data,
                            int size,
                            const char* metadata,
                            const char* type_name) {
  cbuf_preamble* pre = (cbuf_preamble*)data;
  if (cos.dictionary.count(pre->hash) == 0) {
    cos.serialize_metadata(metadata, pre->hash, type_name);
  }

  write(cos.stream, data, size);
}

bool ULogger::initialize() {
  loggerThread = new std::thread([this]() {
    while (logging) {
      std::optional<RingBuffer<1024 * 1024 * 10>::Buffer> r =
          ringbuffer.lastUnread();

      if (!r) {
        usleep(1000);
        continue;
      }

      if (r->size > 0) {
        processPacket(r->loc, r->size, r->metadata, r->type_name);
      }
      ringbuffer.dequeue();
    }

    // Continue processing the queue until it is empty
    while (ringbuffer.size() > 0) {
      std::optional<RingBuffer<1024 * 1024 * 10>::Buffer> r =
          ringbuffer.lastUnread();

      if (!r) {
        usleep(1000);
        continue;
      }

      if (r->size > 0) {
        processPacket(r->loc, r->size, r->metadata, r->type_name);
      }
      ringbuffer.dequeue();
    }
    cos.close();
  });
  return true;
}

static std::mutex g_ulogger_mutex;
static bool initialized = false;
static ULogger* g_ulogger = nullptr;

// No public constructors, this is a singleton
ULogger* ULogger::getULogger() {
  if (!initialized) {
    std::lock_guard<std::mutex> guard(g_ulogger_mutex);

    g_ulogger = new ULogger();

    g_ulogger->fillFilename();

    // Open the serialization file
    bool bret = g_ulogger->cos.open_file(g_ulogger->filename_buffer);
    if (!bret) {
      vlog_fatal(VCAT_GENERAL, "Could not open the ulog file for logging");
      delete g_ulogger;
      return nullptr;
    }

    bret = g_ulogger->initialize();
    if (!bret) {
      vlog_fatal(VCAT_GENERAL, "Could not initialize ulogger singleton");
      delete g_ulogger;
      return nullptr;
    }

    // Do any extra initialization needed here
    initialized = true;
  }
  return g_ulogger;
}

/// function to stop all logging, threads, and terminate the app
void ULogger::endLogging() {
  logging = false;
  loggerThread->join();
  delete loggerThread;
  loggerThread = nullptr;
}
