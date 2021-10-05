#include "cbuf_reader.h"

#include <experimental/filesystem>

#include "hjson/hjson.h"

namespace fs = std::experimental::filesystem;

bool CBufReader::openUlog(bool error_ok) {
  for (const auto& f : fs::directory_iterator(ulog_path_)) {
    if (f.path().extension() == ".cb") {
      if (!role_filter_.empty()) {
        if (std::string::npos == f.path().string().find(role_filter_, 0)) {
          // skip cbufs that do not contain the role we want to filter on
          continue;
        }
      }
      vlog_fine(VCAT_GENERAL, "Found cbuf file: %s", f.path().c_str());
      // this is a cb file, open it
      StreamInfo* si = new StreamInfo;
      si->cis = new cbuf_istream();
      si->filename = f.path().string();
      std::string fname = fs::absolute(f).string();
      if (!si->cis->open_file(fname.c_str())) {
        if (!error_ok) vlog_error(VCAT_GENERAL, "Could not open file %s for reading", fname.c_str());
        return false;
      }
      input_streams.push_back(si);
    }
  }
  if (input_streams.size() == 0) {
    if (!error_ok)
      vlog_error(VCAT_GENERAL, "Could not find any 'cb' file on the ulog file %s", ulog_path_.c_str());
    return false;
  }
  return true;
}
