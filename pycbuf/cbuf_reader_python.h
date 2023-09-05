#pragma once

#include <Python.h>

#include <unordered_map>
#include <vector>

#include "CBufParserPy.h"
#include "cbuf_readerbase.h"  // in ulog/include

class CBufReaderPython : public CBufReaderBase {
  struct MessageInfo {
    CBufParserPy* parser = nullptr;
    PyObject* pytype = nullptr;
    bool parsing_failed = false;
  };

  std::unordered_map<std::string, MessageInfo> msg_map_;
  std::vector<std::string> msg_name_filter_;

  // This cis is used when parsing binary arrays
  cbuf_istream mem_cis;

  PyObject* getMessageInternal(PyObject* module, cbuf_istream* cis);

public:
  CBufReaderPython(const std::string& ulog_path, const Options& options = Options())
      : CBufReaderBase(ulog_path, options) {}
  CBufReaderPython(const Options& options = Options())
      : CBufReaderBase(options) {}

  void setMessageFilter(const std::vector<std::string>& msg_filter) { msg_name_filter_ = msg_filter; }
  const std::vector<std::string>& getMessageFilter() const { return msg_name_filter_; }

  //
  void set_start_time(double time) { startTime = time; }
  void set_end_time(double time) { endTime = time; }
  double get_start_time() { return startTime; }
  double get_end_time() { return endTime; }

  // Get the next message from the data stream (ulog or memory)
  PyObject* getMessage(PyObject* module);

  // Call this function to point the reader to a chunk of memory for future getMessage calls
  bool openMemory(const char* filename, const char* data, size_t size);
};
