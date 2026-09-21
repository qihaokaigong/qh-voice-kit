#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "openai_reply_protocol.h"

namespace qh_voice {

enum class DoubaoTtsStatus {
  kReceiving,
  kComplete,
  kProviderError,
  kProtocolError,
  kTooLarge,
};

inline std::optional<int> findJsonInteger(std::string_view json,
                                          std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t cursor = json.find(needle);
  if (cursor == std::string_view::npos) return std::nullopt;
  cursor = json.find(':', cursor + needle.size());
  if (cursor == std::string_view::npos) return std::nullopt;
  ++cursor;
  while (cursor < json.size() &&
         std::isspace(static_cast<unsigned char>(json[cursor]))) {
    ++cursor;
  }
  if (cursor == json.size() ||
      !std::isdigit(static_cast<unsigned char>(json[cursor]))) {
    return std::nullopt;
  }
  int value = 0;
  while (cursor < json.size() &&
         std::isdigit(static_cast<unsigned char>(json[cursor]))) {
    const int digit = json[cursor] - '0';
    if (value > (2147483647 - digit) / 10) return std::nullopt;
    value = value * 10 + digit;
    ++cursor;
  }
  return value;
}

inline int base64Value(char value) {
  if (value >= 'A' && value <= 'Z') return value - 'A';
  if (value >= 'a' && value <= 'z') return value - 'a' + 26;
  if (value >= '0' && value <= '9') return value - '0' + 52;
  if (value == '+') return 62;
  if (value == '/') return 63;
  return -1;
}

inline std::optional<std::vector<uint8_t>> decodeBase64(
    std::string_view encoded) {
  if (encoded.empty() || encoded.size() % 4 != 0) return std::nullopt;
  std::vector<uint8_t> output;
  output.reserve((encoded.size() / 4) * 3);
  for (std::size_t cursor = 0; cursor < encoded.size(); cursor += 4) {
    const bool pad2 = encoded[cursor + 2] == '=';
    const bool pad3 = encoded[cursor + 3] == '=';
    if (pad2 && !pad3) return std::nullopt;
    if ((pad2 || pad3) && cursor + 4 != encoded.size()) return std::nullopt;

    const int first = base64Value(encoded[cursor]);
    const int second = base64Value(encoded[cursor + 1]);
    const int third = pad2 ? 0 : base64Value(encoded[cursor + 2]);
    const int fourth = pad3 ? 0 : base64Value(encoded[cursor + 3]);
    if (first < 0 || second < 0 || third < 0 || fourth < 0) {
      return std::nullopt;
    }

    const uint32_t block = (static_cast<uint32_t>(first) << 18) |
                           (static_cast<uint32_t>(second) << 12) |
                           (static_cast<uint32_t>(third) << 6) |
                           static_cast<uint32_t>(fourth);
    output.push_back(static_cast<uint8_t>((block >> 16) & 0xFF));
    if (!pad2) output.push_back(static_cast<uint8_t>((block >> 8) & 0xFF));
    if (!pad3) output.push_back(static_cast<uint8_t>(block & 0xFF));
  }
  return output;
}

class DoubaoTtsSseParser {
 public:
  explicit DoubaoTtsSseParser(std::size_t max_audio_bytes)
      : max_audio_bytes_(max_audio_bytes) {}

  bool feed(std::string_view chunk) {
    if (isError()) return false;
    for (const char value : chunk) {
      if (value == '\n') {
        if (!current_line_.empty() && current_line_.back() == '\r') {
          current_line_.pop_back();
        }
        if (!processLine(current_line_)) return false;
        current_line_.clear();
      } else {
        current_line_.push_back(value);
      }
    }
    return !isError();
  }

  bool finish() {
    if (isError()) return false;
    if (!current_line_.empty()) {
      if (!processLine(current_line_)) return false;
      current_line_.clear();
    }
    if (event_id_.has_value() || !event_data_.empty()) {
      if (!dispatchEvent()) return false;
    }
    if (status_ != DoubaoTtsStatus::kComplete) {
      fail(DoubaoTtsStatus::kProtocolError);
      return false;
    }
    return true;
  }

  DoubaoTtsStatus status() const { return status_; }
  int providerCode() const { return provider_code_; }
  const std::vector<uint8_t>& audio() const { return audio_; }

 private:
  bool isError() const {
    return status_ == DoubaoTtsStatus::kProviderError ||
           status_ == DoubaoTtsStatus::kProtocolError ||
           status_ == DoubaoTtsStatus::kTooLarge;
  }

  void fail(DoubaoTtsStatus status) {
    status_ = status;
    audio_.clear();
  }

  bool processLine(std::string_view line) {
    if (line.empty()) return dispatchEvent();
    if (line.rfind("event:", 0) == 0) {
      std::size_t cursor = 6;
      while (cursor < line.size() && line[cursor] == ' ') ++cursor;
      if (cursor == line.size()) {
        fail(DoubaoTtsStatus::kProtocolError);
        return false;
      }
      int event = 0;
      for (; cursor < line.size(); ++cursor) {
        if (!std::isdigit(static_cast<unsigned char>(line[cursor]))) {
          fail(DoubaoTtsStatus::kProtocolError);
          return false;
        }
        event = event * 10 + (line[cursor] - '0');
      }
      event_id_ = event;
      return true;
    }
    if (line.rfind("data:", 0) == 0) {
      std::size_t cursor = 5;
      if (cursor < line.size() && line[cursor] == ' ') ++cursor;
      if (!event_data_.empty()) event_data_.push_back('\n');
      event_data_.append(line.substr(cursor));
    }
    return true;
  }

  bool dispatchEvent() {
    if (!event_id_.has_value() && event_data_.empty()) return true;
    if (!event_id_.has_value() || event_data_.empty()) {
      fail(DoubaoTtsStatus::kProtocolError);
      return false;
    }

    const int event = *event_id_;
    const std::string data = event_data_;
    event_id_.reset();
    event_data_.clear();

    const auto code = findJsonInteger(data, "code");
    if (!code.has_value()) {
      fail(DoubaoTtsStatus::kProtocolError);
      return false;
    }
    provider_code_ = *code;

    if (event == 352) {
      if (*code != 0) {
        fail(DoubaoTtsStatus::kProviderError);
        return false;
      }
      const auto encoded = findJsonString(data, "data", 0);
      if (!encoded.has_value()) {
        fail(DoubaoTtsStatus::kProtocolError);
        return false;
      }
      auto decoded = decodeBase64(*encoded);
      if (!decoded.has_value()) {
        fail(DoubaoTtsStatus::kProtocolError);
        return false;
      }
      if (decoded->size() > max_audio_bytes_ -
                                std::min(max_audio_bytes_, audio_.size())) {
        fail(DoubaoTtsStatus::kTooLarge);
        return false;
      }
      audio_.insert(audio_.end(), decoded->begin(), decoded->end());
      return true;
    }
    if (event == 152) {
      if (*code != 20000000) {
        fail(DoubaoTtsStatus::kProviderError);
        return false;
      }
      status_ = DoubaoTtsStatus::kComplete;
      return true;
    }
    if (event == 153) {
      fail(DoubaoTtsStatus::kProviderError);
      return false;
    }
    return true;
  }

  std::size_t max_audio_bytes_;
  DoubaoTtsStatus status_ = DoubaoTtsStatus::kReceiving;
  int provider_code_ = 0;
  std::optional<int> event_id_;
  std::string event_data_;
  std::string current_line_;
  std::vector<uint8_t> audio_;
};

}  // namespace qh_voice
