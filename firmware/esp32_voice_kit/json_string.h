#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace qh_voice {

inline std::string escapeJsonString(std::string_view input) {
  std::string output;
  output.reserve(input.size());
  for (const unsigned char value : input) {
    switch (value) {
      case '"':
        output += "\\\"";
        break;
      case '\\':
        output += "\\\\";
        break;
      case '\b':
        output += "\\b";
        break;
      case '\f':
        output += "\\f";
        break;
      case '\n':
        output += "\\n";
        break;
      case '\r':
        output += "\\r";
        break;
      case '\t':
        output += "\\t";
        break;
      default:
        if (value < 0x20) {
          char encoded[7];
          std::snprintf(encoded, sizeof(encoded), "\\u%04x", value);
          output += encoded;
        } else {
          output.push_back(static_cast<char>(value));
        }
    }
  }
  return output;
}

inline int hexDigit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

inline bool appendUtf8(std::string& output, uint32_t codepoint) {
  if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return false;
  if (codepoint <= 0x7F) {
    output.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    return false;
  }
  return true;
}

inline std::optional<std::string> parseJsonString(std::string_view json,
                                                   std::size_t quote) {
  if (quote >= json.size() || json[quote] != '"') return std::nullopt;
  std::string output;
  for (std::size_t cursor = quote + 1; cursor < json.size(); ++cursor) {
    const char value = json[cursor];
    if (value == '"') return output;
    if (value != '\\') {
      if (static_cast<unsigned char>(value) < 0x20) return std::nullopt;
      output.push_back(value);
      continue;
    }
    if (++cursor >= json.size()) return std::nullopt;
    switch (json[cursor]) {
      case '"':
      case '\\':
      case '/':
        output.push_back(json[cursor]);
        break;
      case 'b':
        output.push_back('\b');
        break;
      case 'f':
        output.push_back('\f');
        break;
      case 'n':
        output.push_back('\n');
        break;
      case 'r':
        output.push_back('\r');
        break;
      case 't':
        output.push_back('\t');
        break;
      case 'u': {
        if (cursor + 4 >= json.size()) return std::nullopt;
        uint32_t codepoint = 0;
        for (int index = 0; index < 4; ++index) {
          const int digit = hexDigit(json[++cursor]);
          if (digit < 0) return std::nullopt;
          codepoint = (codepoint << 4) | static_cast<uint32_t>(digit);
        }
        if (!appendUtf8(output, codepoint)) return std::nullopt;
        break;
      }
      default:
        return std::nullopt;
    }
  }
  return std::nullopt;
}

inline std::optional<std::string> findJsonString(
    std::string_view json, std::string_view key, std::size_t start = 0) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t cursor = json.find(needle, start);
  if (cursor == std::string_view::npos) return std::nullopt;
  cursor = json.find(':', cursor + needle.size());
  if (cursor == std::string_view::npos) return std::nullopt;
  ++cursor;
  while (cursor < json.size() &&
         std::isspace(static_cast<unsigned char>(json[cursor]))) {
    ++cursor;
  }
  return parseJsonString(json, cursor);
}

inline std::optional<uint32_t> findJsonUnsignedInteger(
    std::string_view json, std::string_view key, std::size_t start = 0) {
  const std::string needle = "\"" + std::string(key) + "\"";
  std::size_t cursor = json.find(needle, start);
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
  uint32_t value = 0;
  while (cursor < json.size() &&
         std::isdigit(static_cast<unsigned char>(json[cursor]))) {
    const uint32_t digit = static_cast<uint32_t>(json[cursor] - '0');
    if (value > (UINT32_MAX - digit) / 10U) return std::nullopt;
    value = value * 10U + digit;
    ++cursor;
  }
  return value;
}

}  // namespace qh_voice
