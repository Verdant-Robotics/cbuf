#pragma once

#include <functional>
#include <unordered_map>

#include "CBufParser.h"
#include "cbuf_stream.h"
#include "vlog.h"

template <typename T>
void loadFromJson(const Hjson::Value& json, T& obj);

class CBufHandlerBase {
  std::string msg_name_;
  // set this to true to process this message even when out of start,end time
  bool process_always_ = false;

public:
  CBufHandlerBase(const std::string& msg_name, bool process)
      : msg_name_(msg_name)
      , process_always_(process) {}
  const std::string& name() const { return msg_name_; }
  bool process_always() const { return process_always_; }
  virtual ~CBufHandlerBase() {}
  // returns true if the message was consumed, 0 otherwise
  virtual bool processMessage(cbuf_istream& cis) = 0;
};

// Handler callback function pointer.
template <typename TApp, typename CBufMsg>
using CBufMessageHandler = void (TApp::*)(CBufMsg* msg);

template <typename TApp, typename CBufMsg>
class CBufHandler : public CBufHandlerBase {
  TApp* caller = nullptr;
  CBufMessageHandler<TApp, CBufMsg> handler = nullptr;
  CBufMsg* msg = nullptr;
  CBufParser* parser = nullptr;
  bool allow_json_conversion = true;
  bool warned_json = false;

public:
  CBufHandler(TApp* owner, CBufMessageHandler<TApp, CBufMsg> h, bool allow_json = true,
              bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , caller(owner)
      , handler(h)
      , allow_json_conversion(allow_json) {}

  ~CBufHandler() override {
    if (msg) {
      delete msg;
      msg = nullptr;
    }
  }

  bool processMessage(cbuf_istream& cis) override {
    if (cis.empty()) return true;
    if (msg == nullptr) {
      msg = new CBufMsg();
    }
    auto hash = cis.get_next_hash();

    if (hash == CBufMsg::TYPE_HASH) {
      bool ret = cis.deserialize(msg);
      if (!ret) {
        vlog_error(VCAT_GENERAL, "Could not deserialize message of type %s", CBufMsg::TYPE_STRING);
        return false;
      }
      (caller->*handler)(msg);
      return true;
    }
    auto msg_type = cis.get_string_for_hash(hash);
    if (msg_type == CBufMsg::TYPE_STRING) {
      if (allow_json_conversion) {
        // the hash did not match but has the same name, try to do json conversion
        if (parser == nullptr) {
          parser = new CBufParser();
          if (!parser->ParseMetadata(cis.get_meta_string_for_hash(hash), CBufMsg::TYPE_STRING)) {
            vlog_error(VCAT_GENERAL, "metadata could not be parsed for message %s", CBufMsg::TYPE_STRING);
            return false;
          }
        }

        // Initialize the fields on the cbuf in order to ensure when json backwards compat
        // does not fill in fields (that are not there) still have sane values
        msg->Init();
        Hjson::Value msg_data;
        parser->FillHjson(CBufMsg::TYPE_STRING, cis.get_current_ptr(), cis.get_next_size(), msg_data, false);
        loadFromJson(msg_data, *msg);
        msg->preamble.packet_timest = cis.get_next_timestamp();

        (caller->*handler)(msg);
        // loadFromJson does not consume the message in cis, we need to advance it
        cis.skip_message();
        return true;
      }
      if (!warned_json) {
        vlog_error(VCAT_GENERAL, "cbuf version mismatch for message %s but JSON conversion is not allowed",
                   CBufMsg::TYPE_STRING);
        warned_json = true;
      }
    }
    return false;
  }
};

template <typename CBufMsg>
using CBufHandlerLambdaFn = std::function<void(CBufMsg*)>;

template <typename CBufMsg>
class CBufHandlerLambda : public CBufHandlerBase {
  CBufHandlerLambdaFn<CBufMsg> handler = nullptr;
  CBufMsg* msg = nullptr;
  CBufParser* parser = nullptr;
  bool allow_json_conversion = true;
  bool warned_json = false;

public:
  CBufHandlerLambda(void (*h)(CBufMsg*), bool allow_json = true, bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , handler(h)
      , allow_json_conversion(allow_json) {}

  CBufHandlerLambda(std::function<void(CBufMsg*)> h, bool allow_json = true, bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , handler(h)
      , allow_json_conversion(allow_json) {}

  ~CBufHandlerLambda() override {
    if (msg) {
      delete msg;
      msg = nullptr;
    }
  }

  bool processMessage(cbuf_istream& cis) override {
    if (cis.empty()) return true;
    if (msg == nullptr) {
      msg = new CBufMsg();
    }
    auto hash = cis.get_next_hash();
    if (hash == CBufMsg::TYPE_HASH) {
      bool ret = cis.deserialize(msg);
      if (!ret) {
        vlog_error(VCAT_GENERAL, "Could not deserialize message of type %s", CBufMsg::TYPE_STRING);
        return false;
      }
      handler(msg);
      return true;
    }
    auto msg_type = cis.get_string_for_hash(hash);
    if (msg_type == CBufMsg::TYPE_STRING) {
      if (allow_json_conversion) {
        // the hash did not match but has the same name, try to do json conversion
        if (parser == nullptr) {
          parser = new CBufParser();
          if (!parser->ParseMetadata(cis.get_meta_string_for_hash(hash), CBufMsg::TYPE_STRING)) {
            vlog_error(VCAT_GENERAL, "metadata could not be parsed for message %s", CBufMsg::TYPE_STRING);
            return false;
          }
        }

        // Initialize the fields on the cbuf in order to ensure when json backwards compat
        // does not fill in fields (that are not there) still have sane values
        msg->Init();
        Hjson::Value msg_data;
        parser->FillHjson(CBufMsg::TYPE_STRING, cis.get_current_ptr(), cis.get_next_size(), msg_data, false);
        loadFromJson(msg_data, *msg);

        handler(msg);
        // loadFromJson does not consume the message in cis, we need to advance it
        cis.skip_message();
        return true;
      }
      if (!warned_json) {
        vlog_error(VCAT_GENERAL, "Version mismatch for message %s but JSON conversion is not allowed",
                   CBufMsg::TYPE_STRING);
        warned_json = true;
      }
    }

    return false;
  }
};

class CBufReader {
public:
  struct Options {
    Options() {}
    bool try_recovery = false;  // whether to try to continue past corruptions.
  };

  int num_corruptions = 0;  // counter of corruptions found.

private:
  static constexpr double VERY_LARGE_TIMESTAMP = 1e15;
  std::string ulog_path_;
  std::string role_filter_;
  std::unordered_map<std::string, std::shared_ptr<CBufHandlerBase>> msg_map;
  Options options_;

  struct StreamInfo {
    cbuf_istream* cis;
    double packet_time;
    std::string filename;
  };

  std::vector<StreamInfo*> input_streams;
  StreamInfo* next_si = nullptr;
  bool finish_reading = false;
  double startTime = -1;
  double endTime = -1;

  // returns true if time t is within our range
  bool is_valid_early(double t) {
    if (startTime > 0) {
      if (t < startTime) {
        return false;
      }
    }
    return true;
  }

  // returns true if time t is within our range
  bool is_valid_late(double t) {
    if (endTime > 0) {
      if (t > endTime) {
        return false;
      }
    }
    return true;
  }

  bool computeNextSi() {
    // nothing else to do
    if (finish_reading) return false;

    // Compute the correct packet time and ensure we skip corruption
    for (auto si : input_streams) {
      if (si->cis->empty()) {
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
                " ** Reading a cbuf message on %s with invalid preamble (size: %u, hash: %zX) [FileSize %zu, "
                "Offset "
                "%zu], this indicates a corrupted ulog. Trying to recover...\n",
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
        si->packet_time = si->cis->get_next_timestamp();
      }
    }

    // Find earliest packet
    StreamInfo* prev_si = next_si;
    next_si = input_streams[0];
    for (auto si : input_streams) {
      if (si->packet_time < next_si->packet_time) {
        next_si = si;
      }
    }

    if (next_si != prev_si) {
      vlog_fine(VCAT_GENERAL, "[cbuf_reader] ************** %s ************", next_si->filename.c_str());
    }

    if (next_si->cis->empty() || !is_valid_late(next_si->packet_time)) {
      finish_reading = true;
      // nothing more to do here
      return false;
    }

    return true;
  }

public:
  CBufReader(const std::string& ulog_path, const Options& options = Options())
      : ulog_path_(ulog_path)
      , options_(options) {}
  CBufReader(const Options& options = Options())
      : options_(options) {}

  void setOptions(const Options& options) { options_ = options; }
  void setULogPath(const std::string& ulog_path) { ulog_path_ = ulog_path; }
  void setRoleFilter(const std::string& filter) { role_filter_ = filter; }

  bool addHandler(const std::string& msg_type, std::shared_ptr<CBufHandlerBase> handler) {
    if (msg_map.count(msg_type) > 0) {
      vlog_error(VCAT_GENERAL, "Trying to register a handler twice for message type %s is not allowed",
                 msg_type.c_str());
      return false;
    }
    msg_map[msg_type] = handler;
    return true;
  }

  void setStartTime(double t) { startTime = t; }
  void setEndTime(double t) { endTime = t; }

  template <typename TApp, typename CBufMsg>
  bool addHandler(TApp* owner, CBufMessageHandler<TApp, CBufMsg> h, bool allow_json = true,
                  bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(
        new CBufHandler<TApp, CBufMsg>(owner, h, allow_json, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  template <typename CBufMsg>
  bool addHandler(CBufHandlerLambda<CBufMsg> h, bool allow_json = true, bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(new CBufHandlerLambda<CBufMsg>(h, allow_json, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  template <typename CBufMsg>
  bool addHandlerFn(std::function<void(CBufMsg*)> h, bool allow_json = true, bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(new CBufHandlerLambda<CBufMsg>(h, allow_json, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  bool openUlog(bool error_ok = false) {
    std::vector<std::string> files_in_dir = GetFilesInDirectory(ulog_path_);
    for (auto& f : files_in_dir) {
      if (GetFileExtension(f) == "cb") {
        if (!role_filter_.empty()) {
          if (std::string::npos == f.find(role_filter_, 0)) {
            // skip cbufs that do not contain the role we want to filter on
            continue;
          }
        }
        vlog_fine(VCAT_GENERAL, "Found cbuf file: %s", f.c_str());
        // this is a cb file, open it
        StreamInfo* si = new StreamInfo;
        si->cis = new cbuf_istream();
        si->filename = f;
        std::string fname = ulog_path_ + "/" + f;
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

  double getNextTimestamp() {
    if (!computeNextSi()) return -1;

    return next_si->cis->get_next_timestamp();
  }

  bool processMessage() {
    if (!computeNextSi()) return false;

    auto nhash = next_si->cis->get_next_hash();
    cbuf_istream* next_cis = next_si->cis;
    auto str = next_cis->get_string_for_hash(nhash);

    auto msize = next_cis->get_next_size();
    VLOG_ASSERT(msize != 0 && next_cis->check_next_preamble(),
                "All corrupted cbuf issues should be handled on computeNextSi");

    //    vlog_finer(VCAT_GENERAL, "** Processing type %s", str.c_str());
    if (msg_map.count(str) > 0) {
      bool skip = false;
      if (!msg_map[str]->process_always()) {
        skip = !is_valid_early(next_cis->get_next_timestamp());
      }
      if (!skip) {
        bool consumed = msg_map[str]->processMessage(*next_cis);
        if (!consumed) next_cis->skip_message();
      } else {
        if (!next_cis->skip_message()) return false;
      }
    } else {
      if (!next_cis->skip_message()) return false;
    }

    return true;
  }
};
