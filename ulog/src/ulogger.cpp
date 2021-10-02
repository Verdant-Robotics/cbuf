#include "ulogger.h"

#include <linux/limits.h>
#include <memory.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <experimental/filesystem>  //#include <filesystem>
#include <fstream>
#include <mutex>
#include <streambuf>

#include "vlog.h"

static std::recursive_mutex g_file_mutex;
static std::mutex g_ulogger_mutex;
static bool initialized = false;
static ULogger* g_ulogger = nullptr;

namespace std {
namespace filesystem = experimental::filesystem;
}

void ULogger::setLogPath(const std::string& path) {
  std::lock_guard guard(g_file_mutex);
  if (outputdir != path) {
    std::error_code ec;
    outputdir = path;

    // create output path
    if (!std::filesystem::exists(outputdir.c_str())) {
      vlog_info(VCAT_GENERAL, "ULogger creating directory %s", outputdir.c_str());
      if (!std::filesystem::create_directories(outputdir.c_str(), ec)) {
        vlog_error(VCAT_GENERAL,
                   "Error: output directory does not exist and could not create it: %s Error: %s \n",
                   outputdir.c_str(), ec.message().c_str());
        outputdir = ".";
      }
    }

    char hostname[128] = {};
    gethostname(hostname, sizeof(hostname));
    if (getenv("VLOG_TEE_LOG")) {
      sprintf((char*)vlog_option_tee_file, "%s/%s_console_output.txt", outputdir.c_str(), hostname);
    }

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
  pthread_setname_np(pthread_self(), thread_name);
}

double ULogger::time_now() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  double now = double(ts.tv_sec) + double(ts.tv_nsec) / 1e9;
  return now;
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
    vlog_always("Output folder is not set, logging to the current directory");
    outputdir = ".";
  }

  // Issue a warning here if outputdir is empty
  ulogfilename = getLogPath() + "/" + buffer;
  unsigned int suffix = 1;
  while (std::experimental::filesystem::exists(ulogfilename)) {
    sprintf(buffer, "%s.%s.%d.%02d.%02d.%02d_%02d_%02d_%d.cb", __progname, hostname, info->tm_year + 1900,
            info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec, suffix);
    suffix++;
    ulogfilename = getLogPath() + "/" + buffer;
  }
}

// Process packet here:
// Check the dictionary if needed for metadata
// Write the packet otherwise
void ULogger::processPacket(void* data, int size, const char* metadata, const char* type_name) {
  cbuf_preamble* pre = (cbuf_preamble*)data;
  if (!cos.is_open()) {
    bool r = openFile();
    VLOG_ASSERT(r, "Could not open the next file!!!");
  }

  // Basic integrity checks
  VLOG_ASSERT(pre->magic == CBUF_MAGIC,
              "Expected magic to be %X, but it is %X for packet of size %d, type %s, metadata: [[ %s ]] ",
              CBUF_MAGIC, pre->magic, size, type_name, metadata);
  VLOG_ASSERT(pre->hash != 0);
  VLOG_ASSERT(pre->size() != 0);
  VLOG_ASSERT(pre->size() == size, "Ulogger, writing %d bytes but the cbuf reports it has %d bytes", size,
              pre->size());

  file_check_count++;
  if (file_check_count > 1000) {
    // Once in a while, check disk space
    std::filesystem::space_info disk = std::filesystem::space(ulogfilename);
    VLOG_ASSERT(disk.free > 1024 * 1024 * 1024, "We are running low on disk, we have %zu free bytes",
                disk.free);
    file_check_count = 0;
  }

  if (cos.dictionary.count(pre->hash) == 0) {
    cos.serialize_metadata(metadata, pre->hash, type_name);
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
        vlog_warning(VCAT_GENERAL, "Cbuf writing error: %d", errno);
      }
      error_count++;
      if (error_count > 10) {
        assert(false);
      }
    }
  } while (bytes_to_write > 0);
  current_file_size += size;

  // Paranoia
  VLOG_ASSERT(pre->magic == CBUF_MAGIC, "Expected magic to be %X, but it is %X", CBUF_MAGIC, pre->magic);
  VLOG_ASSERT(pre->hash != 0);
  VLOG_ASSERT(pre->size() != 0);

  if (current_file_size > SPLIT_FILE_SIZE) {
    closeFile();
    bool r = openFile();
    VLOG_ASSERT(r, "Could not open the next file!!!");
  }
}

// return a copy of the filename, not a reference
std::string ULogger::getCurrentUlogPath() {
  std::lock_guard guard(g_file_mutex);
  std::string result = ulogfilename;
  return result;
}

bool ULogger::openFile() {
  std::lock_guard guard(g_file_mutex);
  std::error_code ec;

  // create output path
  if (!std::filesystem::exists(outputdir.c_str())) {
    vlog_info(VCAT_GENERAL, "ULogger creating directory %s", outputdir.c_str());
    if (!std::filesystem::create_directories(outputdir.c_str(), ec)) {
      vlog_error(VCAT_GENERAL,
                 "Error: output directory does not exist and could not create it: %s Error: %s \n",
                 outputdir.c_str(), ec.message().c_str());
      outputdir = ".";
    }
  }

  fillUlogFilename();

  vlog_info(VCAT_GENERAL, "Ulogger openFile %s", ulogfilename.c_str());

  // Open the serialization file
  bool bret = cos.open_file(ulogfilename.c_str());
  if (!bret) {
    vlog_fatal(VCAT_GENERAL, "Could not open the ulog file for logging %s\n", ulogfilename.c_str());
    return false;
  }
  current_file_size = 0;
  return true;
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

bool ULogger::initialize() {
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
        processPacket(r->loc, r->size, r->metadata, r->type_name);
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
        processPacket(r->loc, r->size, r->metadata, r->type_name);
      }
      ringbuffer.dequeue();
    }

    closeFile();
  });
  return true;
}

// No public constructors, this is a singleton
ULogger* ULogger::getULogger() {
  if (!initialized) {
    std::lock_guard guard(g_ulogger_mutex);
    if (initialized) return g_ulogger;

    g_ulogger = new ULogger();
    g_ulogger->quit_thread = false;

    bool bret = g_ulogger->initialize();
    if (!bret) {
      vlog_fatal(VCAT_GENERAL, "Could not initialize ulogger singleton");
      delete g_ulogger;
      return nullptr;
    }

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
