#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/device_config.h"

namespace {

qh_voice::DeviceConfig validConfig() {
  return {
      3,
      {"studio-wifi", "wifi-secret"},
      {"doubao-seeduplex-v1", "realtime-api-key",
       "zh_female_xiaohe_jupiter_bigtts"},
      {"zh-CN", "Reply briefly.", true},
      {true, "https://device.example.com", "device-1", "qh-secret"},
      {50},
  };
}

void acceptsCompleteConfiguration() {
  const auto result = qh_voice::validateDeviceConfig(validConfig());
  assert(result.ok());
  assert(result.errors.empty());
}

void rejectsMissingRuntimeSecrets() {
  auto config = validConfig();
  config.network.password.clear();
  config.realtime_voice.api_key.clear();

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(!result.ok());
  assert(result.has(qh_voice::ConfigError::kMissingWifiPassword));
  assert(result.has(qh_voice::ConfigError::kMissingRealtimeVoiceCredential));
}

void rejectsUnsupportedAdapterAndMissingVoice() {
  auto config = validConfig();
  config.realtime_voice.adapter = "custom-realtime";
  config.realtime_voice.voice.clear();

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(result.has(qh_voice::ConfigError::kUnsupportedRealtimeVoiceAdapter));
  assert(result.has(qh_voice::ConfigError::kInvalidRealtimeVoiceConfig));
}

void allowsDisabledQhSyncWithoutCredentials() {
  auto config = validConfig();
  config.qh_sync = {false, "", "", ""};
  assert(qh_voice::validateDeviceConfig(config).ok());
}

void redactedSummaryNeverContainsSecrets() {
  const auto config = validConfig();
  const std::string summary = qh_voice::redactedSummary(config);

  assert(summary.find("wifi-secret") == std::string::npos);
  assert(summary.find("realtime-api-key") == std::string::npos);
  assert(summary.find("qh-secret") == std::string::npos);
  assert(summary.find("studio-wifi") != std::string::npos);
  assert(summary.find("doubao-seeduplex-v1") != std::string::npos);
  assert(summary.find("zh_female_xiaohe_jupiter_bigtts") !=
         std::string::npos);
  assert(summary.find("configured") != std::string::npos);
}

void rejectsOldSchemaAndUnsafePreferences() {
  auto config = validConfig();
  config.schema_version = 2;
  config.preferences.volume_percent = 101;

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(result.has(qh_voice::ConfigError::kUnsupportedSchema));
  assert(result.has(qh_voice::ConfigError::kInvalidVolume));
}

}  // namespace

int main() {
  acceptsCompleteConfiguration();
  rejectsMissingRuntimeSecrets();
  rejectsUnsupportedAdapterAndMissingVoice();
  allowsDisabledQhSyncWithoutCredentials();
  redactedSummaryNeverContainsSecrets();
  rejectsOldSchemaAndUnsafePreferences();
  return 0;
}
