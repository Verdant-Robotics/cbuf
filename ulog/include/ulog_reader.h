#pragma once
#include "cbuf_reader.h"

// Read ulogs. Extract message data.

//--------------------------
// Example Usage 1:
//      UlogReader reader;
//
//      reader.addHandlerFn<messages::pose_msg>(
//        [](messages::pose_msg* pose){
//          printf("Lambda got pose \n");
//        }, false, true);
//
//      reader.addHandlerFn<messages::vehicle_pose>(
//        [](messages::vehicle_pose* vehiclepose){
//          if( vehiclepose->sys_time <= 0.0 ) {  // not valid
//            return;
//          }
//          printf("Lambda got vehicle_pose at %f\n", vehiclepose->sys_time);
//        }, false, true);
//
//      reader.ProcessUlog("/absolute/path/to.ulog");

//-------------------------
// Example Usage 2:
//      // Capture with ampersand.
//      UlogReader reader;
//      reader.addHandlerFn<messages::pose_msg>(
//        [&](messages::pose_msg* pose){      //   <-- Note: ampersand
//          this->do_fancy_stuff_with_pose( pose );
//        }, false, true);
//
//      reader.ProcessUlog("/absolute/path/to.ulog");

class UlogReader : public CBufReader {
public:
  UlogReader() {}
  ~UlogReader() {}

  // Add function here and reduce duplicate code elsewhere.

  void processAllMessages() {
    while (processMessage())
      ;
  }

  bool ProcessUlog(const std::string& ulogpath) {
    ulog_path = ulogpath;
    setULogPath(ulogpath);
    if (!openUlog()) {
      return false;
    }
    ulog_time = getNextTimestamp();
    processAllMessages();
    return true;
  }

public:
  std::string ulog_path;
  double ulog_time = -1;
};

class WindowUlogReader : public CBufReaderWindow {
public:
  WindowUlogReader() {}
  ~WindowUlogReader() {}

public:
  std::string ulog_path;
  double ulog_time = -1;
};
