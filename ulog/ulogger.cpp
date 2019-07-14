#include "ulogger.h"
#include <mutex>
#include "vlog.h"
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <memory.h>
#include <experimental/filesystem> //#include <filesystem>
#include <fstream>
#include <streambuf>

// stat() check if directory exists
static  
bool directory_exists( const char* dirpath )
{
  struct stat status;
  if( stat( dirpath, &status ) == 0 && S_ISDIR( status.st_mode ) ) {  
    return true;
  }
  return false;
}

static
bool file_exists( const char* filepath )
{
  struct stat status;
  if( stat( filepath, &status ) == 0 && S_ISREG( status.st_mode ) ) {
    return true;
  }
  return false;
}

static std::mutex g_file_mutex;
static std::string outputdir;
static std::string sessiontoken;
static std::string filename;

static std::mutex g_ulogger_mutex;
static bool initialized = false;
static ULogger* g_ulogger = nullptr;

void ULogger::setOutputDir(const char* outdir)
{
  std::lock_guard<std::mutex> guard(g_file_mutex);
  if( directory_exists( outdir ) ) {
    outputdir = outdir;
  }
}

std::string ULogger::getSessionToken()
{
  // WIP: if and when the logdriving executable can stop recording and start the next one
  // without re-starting the executable,  then use the executable's process ID as the session token.
  // For now, just use the year_month_day  as the session.
  if( sessiontoken.empty() ) {
    time_t rawtime;
    struct tm *info;
    time( &rawtime );
    info = localtime( &rawtime );

    const char *robot_name = "unknown";
    std::stringstream sbuffer;

    if (file_exists( "/etc/robot_name" )) {
      std::ifstream t("/etc/robot_name");
      sbuffer << t.rdbuf();
      robot_name = sbuffer.str().c_str();
    }

    char buffer[PATH_MAX];
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%s-%d.%02d.%02d.%02d.%02d.ulog", robot_name,
            info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min );
    sessiontoken = buffer;
  }
  return sessiontoken;
}

std::string ULogger::getSessionPath()
{
  std::string result;
  std::string sessiontok = getSessionToken();
  if( !outputdir.empty() ) {
    result = outputdir + "/" + sessiontok;
  }
  else {
    result = sessiontok;
  }
  if( !std::experimental::filesystem::exists( result.c_str() ) ) {
    if( !std::experimental::filesystem::create_directories( result.c_str() ) ) {
      vlog_fatal(VCAT_GENERAL, "Error: output directory does not exist and could not create it %s \n", result.c_str() );
    }
  }

  return result;    
}

void ULogger::fillFilename()
{
  time_t rawtime;
  struct tm* info;
  time(&rawtime);
  info = localtime(&rawtime);

  char buffer[PATH_MAX];
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "vdnt.%d.%02d.%02d.%02d.%02d.%02d.ulog",
          info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);

  filename = getSessionPath() + "/" + buffer;
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

// return a copy of the filename, not a reference 
std::string ULogger::getFilename()
{
  std::lock_guard<std::mutex> guard(g_file_mutex);
  std::string result = filename;
  return result;   
}

bool ULogger::openFile()
{ 
  std::lock_guard<std::mutex> guard(g_file_mutex);
  fillFilename();
 
  vlog_fine(VCAT_GENERAL, "Ulogger openFile %s\n", filename.c_str() );
 
  // Open the serialization file
  bool bret = cos.open_file(filename.c_str());
  if (!bret) {
    vlog_fatal(VCAT_GENERAL, "Could not open the ulog file for logging %s\n", filename.c_str() );
    return false;  
  }
  return true;
}

void ULogger::closeFile()
{
  std::lock_guard<std::mutex> guard(g_file_mutex);
  cos.close();    
}

void ULogger::endLoggingThread()
{
  quit_thread = true;
  //uint64_t buffer_handle = ringbuffer.alloc(0, nullptr, nullptr);
  //ringbuffer.populate(buffer_handle);
  loggerThread->join();
  delete loggerThread;
  loggerThread = nullptr;
}

bool ULogger::initialize() {
  loggerThread = new std::thread([this]() {
    while (!this->quit_thread) {
      if (ringbuffer.size() == 0)
      {
        usleep(1000);
        continue;
      }
      std::optional<RingBuffer<1024 * 1024 * 10>::Buffer> r =
          ringbuffer.lastUnread();

      if (!r) {
        usleep(1000);
        continue;
      }

      if( !cos.is_open() ) {
        if( !openFile() ) {
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

    closeFile();
  });
  return true;
}

bool ULogger::isInitialized()
{
    return initialized;
}

// No public constructors, this is a singleton
ULogger* ULogger::getULogger()
{
  if (!initialized) { 
    std::lock_guard<std::mutex> guard(g_ulogger_mutex);
    if (!initialized) {    
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
  }
  return g_ulogger;
}

/// function to stop all logging, threads, and terminate the app
void ULogger::endLogging()
{
  if( g_ulogger == nullptr ) {
    return;
  }
  g_ulogger->endLoggingThread();
  delete g_ulogger;
  g_ulogger = nullptr;
  initialized = false;
}
