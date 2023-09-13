#include "cbuf_reader.h"

bool CBufReaderWindow::initialize() {
  // Find the mapping between offsets and states
  max_offset_ = 0;
  std::vector<size_t> lastState = getState();
  while (processGetters()) {
    auto offset = getCurrentOffset();
    auto timestamp = getCurrentTimestamp();

    if (offset && timestamp) {
      if (stateMap_.find(offset.value()) == stateMap_.end()) {
        stateMap_[offset.value()] = lastState;
        timestampMap_[timestamp.value()] = offset.value();
        max_offset_ = std::max(max_offset_, offset.value());
      }
    }

    lastState = getState();
  }

  lastState = getState();
  stateMap_.insert({max_offset_ + 1, lastState});

  for (uint32_t i = max_offset_; i > 0; i--) {
    auto it = stateMap_.lower_bound(i);
    if (it == stateMap_.end()) {
      stateMap_.insert({i, lastState});
    } else {
      lastState = it->second;
    }
  }

  // Run the current handlers for all messages and then remove them
  finish_reading = false;

  for (auto si : input_streams) {
    si->cis->reset_ptr();
  }

  while (processMessage()) {
  }

  for (auto si : input_streams) {
    si->cis->reset_ptr();
  }

  removeHandlers();

  return true;
}

bool CBufReaderWindow::isStateLoaded() {
  std::vector<size_t> state = getState();

  for (uint8_t i = 0; i < state.size(); ++i) {
    // if at least one file offset falls outs of the current loaded interval, the current message isn't loaded
    if (lowest_loaded_state_[i] > state[i] || highest_loaded_state_[i] < state[i]) return false;
  }

  return true;
}

void CBufReaderWindow::keepLoadedStates(const uint32_t low_offset, const uint32_t high_offset) {
  const std::vector<size_t>& low_state = (stateMap_.lower_bound(low_offset))->second;
  std::vector<size_t> high_state = (stateMap_.lower_bound(high_offset + 1))->second;
  high_state[0] -= 1;  // we want [low_state,high_state)

  // We have the window [lowest_loaded_state_,highest_loaded_state_] right now, and we want to load
  // [low_state,high_state). We just keep their intersection. We can do this because the states
  // are strictly increasing with respect to the offset.

  if (lowest_loaded_state_.empty() || highest_loaded_state_.empty()) {  // initialization case
    lowest_loaded_state_ = high_state;
    highest_loaded_state_ = low_state;
  }
  if (low_state > highest_loaded_state_ || high_state < lowest_loaded_state_) {  // no intersection
    lowest_loaded_state_ = high_state;
    highest_loaded_state_ = low_state;
    // set these to signify nothing is loaded
  } else {  // intersection
    lowest_loaded_state_ = std::max(lowest_loaded_state_, low_state);
    highest_loaded_state_ = std::min(highest_loaded_state_, high_state);
  }

  return;
}

bool CBufReaderWindow::loadWindow(const uint32_t seq_offset) {
  finish_reading = false;
  uint32_t starting_offset = (seq_offset >= window_size_) ? seq_offset - window_size_ : 0;
  uint32_t ending_offset =
      (seq_offset + window_size_ <= max_offset_) ? seq_offset + window_size_ : max_offset_;

  if (!jumpToOffset(starting_offset)) return false;

  std::optional<uint32_t> current_offset = {};

  // we should be exactly at a frame info message
  if (!processMessage()) return false;
  if (current_offset = getCurrentOffset(); !current_offset) return false;

  keepLoadedStates(starting_offset,
                   ending_offset);  // discard any file states outside of this sequence offset window
  std::vector<size_t> state = getState();

  // these will represent the window of states that we will have actually loaded by the end of this function
  std::vector<size_t> next_lowest_loaded_state_ = state;
  std::vector<size_t> next_highest_loaded_state_ = state;

  // load states
  while (current_offset.value() <= ending_offset) {
    if (!isStateLoaded()) {
      std::vector<size_t> state = getState();
      next_lowest_loaded_state_ = std::min(next_lowest_loaded_state_, state);
      next_highest_loaded_state_ = std::max(next_highest_loaded_state_, state);

      if (!processMessage()) break;
    } else {
      if (!processSilently()) break;
    }

    if (auto new_offset = getCurrentOffset()) current_offset = new_offset;
  }

  // extend the current loaded window to include the states we just loaded
  lowest_loaded_state_ = std::min(lowest_loaded_state_, next_lowest_loaded_state_);
  highest_loaded_state_ = std::max(highest_loaded_state_, next_highest_loaded_state_);

  return true;
}

uint32_t CBufReaderWindow::timestampToOffset(double timestamp) {
  return (timestampMap_.lower_bound(timestamp))->second - 1;
}

bool CBufReaderWindow::processGetters() {
  if (!computeNextSi()) return false;

  auto nhash = next_si->cis->get_next_hash();
  cbuf_istream* next_cis = next_si->cis;
  auto str = next_cis->get_string_for_hash(nhash);
  last_msg_type_ = str;
  // check if the topic name for this message has a namespace

  auto msize = next_cis->get_next_size();
  if (msize == 0 || !next_cis->check_next_preamble()) {
    error_string_ = "All corrupted cbuf issues should be handled on computeNextSi";
    return false;
  }

  if ((is_external_ || isCorrectBox()) && info_getters_map.count(str) > 0) {
    auto& handler = info_getters_map[str];
    handler->processMessage(*next_cis);
  }

  if (!next_cis->skip_message()) return false;

  return true;
}

bool CBufReaderWindow::processMessage() {
  if (!computeNextSi()) return false;

  auto nhash = next_si->cis->get_next_hash();
  cbuf_istream* next_cis = next_si->cis;
  auto str = next_cis->get_string_for_hash(nhash);
  last_msg_type_ = str;
  // check if the topic name for this message has a namespace

  auto msize = next_cis->get_next_size();
  if (msize == 0 || !next_cis->check_next_preamble()) {
    error_string_ = "All corrupted cbuf issues should be handled on computeNextSi";
    return false;
  }

  if ((is_external_ || isCorrectBox()) && msg_map.count(str) > 0) {
    for (auto& handler : msg_map[str]) {
      if (handler->process_always() || is_valid_early(next_cis->get_next_timestamp())) {
        handler->processMessage(*next_cis);
      }
    }
  }

  if ((is_external_ || isCorrectBox()) && info_getters_map.count(str) > 0) {
    auto& handler = info_getters_map[str];
    handler->processMessage(*next_cis);
  }

  if (!next_cis->skip_message()) return false;

  return true;
}

bool CBufReaderWindow::processSilently() {
  if (!computeNextSi()) return false;

  auto nhash = next_si->cis->get_next_hash();
  cbuf_istream* next_cis = next_si->cis;
  auto str = next_cis->get_string_for_hash(nhash);
  last_msg_type_ = str;
  // check if the topic name for this message has a namespace

  auto msize = next_cis->get_next_size();
  if (msize == 0 || !next_cis->check_next_preamble()) {
    error_string_ = "All corrupted cbuf issues should be handled on computeNextSi";
    return false;
  }

  if ((is_external_ || isCorrectBox()) && info_getters_map.count(str) > 0) {
    auto& handler = info_getters_map[str];
    handler->processMessage(*next_cis);
  }

  if (!next_cis->skip_message()) return false;

  return true;
}

std::vector<size_t> CBufReaderWindow::getState() const {
  std::vector<size_t> state;

  for (auto si : input_streams) {
    state.push_back(si->cis->get_current_offset());
  }

  return state;
}

bool CBufReaderWindow::jumpToOffset(const uint32_t seq_offset) {
  const std::vector<size_t>& state = (stateMap_.lower_bound(seq_offset))->second;

  for (int32_t i = 0; i < state.size(); i++) {
    auto si = input_streams[i];
    size_t jump_offset = state[i];

    if (!(si->cis->jump_to_offset(jump_offset))) return false;
  }

  if (!computeNextSi()) return false;

  return true;
}

std::optional<uint32_t> CBufReaderWindow::getCurrentOffset() {
  if (!is_external_ && !isCorrectBox()) return {};

  if (auto it = info_getters_map.find(last_msg_type_); it != info_getters_map.end()) {
    return it->second->getCurrentOffset();
  }

  return {};
}

std::optional<double> CBufReaderWindow::getCurrentTimestamp() {
  if (!is_external_ && !isCorrectBox()) return {};

  if (auto it = info_getters_map.find(last_msg_type_); it != info_getters_map.end()) {
    return it->second->getCurrentTimestamp();
  }

  return {};
}
