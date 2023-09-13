#include "cbuf_readerbase.h"

#include <filesystem>

#include "ulogger.h"

namespace fs = std::filesystem;

// returns true if time t is within our range
bool CBufReaderBase::is_valid_early(double t) const noexcept {
  if (startTime > 0) {
    if (t < startTime) {
      return false;
    }
  }
  return true;
}

// returns true if time t is within our range
bool CBufReaderBase::is_valid_late(double t) const noexcept {
  if (endTime > 0) {
    if (t > endTime) {
      return false;
    }
  }
  return true;
}

bool CBufReaderBase::computeNextSi() {
  // nothing else to do
  if (finish_reading) return false;

  if (input_streams.empty()) return false;

  // Compute the correct packet time and ensure we skip corruption
  for (auto si : input_streams) {
    if (si->cis->empty_no_internal()) {
      si->packet_time = VERY_LARGE_TIMESTAMP;
      continue;
    }
    bool corrupted = !si->cis->check_next_preamble() || (si->cis->get_next_size() == 0);
    if (!options_.try_recovery && corrupted) {
      fprintf(stderr, "** Failed to process corrupted cbuf file %s; halting\n", si->filename.c_str());
      num_corruptions++;
      return false;
    }

    while (corrupted && !si->cis->empty()) {
      num_corruptions++;
      auto msize = si->cis->get_next_size();
      auto nhash = si->cis->get_next_hash();
      fprintf(stderr,
              " ** Reading a cbuf message on %s with invalid preamble (size: %u, hash: %" PRIX64
              ") [FileSize %zu, Offset %zu], this indicates a corrupted ulog. Trying to recover...\n",
              si->filename.c_str(), msize, nhash, si->cis->get_filesize(), si->cis->get_current_offset());
      auto off = si->cis->get_current_offset();
      if (si->cis->skip_corrupted()) {
        fprintf(stderr, "  => Recovered from corruption, skipped %zu bytes\n",
                si->cis->get_current_offset() - off);
      }
      if (si->cis->empty()) {
        corrupted = false;
        break;
      }
      corrupted = !si->cis->check_next_preamble() || (si->cis->get_next_size() == 0);
    }
    if (corrupted || si->cis->empty()) {
      si->packet_time = VERY_LARGE_TIMESTAMP;
    } else {
      // One more case that can happen here, we have one last packet that is not complete
      // The answer is to skip it
      if (si->cis->get_next_size() > si->cis->get_remaining_size()) {
        si->cis->skip_message();
        si->packet_time = VERY_LARGE_TIMESTAMP;
      } else {
        // This is the good case, we found the right timestamp of a valid packet
        si->packet_time = si->cis->get_next_timestamp();
      }
    }
  }

  // Find earliest packet
  next_si = input_streams[0];
  for (auto si : input_streams) {
    if (si->packet_time < next_si->packet_time) {
      next_si = si;
    }
  }

  if (next_si->cis->empty() || !is_valid_late(next_si->packet_time)) {
    finish_reading = true;
    // nothing more to do here
    return false;
  }

  return true;
}

bool CBufReaderBase::openUlog(bool error_ok) {
  if (!fs::exists(ulog_path_)) {
    error_string_ = "Could not find ulog path " + ulog_path_;
    return false;
  }
  for (const auto& f : fs::directory_iterator(ulog_path_)) {
    if (f.path().extension().string() == ".cb") {
      if (!source_filters_.empty()) {
        bool filter_match = false;
        std::string filename = f.path().filename();
        for (auto& str : source_filters_) {
          // Find the source_filter in filename only.
          if (std::string::npos != filename.find(str, 0)) {
            filter_match = true;
            break;
          }
        }
        if (!filter_match) {
          // skip cbufs that do not contain the source name we want to filter on
          continue;
        }
      }
      // this is a cb file, open it
      StreamInfo* si = new StreamInfo;
      si->cis = new cbuf_istream();
      si->filename = f.path().string();
      std::string fname = fs::absolute(f).string();
      if (si->cis->open_file(fname.c_str())) {
        si->cis->disable_consume_on_deserialize();
        input_streams.push_back(si);
      } else {
        error_string_ = "Could not open file " + fname + " for reading.";
        delete si;
        if (!options_.try_recovery) {
          error_string_ +=
              "\nFailed to open ulog due to reading errors. Pass option try_recovery to continue";
          return false;
        }
      }
    }
  }
  if (input_streams.size() == 0) {
    if (!error_ok) {
      error_string_ = "Could not find any 'cb' file on the ulog file " + ulog_path_;
      return false;
    }
  }
  is_opened = true;
  return true;
}

size_t CBufReaderBase::getTotalCbSize() const {
  size_t total = 0;
  for (auto si : input_streams) {
    total += si->cis->get_filesize();
  }
  return total;
}

size_t CBufReaderBase::getConsumedCbSize() const {
  size_t consumed = 0;
  for (auto si : input_streams) {
    consumed += si->cis->get_current_offset();
  }
  return consumed;
}

void CBufReaderBase::close() {
  for (auto& si : input_streams) {
    if (si != nullptr) {
      if (si->cis != nullptr) {
        delete si->cis;
        si->cis = nullptr;
      }
      delete si;
      si = nullptr;
    }
  }
  input_streams.clear();
  is_opened = false;
}

std::unordered_map<std::string, unsigned int> CBufReaderBase::getMessageCounts(std::string& error_string) {
  std::unordered_map<std::string, unsigned int> msg_counts;
  for (auto si : input_streams) {
    si->cis->reset_ptr();
  }

  for (auto si : input_streams) {
    while (!si->cis->empty()) {
      auto msize = si->cis->get_next_size();
      auto nhash = si->cis->get_next_hash();
      if (msize == 0) {
        si->cis->skip_corrupted();
        continue;
      }
      if (nhash == 0) {
        si->cis->skip_message();
        continue;
      }
      auto msg_name = si->cis->get_string_for_hash(nhash);
      if (msg_counts.find(msg_name) == msg_counts.end()) {
        msg_counts[msg_name] = 1;
      } else {
        msg_counts[msg_name]++;
      }
      si->cis->skip_message();
    }
  }
  // Leave the streams at the start
  for (auto si : input_streams) {
    si->cis->reset_ptr();
  }
  return msg_counts;
}
