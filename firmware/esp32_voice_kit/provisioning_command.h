#pragma once

#include <charconv>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>

#include "device_config_wire.h"

namespace qh_voice {

enum class ProvisioningCommandType { kInvalid, kStatus, kConfig };

struct ProvisioningCommand {
  ProvisioningCommandType type;
  std::size_t payload_size;
  std::string sha256;
};

inline bool isSha256Hex(std::string_view value) {
  if (value.size() != 64) return false;
  for (const unsigned char character : value) {
    if (!std::isxdigit(character)) return false;
  }
  return true;
}

inline ProvisioningCommand parseProvisioningCommand(std::string_view line) {
  if (line == "QH_VOICE_STATUS") {
    return {ProvisioningCommandType::kStatus, 0, ""};
  }

  std::istringstream stream{std::string(line)};
  std::string name;
  std::string size_text;
  std::string digest;
  std::string trailing;
  if (!(stream >> name >> size_text >> digest) || (stream >> trailing) ||
      name != "QH_VOICE_CONFIG" || !isSha256Hex(digest)) {
    return {ProvisioningCommandType::kInvalid, 0, ""};
  }
  std::size_t payload_size = 0;
  const auto parsed = std::from_chars(
      size_text.data(), size_text.data() + size_text.size(), payload_size);
  if (parsed.ec != std::errc{} || parsed.ptr != size_text.data() + size_text.size() ||
      payload_size == 0 || payload_size > kMaximumDeviceConfigWireBytes) {
    return {ProvisioningCommandType::kInvalid, 0, ""};
  }
  for (char& character : digest) {
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  }
  return {ProvisioningCommandType::kConfig, payload_size, digest};
}

}  // namespace qh_voice
