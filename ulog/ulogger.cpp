#include "ulogger.h"
#include <mutex>
#include "vlog.h"
#include <unistd.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <memory.h>
#include <experimental/filesystem> //#include <filesystem>


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

    char buffer[PATH_MAX];
    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d%d%d",
            info->tm_year + 1900, info->tm_mon, info->tm_mday );
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
      printf( "Error: output directory does not exist and could not create it %s \n", result.c_str() );
    }
  }

  return result;    
}


void ULogger::fillFilename()
{
  time_t rawtime;
  struct tm *info;
  time( &rawtime );
  info = localtime( &rawtime );

  char buffer[PATH_MAX];
  memset(buffer, 0, sizeof(buffer));
  sprintf(buffer, "vdnt.%d.%d.%d.%d.%d.%d.ulog",
          info->tm_year + 1900, info->tm_mon, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);

  filename = getSessionPath() + "/" + buffer;
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
 
  printf( "openFile %s\n", filename.c_str() );
 
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



/// Return the allocated memory to our pool
void ULogger::freeBuffer(void *buffer)
{
  // For now, just a free
  free(buffer);
}

/// Functions to get memory and queue packets for logging
void *ULogger::getBuffer(unsigned int size)
{
  // change me:
  return malloc(size);
}



// Process packet here:
// Check the dictionary if needed for metadata
// Write the packet otherwise
void ULogger::processPacket(Packet& pkt)
{
  std::lock_guard<std::mutex> guard(g_file_mutex);
  cbuf_preamble *pre = (cbuf_preamble *)pkt.data;
  if (cos.dictionary.count(pre->hash) == 0) {
      cos.serialize_metadata(pkt.metadata, pre->hash, pkt.type_name);
  }

  write(cos.stream, pkt.data, pkt.size);

  freeBuffer(pkt.data);
}

bool ULogger::initialize()
{
  loggerThread = new std::thread(
        [this]()
  {
    printf( "ULogger::initialize  THREAD ENTER\n" );

    while( !this->quit_thread ) {
      Packet pkt;

      bool empty = false;
      {
        std::lock_guard<std::mutex> guard(mutexQueue);
        empty = packetQueue.empty();        
      }

      if (empty) {
        usleep(1000);
        continue;
      }
      else
      {
        std::lock_guard<std::mutex> guard(mutexQueue);
        pkt = packetQueue.front();
        packetQueue.pop();
      }

      if( !cos.is_open() ) {
        printf( "ULogger::initialize  OPEN THE FILE\n" );
        if( !openFile() ) {
          return;
        }
      }

      processPacket(pkt);
    }

    printf( "ULogger::initialize  flush the queue\n" );
    // Continue processing the queue until it is empty
    {
      std::lock_guard<std::mutex> guard(mutexQueue);
      while(!packetQueue.empty()) {
        Packet pkt = packetQueue.front();
        packetQueue.pop();
        processPacket(pkt);
      }
    }

    printf( "ULogger::initialize  close the file\n" );
    closeFile();
    printf( "ULogger::initialize  THREAD EXIT\n" );
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
  g_ulogger->quit_thread = true;
  g_ulogger->loggerThread->join();
  delete g_ulogger->loggerThread;
  g_ulogger->loggerThread = nullptr;
  delete g_ulogger;
  g_ulogger = nullptr;
  initialized = false;
}

void ULogger::queuePacket(void *data, unsigned int size, const char *metadata, const char *type_name)
{
  std::lock_guard<std::mutex> guard(mutexQueue);
  packetQueue.emplace( metadata, type_name, data, size );
}
