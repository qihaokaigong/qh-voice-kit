#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace qh_voice {

constexpr uint16_t kDeviceConfigSchemaVersion = 1;

struct NetworkConfig {
  std::string ssid;
  std::string password;
};

struct SttConfig {
  std::string adapter;
  std::string endpoint;
  std::string app_key;
  std::string credential;
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

inline bool isSecureEndpoint(std::string_view endpoint,
                             std::string_view prefix) {
  if (endpoint.size() <= prefix.size() || endpoint.substr(0, prefix.size()) != prefix) {
    return false;
  }
  const std::size_t authority_end = endpoint.find_first_of("/?#", prefix.size());
  const std::string_view authority = endpoint.substr(
      prefix.size(), authority_end == std::string_view::npos
                         ? std::string_view::npos
                         : authority_end - prefix.size());
  if (authority.empty() || authority.find('@') != std::string_view::npos) {
    return false;
  }
  return endpoint.find_first_of("\r\n\t ") == std::string_view::npos;
}

inline bool isHttpsEndpoint(std::string_view endpoint) {
  return isSecureEndpoint(endpoint, "https://");
}

inline bool isSecureWebSocketEndpoint(std::string_view endpoint) {
  return isSecureEndpoint(endpoint, "wss://");
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
  if (config.stt.credential.empty()) {
    result.errors.push_back(ConfigError::kMissingSttCredential);
  }
  if (config.reply.credential.empty()) {
    result.errors.push_back(ConfigError::kMissingReplyCredential);
  }
  if (config.tts.credential.empty()) {
    result.errors.push_back(ConfigError::kMissingTtsCredential);
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
