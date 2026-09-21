#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "secure_endpoint.h"

namespace qh_voice {

constexpr uint16_t kDeviceConfigSchemaVersion = 2;

struct NetworkConfig {
  std::string ssid;
  std::string password;
};

struct SttConfig {
  std::string adapter;
  std::string endpoint;
  std::string api_key;
  std::string resource_id;
};

struct ReplyConfig {
  std::string adapter;
  std::string endpoint;
  std::string model;
  std::string credential;
};

struct TtsConfig {
  std::string adapter;
  std::string endpoint;
  std::string credential;
  std::string resource_id;
  std::string speaker;
};

struct AssistantConfig {
  std::string language;
  std::string system_prompt;
  uint16_t max_reply_chars;
};

struct QhSyncConfig {
  bool enabled;
  std::string endpoint;
  std::string device_id;
  std::string credential;
};

struct PreferencesConfig {
  uint8_t volume_percent;
};

struct DeviceConfig {
  uint16_t schema_version;
  NetworkConfig network;
  SttConfig stt;
  ReplyConfig reply;
  TtsConfig tts;
  AssistantConfig assistant;
  QhSyncConfig qh_sync;
  PreferencesConfig preferences;
};

enum class ConfigError {
  kUnsupportedSchema,
  kMissingWifiSsid,
  kMissingWifiPassword,
  kMissingSttCredential,
  kMissingReplyCredential,
  kMissingTtsCredential,
  kUnsupportedSttAdapter,
  kUnsupportedReplyAdapter,
  kUnsupportedTtsAdapter,
  kInvalidSttConfig,
  kInvalidReplyConfig,
  kInvalidTtsConfig,
  kInvalidSttEndpoint,
  kInvalidReplyEndpoint,
  kInvalidTtsEndpoint,
  kInvalidQhSync,
  kInvalidReplyLimit,
  kInvalidVolume,
};

struct ConfigValidationResult {
  std::vector<ConfigError> errors;

  bool ok() const { return errors.empty(); }

  bool has(ConfigError error) const {
    return std::find(errors.begin(), errors.end(), error) != errors.end();
  }
};

inline bool isHttpsEndpoint(std::string_view endpoint) {
  return parseSecureEndpoint(endpoint, "https://", 443).has_value();
}

inline bool isSecureWebSocketEndpoint(std::string_view endpoint) {
  return parseSecureEndpoint(endpoint, "wss://", 443).has_value();
}

inline ConfigValidationResult validateDeviceConfig(const DeviceConfig& config) {
  ConfigValidationResult result;
  if (config.schema_version != kDeviceConfigSchemaVersion) {
    result.errors.push_back(ConfigError::kUnsupportedSchema);
  }
  if (config.network.ssid.empty()) {
    result.errors.push_back(ConfigError::kMissingWifiSsid);
  }
  if (config.network.password.empty()) {
    result.errors.push_back(ConfigError::kMissingWifiPassword);
  }
  if (config.stt.api_key.empty()) {
    result.errors.push_back(ConfigError::kMissingSttCredential);
  }
  if (config.reply.credential.empty()) {
    result.errors.push_back(ConfigError::kMissingReplyCredential);
  }
  if (config.tts.credential.empty()) {
    result.errors.push_back(ConfigError::kMissingTtsCredential);
  }
  if (config.stt.adapter != "doubao-asr-v1") {
    result.errors.push_back(ConfigError::kUnsupportedSttAdapter);
  }
  if (config.reply.adapter != "openai-compatible-v1") {
    result.errors.push_back(ConfigError::kUnsupportedReplyAdapter);
  }
  if (config.tts.adapter != "doubao-tts-v1") {
    result.errors.push_back(ConfigError::kUnsupportedTtsAdapter);
  }
  if (config.stt.resource_id.empty()) {
    result.errors.push_back(ConfigError::kInvalidSttConfig);
  }
  if (config.reply.model.empty()) {
    result.errors.push_back(ConfigError::kInvalidReplyConfig);
  }
  if (config.tts.resource_id.empty() || config.tts.speaker.empty()) {
    result.errors.push_back(ConfigError::kInvalidTtsConfig);
  }
  if (!isSecureWebSocketEndpoint(config.stt.endpoint)) {
    result.errors.push_back(ConfigError::kInvalidSttEndpoint);
  }
  if (!isHttpsEndpoint(config.reply.endpoint)) {
    result.errors.push_back(ConfigError::kInvalidReplyEndpoint);
  }
  if (!isHttpsEndpoint(config.tts.endpoint)) {
    result.errors.push_back(ConfigError::kInvalidTtsEndpoint);
  }
  if (config.qh_sync.enabled &&
      (!isHttpsEndpoint(config.qh_sync.endpoint) ||
       config.qh_sync.device_id.empty() || config.qh_sync.credential.empty())) {
    result.errors.push_back(ConfigError::kInvalidQhSync);
  }
  if (config.assistant.max_reply_chars == 0 ||
      config.assistant.max_reply_chars > 1000) {
    result.errors.push_back(ConfigError::kInvalidReplyLimit);
  }
  if (config.preferences.volume_percent > 100) {
    result.errors.push_back(ConfigError::kInvalidVolume);
  }
  return result;
}

inline std::string redactedSummary(const DeviceConfig& config) {
  std::string summary = "configured=true ssid=";
  summary += config.network.ssid;
  summary += " stt=";
  summary += config.stt.adapter;
  summary += " reply=";
  summary += config.reply.adapter;
  summary += "/";
  summary += config.reply.model;
  summary += " tts=";
  summary += config.tts.adapter;
  summary += " qh_sync=";
  summary += config.qh_sync.enabled ? "enabled" : "disabled";
  return summary;
}

}  // namespace qh_voice
