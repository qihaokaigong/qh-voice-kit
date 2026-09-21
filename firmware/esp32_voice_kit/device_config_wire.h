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
constexpr uint8_t kDeviceConfigWireVersion = 1;
constexpr uint8_t kDeviceConfigWireFieldCount = 24;

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
  appendConfigField(output, 3, config.stt.adapter);
  appendConfigField(output, 4, config.stt.endpoint);
  appendConfigField(output, 5, config.stt.app_key);
  appendConfigField(output, 6, config.stt.credential);
  appendConfigField(output, 7, config.stt.resource_id);
  appendConfigField(output, 8, config.reply.adapter);
  appendConfigField(output, 9, config.reply.endpoint);
  appendConfigField(output, 10, config.reply.model);
  appendConfigField(output, 11, config.reply.credential);
  appendConfigField(output, 12, config.tts.adapter);
  appendConfigField(output, 13, config.tts.endpoint);
  appendConfigField(output, 14, config.tts.credential);
  appendConfigField(output, 15, config.tts.resource_id);
  appendConfigField(output, 16, config.tts.speaker);
  appendConfigField(output, 17, config.assistant.language);
  appendConfigField(output, 18, config.assistant.system_prompt);
  appendConfigField(output, 19,
                    std::to_string(config.assistant.max_reply_chars));
  appendConfigField(output, 20, config.qh_sync.enabled ? "1" : "0");
  appendConfigField(output, 21, config.qh_sync.endpoint);
  appendConfigField(output, 22, config.qh_sync.device_id);
  appendConfigField(output, 23, config.qh_sync.credential);
  appendConfigField(output, 24,
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
      case 3: config.stt.adapter = value; break;
      case 4: config.stt.endpoint = value; break;
      case 5: config.stt.app_key = value; break;
      case 6: config.stt.credential = value; break;
      case 7: config.stt.resource_id = value; break;
      case 8: config.reply.adapter = value; break;
      case 9: config.reply.endpoint = value; break;
      case 10: config.reply.model = value; break;
      case 11: config.reply.credential = value; break;
      case 12: config.tts.adapter = value; break;
      case 13: config.tts.endpoint = value; break;
      case 14: config.tts.credential = value; break;
      case 15: config.tts.resource_id = value; break;
      case 16: config.tts.speaker = value; break;
      case 17: config.assistant.language = value; break;
      case 18: config.assistant.system_prompt = value; break;
      case 19: {
        uint32_t parsed = 0;
        if (!parseConfigUnsigned(value,
                                 std::numeric_limits<uint16_t>::max(), parsed)) {
          return {ConfigWireError::kInvalidNumber, std::nullopt};
        }
        config.assistant.max_reply_chars = static_cast<uint16_t>(parsed);
        break;
      }
      case 20:
        if (value == "1") {
          config.qh_sync.enabled = true;
        } else if (value == "0") {
          config.qh_sync.enabled = false;
        } else {
          return {ConfigWireError::kInvalidNumber, std::nullopt};
        }
        break;
      case 21: config.qh_sync.endpoint = value; break;
      case 22: config.qh_sync.device_id = value; break;
      case 23: config.qh_sync.credential = value; break;
      case 24: {
        uint32_t parsed = 0;
        if (!parseConfigUnsigned(value,
                                 std::numeric_limits<uint8_t>::max(), parsed)) {
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
