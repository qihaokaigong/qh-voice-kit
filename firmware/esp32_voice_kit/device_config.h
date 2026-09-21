#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "secure_endpoint.h"

namespace qh_voice {

constexpr uint16_t kDeviceConfigSchemaVersion = 3;

struct NetworkConfig {
  std::string ssid;
  std::string password;
};

struct RealtimeVoiceConfig {
  std::string adapter;
  std::string api_key;
  std::string voice;
};

struct AssistantConfig {
  std::string language;
  std::string system_prompt;
  bool show_reply_text;
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
  RealtimeVoiceConfig realtime_voice;
  AssistantConfig assistant;
  QhSyncConfig qh_sync;
  PreferencesConfig preferences;
};

enum class ConfigError {
  kUnsupportedSchema,
  kMissingWifiSsid,
  kMissingWifiPassword,
  kMissingRealtimeVoiceCredential,
  kUnsupportedRealtimeVoiceAdapter,
  kInvalidRealtimeVoiceConfig,
  kInvalidQhSync,
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
  if (config.realtime_voice.api_key.empty()) {
    result.errors.push_back(ConfigError::kMissingRealtimeVoiceCredential);
  }
  if (config.realtime_voice.adapter != "doubao-seeduplex-v1") {
    result.errors.push_back(ConfigError::kUnsupportedRealtimeVoiceAdapter);
  }
  if (config.realtime_voice.voice.empty()) {
    result.errors.push_back(ConfigError::kInvalidRealtimeVoiceConfig);
  }
  if (config.qh_sync.enabled &&
      (!isHttpsEndpoint(config.qh_sync.endpoint) ||
       config.qh_sync.device_id.empty() || config.qh_sync.credential.empty())) {
    result.errors.push_back(ConfigError::kInvalidQhSync);
  }
  if (config.preferences.volume_percent > 100) {
    result.errors.push_back(ConfigError::kInvalidVolume);
  }
  return result;
}

inline std::string redactedSummary(const DeviceConfig& config) {
  std::string summary = "configured=true ssid=";
  summary += config.network.ssid;
  summary += " realtime=";
  summary += config.realtime_voice.adapter;
  summary += "/";
  summary += config.realtime_voice.voice;
  summary += " qh_sync=";
  summary += config.qh_sync.enabled ? "enabled" : "disabled";
  return summary;
}

}  // namespace qh_voice
