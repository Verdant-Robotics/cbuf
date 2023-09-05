#pragma once

#include <unordered_map>

#include "cbuf_stream.h"

class CBufReaderBase {
public:
  struct Options {
    Options() {}
    bool try_recovery = false;  // whether to try to continue past corruptions.
  };

  int num_corruptions = 0;  // counter of corruptions found.

protected:
  static constexpr double VERY_LARGE_TIMESTAMP = 1e15;
  std::string ulog_path_;
  std::string error_string_;
  // Store what substring to discrimnate on input files when reading
  std::vector<std::string> source_filters_;
  Options options_;

  struct StreamInfo {
    cbuf_istream* cis = nullptr;
    double packet_time;
    std::string filename;
  };

  std::vector<StreamInfo*> input_streams;
  StreamInfo* next_si = nullptr;
  bool finish_reading = false;
  bool is_opened = false;
  double startTime = -1;
  double endTime = -1;

  // returns true if time t is within our range
  bool is_valid_early(double t) const noexcept;
  // returns true if time t is within our range
  bool is_valid_late(double t) const noexcept;
  bool computeNextSi();

public:
  CBufReaderBase(const std::string& ulog_path, const Options& options = Options())
      : ulog_path_(ulog_path)
      , options_(options) {}
  CBufReaderBase(const Options& options = Options())
      : options_(options) {}

  void setOptions(const Options& options) { options_ = options; }
  void setULogPath(const std::string& ulog_path) { ulog_path_ = ulog_path; }
  void setSourceFilters(const std::vector<std::string>& filters) { source_filters_ = filters; }
  const std::string& getULogPath() const { return ulog_path_; }
  const std::vector<std::string>& getSourceFilters() const { return source_filters_; }

  // Main function to open the ulog and build the data structures to being processing
  bool openUlog(bool error_ok = false);
  void close();
  bool isOpened() const { return is_opened; }
  // Errors are accumulated on error_string, get this string to provide the user with info
  const std::string& getErrorMessage() const { return error_string_; }

  void setStartTime(double t) { startTime = t; }
  void setEndTime(double t) { endTime = t; }

  double getNextTimestamp() {
    if (!computeNextSi()) return -1;

    return next_si->cis->get_next_timestamp();
  }

  // This function will reset the reading to the start of the log, count all messages and
  // leave the log in the beginning state
  std::unordered_map<std::string, unsigned int> getMessageCounts(std::string& error_string);

  // Get the total size on disk of all the files in the log
  size_t getTotalCbSize() const;
  // Get the filesize of the consumed messages (including skipped ones)
  size_t getConsumedCbSize() const;
};
