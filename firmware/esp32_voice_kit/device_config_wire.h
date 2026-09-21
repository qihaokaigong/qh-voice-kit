#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "device_config.h"

namespace qh_voice {

constexpr std::size_t kMaximumDeviceConfigWireBytes = 8192;
constexpr uint8_t kDeviceConfigWireVersion = 3;
constexpr uint8_t kDeviceConfigWireFieldCount = 13;

enum class ConfigWireError {
  kNone,
  kTooLarge,
  kBadMagic,
  kUnsupportedVersion,
  kTruncated,
  kUnknownField,
  kDuplicateField,
  kMissingField,
  kInvalidNumber,
  kInvalidConfig,
};

struct ConfigWireDecodeResult {
  ConfigWireError error;
  std::optional<DeviceConfig> config;
};

inline void appendConfigField(std::vector<uint8_t>& output, uint8_t id,
                              std::string_view value) {
  output.push_back(id);
  output.push_back(static_cast<uint8_t>(value.size() >> 8));
  output.push_back(static_cast<uint8_t>(value.size()));
  output.insert(output.end(), value.begin(), value.end());
}

inline std::vector<uint8_t> encodeDeviceConfigWire(const DeviceConfig& config) {
  std::vector<uint8_t> output{'Q', 'H', 'V', 'C', kDeviceConfigWireVersion,
                              kDeviceConfigWireFieldCount};
  appendConfigField(output, 1, config.network.ssid);
  appendConfigField(output, 2, config.network.password);
  appendConfigField(output, 3, config.realtime_voice.adapter);
  appendConfigField(output, 4, config.realtime_voice.api_key);
  appendConfigField(output, 5, config.realtime_voice.voice);
  appendConfigField(output, 6, config.assistant.language);
  appendConfigField(output, 7, config.assistant.system_prompt);
  appendConfigField(output, 8, config.assistant.show_reply_text ? "1" : "0");
  appendConfigField(output, 9, config.qh_sync.enabled ? "1" : "0");
  appendConfigField(output, 10, config.qh_sync.endpoint);
  appendConfigField(output, 11, config.qh_sync.device_id);
  appendConfigField(output, 12, config.qh_sync.credential);
  appendConfigField(output, 13,
                    std::to_string(config.preferences.volume_percent));
  return output;
}

inline bool parseConfigUnsigned(std::string_view value, uint32_t maximum,
                                uint32_t& output) {
  if (value.empty()) return false;
  uint32_t parsed = 0;
  for (const char character : value) {
    if (character < '0' || character > '9') return false;
    const uint32_t digit = static_cast<uint32_t>(character - '0');
    if (parsed > (maximum - digit) / 10) return false;
    parsed = parsed * 10 + digit;
  }
  output = parsed;
  return true;
}

inline bool parseConfigBoolean(std::string_view value, bool& output) {
  if (value == "1") {
    output = true;
    return true;
  }
  if (value == "0") {
    output = false;
    return true;
  }
  return false;
}

inline ConfigWireDecodeResult decodeDeviceConfigWire(const uint8_t* input,
                                                       std::size_t size) {
  if (size > kMaximumDeviceConfigWireBytes) {
    return {ConfigWireError::kTooLarge, std::nullopt};
  }
  if (input == nullptr || size < 6) {
    return {ConfigWireError::kTruncated, std::nullopt};
  }
  if (input[0] != 'Q' || input[1] != 'H' || input[2] != 'V' ||
      input[3] != 'C') {
    return {ConfigWireError::kBadMagic, std::nullopt};
  }
  if (input[4] != kDeviceConfigWireVersion) {
    return {ConfigWireError::kUnsupportedVersion, std::nullopt};
  }

  DeviceConfig config{};
  config.schema_version = kDeviceConfigSchemaVersion;
  std::array<bool, kDeviceConfigWireFieldCount + 1> seen{};
  std::size_t cursor = 6;
  const uint8_t field_count = input[5];
  for (uint8_t index = 0; index < field_count; ++index) {
    if (size - cursor < 3) {
      return {ConfigWireError::kTruncated, std::nullopt};
    }
    const uint8_t id = input[cursor++];
    const std::size_t length =
        (static_cast<std::size_t>(input[cursor]) << 8) | input[cursor + 1];
    cursor += 2;
    if (id == 0 || id > kDeviceConfigWireFieldCount) {
      return {ConfigWireError::kUnknownField, std::nullopt};
    }
    if (seen[id]) return {ConfigWireError::kDuplicateField, std::nullopt};
    if (length > size - cursor) {
      return {ConfigWireError::kTruncated, std::nullopt};
    }
    seen[id] = true;
    const std::string value(reinterpret_cast<const char*>(input + cursor),
                            length);
    cursor += length;
    switch (id) {
      case 1: config.network.ssid = value; break;
      case 2: config.network.password = value; break;
      case 3: config.realtime_voice.adapter = value; break;
      case 4: config.realtime_voice.api_key = value; break;
      case 5: config.realtime_voice.voice = value; break;
      case 6: config.assistant.language = value; break;
      case 7: config.assistant.system_prompt = value; break;
      case 8:
        if (!parseConfigBoolean(value, config.assistant.show_reply_text)) {
          return {ConfigWireError::kInvalidNumber, std::nullopt};
        }
        break;
      case 9:
        if (!parseConfigBoolean(value, config.qh_sync.enabled)) {
          return {ConfigWireError::kInvalidNumber, std::nullopt};
        }
        break;
      case 10: config.qh_sync.endpoint = value; break;
      case 11: config.qh_sync.device_id = value; break;
      case 12: config.qh_sync.credential = value; break;
      case 13: {
        uint32_t parsed = 0;
        if (!parseConfigUnsigned(value, std::numeric_limits<uint8_t>::max(),
                                 parsed)) {
          return {ConfigWireError::kInvalidNumber, std::nullopt};
        }
        config.preferences.volume_percent = static_cast<uint8_t>(parsed);
        break;
      }
      default: return {ConfigWireError::kUnknownField, std::nullopt};
    }
  }
  if (cursor != size) return {ConfigWireError::kTruncated, std::nullopt};
  for (uint8_t id = 1; id <= kDeviceConfigWireFieldCount; ++id) {
    if (!seen[id]) return {ConfigWireError::kMissingField, std::nullopt};
  }
  if (!validateDeviceConfig(config).ok()) {
    return {ConfigWireError::kInvalidConfig, std::nullopt};
  }
  return {ConfigWireError::kNone, config};
}

}  // namespace qh_voice
