#include "cbuf_reader_python.h"

static void split_namespace(const std::string& full_name, std::string& spname, std::string& name) {
  std::string::size_type pos = 0;

  pos = full_name.find("::", pos);
  if (pos == std::string::npos) {
    spname = "";
    name = full_name;
  } else {
    spname = full_name.substr(0, pos);
    name = full_name.substr(pos + 2);
  }
}

// Partial name here is a string entered by a user in the command line, can be missing the namespace
// Full_name comes from the cbuf parser and is fully qualified
static bool match_names(const std::string& full_name, const std::string& partial_name) {
  std::string full_spname, full_regname, partial_spname, partial_regname;
  split_namespace(full_name, full_spname, full_regname);
  split_namespace(partial_name, partial_spname, partial_regname);

  if (!partial_spname.empty()) {
    if (partial_spname != full_spname) {
      // If we have namespaces, they have to match
      return false;
    }
  }

  auto dotpos = partial_regname.find('.');
  if (dotpos != std::string::npos) {
    partial_regname = partial_regname.substr(0, dotpos);
  }
  // If we did not provide a namespace, only try to match the regular name
  if (partial_regname == full_regname) {
    return true;
  }
  return false;
}

static bool name_in_filter(const std::vector<std::string>& filters, const std::string& msg_name) {
  // No filters means we match everything
  if (filters.size() == 0) return true;

  for (const auto& filter_name : filters) {
    if (match_names(msg_name, filter_name)) {
      return true;
    }
  }

  return false;
}

PyObject* CBufReaderPython::getMessage(PyObject* module) {
  while (!finish_reading) {
    if (!computeNextSi()) return nullptr;

    auto nhash = next_si->cis->get_next_hash();
    cbuf_istream* next_cis = next_si->cis;
    auto str = next_cis->get_string_for_hash(nhash);
    // check if the topic name for this message has a namespace

    auto msize = next_cis->get_next_size();
    if (msize == 0 || !next_cis->check_next_preamble()) {
      error_string_ = "All corrupted cbuf issues should be handled on computeNextSi";
      return nullptr;
    }

    // Check for the role filter applying to the cbuf is done on open
    // Check if early or late applies
    // Check if message name filter applies
    if (!is_valid_early(next_cis->get_next_timestamp()) || !name_in_filter(msg_name_filter_, str)) {
      if (!next_cis->skip_message()) {
        finish_reading = true;
        return nullptr;
      }
      // Go again and find another message, next timestamp
      continue;
    }

    // Check if we have an entry on the map
    //   if not, we need to create one, create a parser, look for a type or create it if not
    bool skip_msg_due_to_parsing = false;
    if (msg_map_.contains(str) == 0) {
      // TODO: Find a way so we can load multiple messages but have few parsers
      msg_map_[str].parser = new CBufParserPy();
      auto* parser = msg_map_[str].parser;

      if (!parser->ParseMetadata(next_cis->get_meta_string_for_hash(nhash), str)) {
        error_string_ = "metadata could not be parsed for message " + str;
        msg_map_[str].parsing_failed = true;
        skip_msg_due_to_parsing = true;
      }
    } else if (msg_map_[str].parsing_failed) {
      skip_msg_due_to_parsing = true;
    }

    if (skip_msg_due_to_parsing) {
      if (!next_cis->skip_message()) {
        finish_reading = true;
        return nullptr;
      }
      // Go again and find another message, next timestamp
      continue;
    }

    PyObject* return_obj = nullptr;
    auto& parser = msg_map_[str].parser;
    const char* source_file = nullptr;
    for (const auto& str : source_filters_) {
      if (next_cis->filename().find(str) != std::string::npos) {
        source_file = str.c_str();
        break;
      }
    }
    if (source_file == nullptr) {
      source_file = next_cis->filename().c_str();
    }
    parser->FillPyObject(nhash, str.c_str(), next_cis->get_current_ptr(), next_cis->get_next_size(),
                         source_file, module, return_obj);

    // Careful with who consumes / skips the message
    if (!next_cis->skip_message()) {
      finish_reading = true;
    }

    return return_obj;
  }

  return nullptr;
}

bool CBufReaderPython::openMemory(const char* filename, const char* data, size_t size) {
  if (input_streams.empty()) {
    StreamInfo* si = new StreamInfo();
    si->cis = new cbuf_istream();
    si->filename = filename;
    input_streams.push_back(si);
  }

  if (input_streams.size() != 1) {
    error_string_ = "Only one stream supported for memory, do not mix with files";
    return false;
  }
  input_streams[0]->cis->set_filename(filename);
  input_streams[0]->cis->open_memory((const unsigned char*)data, size);
  // We have to mark finish_reading as false as we could have consumed previous data
  finish_reading = false;
  is_opened = true;
  return true;
}
