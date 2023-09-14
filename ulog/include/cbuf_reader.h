#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <unordered_map>

#include "CBufParser.h"
#include "cbuf_readerbase.h"

namespace fs = std::filesystem;

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
  CBufParser* parserCurrent = nullptr;
  bool allow_conversion = true;
  bool warned_conversion = false;

public:
  CBufHandler(TApp* owner, CBufMessageHandler<TApp, CBufMsg> h, bool allow_conv = true,
              bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , caller(owner)
      , handler(h)
      , allow_conversion(allow_conv) {}

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
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // Could not deserialize message of type `CBufMsg::TYPE_STRING`
        return false;
      }
      (caller->*handler)(msg);
      return true;
    }
    auto msg_type = cis.get_string_for_hash(hash);
    if (msg_type == CBufMsg::TYPE_STRING) {
      if (allow_conversion) {
        // the hash did not match but has the same name, try to do conversion
        if (parser == nullptr) {
          parser = new CBufParser();
          if (!parser->ParseMetadata(cis.get_meta_string_for_hash(hash), CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        if (parserCurrent == nullptr) {
          parserCurrent = new CBufParser();
          if (!parserCurrent->ParseMetadata(CBufMsg::cbuf_string, CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for current message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        // Initialize the fields on the cbuf in order to ensure when backwards compat
        // does not fill in fields (that are not there) still have sane values
        msg->Init();
        auto fillret = parser->FastConversion(CBufMsg::TYPE_STRING, cis.get_current_ptr(),
                                              cis.get_next_size(), *parserCurrent, CBufMsg::TYPE_STRING,
                                              (unsigned char*)msg, sizeof(CBufMsg));
        if (fillret == 0) return false;
        msg->preamble.packet_timest = cis.get_next_timestamp();

        (caller->*handler)(msg);
        return true;
      }
      if (!warned_conversion) {
        // cbuf version mismatch for message `CBufMsg::TYPE_STRING` and conversion is not allowed
        warned_conversion = true;
      }
    }
    // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
    return false;
  }
};

template <typename TApp, typename CBufMsg>
using CBufBoxMessageHandler = void (TApp::*)(CBufMsg* msg, const std::string& box_name);

template <typename TApp, typename CBufMsg>
class CBufBoxHandler : public CBufHandlerBase {
  TApp* caller = nullptr;
  CBufBoxMessageHandler<TApp, CBufMsg> handler = nullptr;
  CBufMsg* msg = nullptr;
  CBufParser* parser = nullptr;
  CBufParser* parserCurrent = nullptr;
  bool allow_conversion = true;
  bool warned_conversion = false;

public:
  CBufBoxHandler(TApp* owner, CBufBoxMessageHandler<TApp, CBufMsg> h, bool allow_conv = true,
                 bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , caller(owner)
      , handler(h)
      , allow_conversion(allow_conv) {}

  ~CBufBoxHandler() override {
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
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // Could not deserialize message of type `CBufMsg::TYPE_STRING`
        return false;
      }

      // Assuming the istream has been instantiated
      (caller->*handler)(msg, fs::path(cis.filename()).filename());
      return true;
    }
    auto msg_type = cis.get_string_for_hash(hash);
    if (msg_type == CBufMsg::TYPE_STRING) {
      if (allow_conversion) {
        // the hash did not match but has the same name, try to do conversion
        if (parser == nullptr) {
          parser = new CBufParser();
          if (!parser->ParseMetadata(cis.get_meta_string_for_hash(hash), CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        if (parserCurrent == nullptr) {
          parserCurrent = new CBufParser();
          if (!parserCurrent->ParseMetadata(CBufMsg::cbuf_string, CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for current message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        // Initialize the fields on the cbuf in order to ensure when backwards compat
        // does not fill in fields (that are not there) still have sane values
        msg->Init();
        auto fillret = parser->FastConversion(CBufMsg::TYPE_STRING, cis.get_current_ptr(),
                                              cis.get_next_size(), *parserCurrent, CBufMsg::TYPE_STRING,
                                              (unsigned char*)msg, sizeof(CBufMsg));
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        if (fillret == 0) return false;
        msg->preamble.packet_timest = cis.get_next_timestamp();

        (caller->*handler)(msg, fs::path(cis.filename()).filename());
        return true;
      }
      if (!warned_conversion) {
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // cbuf version mismatch for message `CBufMsg::TYPE_STRING` and conversion is not allowed
        warned_conversion = true;
      }
    }
    // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
    return false;
  }
};

template <typename CBufMsg>
using CBufHandlerLambdaFn = std::function<void(CBufMsg*)>;

template <typename CBufMsg>
using CBufBoxHandlerLambdaFn = std::function<void(CBufMsg*, const std::string& box_name)>;

template <typename CBufMsg>
class CBufHandlerLambda : public CBufHandlerBase {
  CBufHandlerLambdaFn<CBufMsg> handler = nullptr;
  CBufMsg* msg = nullptr;
  CBufParser* parser = nullptr;
  CBufParser* parserCurrent = nullptr;
  bool allow_conversion = true;
  bool warned_conversion = false;

public:
  CBufHandlerLambda(void (*h)(CBufMsg*), bool allow_conv = true, bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , handler(h)
      , allow_conversion(allow_conv) {}

  CBufHandlerLambda(std::function<void(CBufMsg*)> h, bool allow_conv = true, bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , handler(h)
      , allow_conversion(allow_conv) {}

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
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // Could not deserialize message of type `CBufMsg::TYPE_STRING`
        return false;
      }
      handler(msg);
      return true;
    }
    auto msg_type = cis.get_string_for_hash(hash);
    if (msg_type == CBufMsg::TYPE_STRING) {
      if (allow_conversion) {
        // the hash did not match but has the same name, try to do conversion
        if (parser == nullptr) {
          parser = new CBufParser();
          if (!parser->ParseMetadata(cis.get_meta_string_for_hash(hash), CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        if (parserCurrent == nullptr) {
          parserCurrent = new CBufParser();
          if (!parserCurrent->ParseMetadata(CBufMsg::cbuf_string, CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for current message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        // Initialize the fields on the cbuf in order to ensure when backwards compat
        // does not fill in fields (that are not there) still have sane values
        msg->Init();
        auto fillret = parser->FastConversion(CBufMsg::TYPE_STRING, cis.get_current_ptr(),
                                              cis.get_next_size(), *parserCurrent, CBufMsg::TYPE_STRING,
                                              (unsigned char*)msg, sizeof(CBufMsg));
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        if (fillret == 0) return false;

        handler(msg);
        return true;
      }
      if (!warned_conversion) {
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // cbuf version mismatch for message `CBufMsg::TYPE_STRING` and conversion is not allowed
        warned_conversion = true;
      }
    }
    // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
    return false;
  }
};

// Partial specialisation of the template
template <typename CBufMsg>
class CBufBoxHandlerLambda : public CBufHandlerBase {
  CBufBoxHandlerLambdaFn<CBufMsg> handler = nullptr;
  CBufMsg* msg = nullptr;
  CBufParser* parser = nullptr;
  CBufParser* parserCurrent = nullptr;
  bool allow_conversion = true;
  bool warned_conversion = false;

public:
  CBufBoxHandlerLambda(void (*h)(CBufMsg*, std::string), bool allow_conversion = true,
                       bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , handler(h)
      , allow_conversion(allow_conversion) {}

  CBufBoxHandlerLambda(std::function<void(CBufMsg*, std::string)> h, bool allow_conv = true,
                       bool process_always = false)
      : CBufHandlerBase(CBufMsg::TYPE_STRING, process_always)
      , handler(h)
      , allow_conversion(allow_conv) {}

  ~CBufBoxHandlerLambda() override {
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
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // Could not deserialize message of type `CBufMsg::TYPE_STRING`
        return false;
      }
      // assuming the istream has been instantiated
      (handler)(msg, fs::path(cis.filename()).filename());
      return true;
    }
    auto msg_type = cis.get_string_for_hash(hash);
    if (msg_type == CBufMsg::TYPE_STRING) {
      if (allow_conversion) {
        // the hash did not match but has the same name, try to do conversion
        if (parser == nullptr) {
          parser = new CBufParser();
          if (!parser->ParseMetadata(cis.get_meta_string_for_hash(hash), CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        if (parserCurrent == nullptr) {
          parserCurrent = new CBufParser();
          if (!parserCurrent->ParseMetadata(CBufMsg::cbuf_string, CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for current message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        // Initialize the fields on the cbuf in order to ensure when backwards compat
        // does not fill in fields (that are not there) still have sane values
        msg->Init();
        auto fillret = parser->FastConversion(CBufMsg::TYPE_STRING, cis.get_current_ptr(),
                                              cis.get_next_size(), *parserCurrent, CBufMsg::TYPE_STRING,
                                              (unsigned char*)msg, sizeof(CBufMsg));
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        if (fillret == 0) return false;

        // assuming the istream has been instantiated
        (handler)(msg, fs::path(cis.filename()).filename());
        return true;
      }
      if (!warned_conversion) {
        // cbuf version mismatch for message `CBufMsg::TYPE_STRING` and conversion is not allowed
        warned_conversion = true;
      }
    }
    // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
    return false;
  }
};

class CBufReader : public CBufReaderBase {
  std::unordered_map<std::string, std::vector<std::shared_ptr<CBufHandlerBase>>> msg_map;
  std::function<void(cbuf_istream*)> cis_callback_;
  bool use_cis_callback_ = false;

public:
  CBufReader(const std::string& ulog_path, const Options& options = Options())
      : CBufReaderBase(ulog_path, options) {}
  CBufReader(const Options& options = Options())
      : CBufReaderBase(options) {}

  [[deprecated]] void setRoleFilter(const std::string& filter) {
    error_string_ =
        "setRoleFilter is deprecated. Please update your code to handle or ignore messages in each handler "
        "callback";
    source_filters_.push_back(filter);
  }

  bool addHandler(const std::string& msg_type, std::shared_ptr<CBufHandlerBase> handler) {
    msg_map[msg_type].push_back(handler);
    return true;
  }

  template <typename TApp, typename CBufMsg>
  bool addHandler(TApp* owner, CBufMessageHandler<TApp, CBufMsg> h, bool allow_conversion = true,
                  bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(
        new CBufHandler<TApp, CBufMsg>(owner, h, allow_conversion, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  template <typename CBufMsg>
  bool addHandler(CBufHandlerLambdaFn<CBufMsg> h, bool allow_conversion = true, bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(new CBufHandlerLambda<CBufMsg>(h, allow_conversion, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  template <typename CBufMsg>
  bool addHandlerFn(std::function<void(CBufMsg*)> h, bool allow_conversion = true,
                    bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(new CBufHandlerLambda<CBufMsg>(h, allow_conversion, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  template <typename TApp, typename CBufMsg>
  bool addHandler(TApp* owner, CBufBoxMessageHandler<TApp, CBufMsg> h, bool allow_conversion = true,
                  bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(
        new CBufBoxHandler<TApp, CBufMsg>(owner, h, allow_conversion, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  template <typename CBufMsg>
  bool addHandler(CBufBoxHandlerLambdaFn<CBufMsg> h, bool allow_conversion = true,
                  bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(
        new CBufBoxHandlerLambda<CBufMsg>(h, allow_conversion, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  template <typename CBufMsg>
  bool addHandlerFn(std::function<void(CBufMsg*, std::string)> h, bool allow_conversion = true,
                    bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(
        new CBufBoxHandlerLambda<CBufMsg>(h, allow_conversion, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  // CBufIStream Callback can be used to process a message directly using a cbuf_istream instead of
  // CBufReader doing the message decoding
  void addCbufIStreamCallback(std::function<void(cbuf_istream*)> h) {
    cis_callback_ = h;
    use_cis_callback_ = true;
  }

  bool processMessage() {
    if (!computeNextSi()) return false;

    auto nhash = next_si->cis->get_next_hash();
    cbuf_istream* next_cis = next_si->cis;
    auto str = next_cis->get_string_for_hash(nhash);
    // check if the topic name for this message has a namespace

    auto msize = next_cis->get_next_size();
    if (msize == 0 || !next_cis->check_next_preamble()) {
      error_string_ = "All corrupted cbuf issues should be handled on computeNextSi";
      return false;
    }

    if (use_cis_callback_) {
      cis_callback_(next_cis);
    }

    if (msg_map.count(str) > 0) {
      for (auto& handler : msg_map[str]) {
        if (handler->process_always() || is_valid_early(next_cis->get_next_timestamp())) {
          handler->processMessage(*next_cis);
        }
      }
    }

    if (!next_cis->skip_message()) return false;

    return true;
  }
};

class CBufInfoGetterBase {
public:
  CBufInfoGetterBase() {}
  virtual ~CBufInfoGetterBase() {}

  virtual bool processMessage(cbuf_istream& cis) = 0;

  virtual std::optional<uint32_t> getCurrentOffset() = 0;
  virtual std::optional<double> getCurrentTimestamp() = 0;
};

template <typename TApp, typename CBufMsg>
using CBufOffsetGetter = uint32_t (TApp::*)(CBufMsg* msg);

template <typename TApp, typename CBufMsg>
using CBufTimestampGetter = double (TApp::*)(CBufMsg* msg);

template <typename TApp, typename CBufMsg>
class CBufInfoGetter : public CBufInfoGetterBase {
  TApp* caller = nullptr;
  CBufOffsetGetter<TApp, CBufMsg> offsetGetter_ = nullptr;
  CBufTimestampGetter<TApp, CBufMsg> timestampGetter_ = nullptr;
  CBufMsg* msg = nullptr;
  CBufParser* parser = nullptr;
  CBufParser* parserCurrent = nullptr;
  bool allow_conversion = true;
  bool warned_conversion = false;
  bool process_always_;

public:
  CBufInfoGetter(TApp* owner, CBufOffsetGetter<TApp, CBufMsg> offsetGetter,
                 CBufTimestampGetter<TApp, CBufMsg> timestampGetter, bool allow_conv = true,
                 bool process_always = false)
      : CBufInfoGetterBase()
      , caller(owner)
      , offsetGetter_(offsetGetter)
      , timestampGetter_(timestampGetter)
      , allow_conversion(allow_conv)
      , process_always_(process_always) {}

  ~CBufInfoGetter() override {
    if (msg) {
      delete msg;
      msg = nullptr;
    }
  }

  bool isMessageOfType(cbuf_istream& cis, std::string_view type) {
    return cis.get_string_for_hash(cis.get_next_hash()) == type;
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
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // Could not deserialize message of type `CBufMsg::TYPE_STRING`
        return false;
      }
      return true;
    }
    auto msg_type = cis.get_string_for_hash(hash);
    if (msg_type == CBufMsg::TYPE_STRING) {
      if (allow_conversion) {
        // the hash did not match but has the same name, try to do conversion
        if (parser == nullptr) {
          parser = new CBufParser();
          if (!parser->ParseMetadata(cis.get_meta_string_for_hash(hash), CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        if (parserCurrent == nullptr) {
          parserCurrent = new CBufParser();
          if (!parserCurrent->ParseMetadata(CBufMsg::cbuf_string, CBufMsg::TYPE_STRING)) {
            // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
            // Metadata could not be parsed for current message `CBufMsg::TYPE_STRING`
            return false;
          }
        }

        // Initialize the fields on the cbuf in order to ensure when backwards compat
        // does not fill in fields (that are not there) still have sane values
        msg->Init();
        auto fillret = parser->FastConversion(CBufMsg::TYPE_STRING, cis.get_current_ptr(),
                                              cis.get_next_size(), *parserCurrent, CBufMsg::TYPE_STRING,
                                              (unsigned char*)msg, sizeof(CBufMsg));
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        if (fillret == 0) return false;
        msg->preamble.packet_timest = cis.get_next_timestamp();

        return true;
      }
      if (!warned_conversion) {
        // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
        // cbuf version mismatch for message `CBufMsg::TYPE_STRING` and conversion is not allowed
        warned_conversion = true;
      }
    }
    // TODO(https://github.com/Verdant-Robotics/cbuf/issues/9): Better error handling
    return false;
  }

  std::optional<uint32_t> getCurrentOffset() override {
    if (msg != nullptr) {
      return (caller->*offsetGetter_)(msg);
    }

    return {};
  }

  std::optional<double> getCurrentTimestamp() override {
    if (msg != nullptr) {
      return (caller->*timestampGetter_)(msg);
    }

    return {};
  }
};

class CBufReaderWindow : public CBufReaderBase {
  uint32_t window_size_;  // how many frames left and right to load
  uint32_t max_offset_;
  std::map<double, uint32_t> timestampMap_;
  std::map<uint32_t, std::vector<size_t>> stateMap_;
  std::string box_name_;
  std::unordered_map<std::string, std::vector<std::shared_ptr<CBufHandlerBase>>> msg_map;
  std::unordered_map<std::string, std::shared_ptr<CBufInfoGetterBase>> info_getters_map;
  std::vector<size_t> lowest_loaded_state_;
  std::vector<size_t> highest_loaded_state_;
  bool is_external_ = false;
  std::string last_msg_type_;

public:
  CBufReaderWindow(const std::string& ulog_path, const Options& options = Options())
      : CBufReaderBase(ulog_path, options) {}
  CBufReaderWindow(const Options& options = Options())
      : CBufReaderBase(options) {}

  bool addHandler(const std::string& msg_type, std::shared_ptr<CBufHandlerBase> handler) {
    msg_map[msg_type].push_back(handler);
    return true;
  }

  template <typename TApp, typename CBufMsg>
  bool addHandler(TApp* owner, CBufMessageHandler<TApp, CBufMsg> h, bool allow_conversion = true,
                  bool process_always = false) {
    std::shared_ptr<CBufHandlerBase> ptr(
        new CBufHandler<TApp, CBufMsg>(owner, h, allow_conversion, process_always));
    return addHandler(CBufMsg::TYPE_STRING, ptr);
  }

  bool addInfoGetter(const std::string& msg_type, std::shared_ptr<CBufInfoGetterBase> getter) {
    info_getters_map[msg_type] = getter;
    return true;
  }

  template <typename TApp, typename CBufMsg>
  bool addInfoGetter(TApp* owner, CBufOffsetGetter<TApp, CBufMsg> ig, CBufTimestampGetter<TApp, CBufMsg> tg) {
    std::shared_ptr<CBufInfoGetterBase> ptr(new CBufInfoGetter<TApp, CBufMsg>(owner, ig, tg));
    return addInfoGetter(CBufMsg::TYPE_STRING, ptr);
  }

  void setBoxName(const std::string box_name) { box_name_ = box_name; }
  void setWindowSize(const uint32_t window_size) { window_size_ = window_size; }
  void setExternal() { is_external_ = true; }

  bool initialize();

  bool loadWindow(const uint32_t seq_offset);

  size_t GetNumFrames() const { return stateMap_.size(); }

  uint32_t timestampToOffset(double timestamp);

  uint32_t maxSequenceOffset() { return max_offset_ - 1; }

private:
  bool processMessage();
  bool processSilently();
  bool processGetters();

  void removeHandlers() { msg_map.clear(); }

  bool jumpToOffset(const uint32_t);

  bool isCorrectBox() const { return next_si->cis->filename().find(box_name_) != std::string::npos; }
  std::string getMessageType() { return next_si->cis->get_string_for_hash(next_si->cis->get_next_hash()); }

  std::optional<uint32_t> getCurrentOffset();
  std::optional<double> getCurrentTimestamp();

  // functions to handle which file states we have already loaded to avoid double loading in our maps
  std::vector<size_t> getState() const;
  bool isStateLoaded();
  void updateLoadedStates();
  void updateCurrentLoadedStates();
  void initializeCurrentLoadedStates();
  void keepLoadedStates(const uint32_t low_offset, const uint32_t high_offset);
};
