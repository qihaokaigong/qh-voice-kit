#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/device_config.h"

namespace {

qh_voice::DeviceConfig validConfig() {
  return {
      1,
      {"studio-wifi", "wifi-secret"},
      {"doubao-asr-v1",
       "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel", "app-key",
       "asr-secret", "volc.bigasr.sauc.duration"},
      {"openai-compatible-v1", "https://api.example.com/v1", "reply-model",
       "reply-secret"},
      {"doubao-tts-v1", "https://openspeech.bytedance.com", "tts-secret",
       "seed-tts-2.0", "speaker-id"},
      {"zh-CN", "Reply briefly.", 120},
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
  config.stt.credential.clear();
  config.reply.credential.clear();
  config.tts.credential.clear();

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(!result.ok());
  assert(result.has(qh_voice::ConfigError::kMissingWifiPassword));
  assert(result.has(qh_voice::ConfigError::kMissingSttCredential));
  assert(result.has(qh_voice::ConfigError::kMissingReplyCredential));
  assert(result.has(qh_voice::ConfigError::kMissingTtsCredential));
}

void rejectsUnsafeProviderEndpoints() {
  auto config = validConfig();
  config.stt.endpoint = "https://openspeech.bytedance.com/api/v3/sauc/bigmodel";
  config.reply.endpoint = "https://user:password@example.com/v1";
  config.tts.endpoint = "file:///tmp/audio";

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(result.has(qh_voice::ConfigError::kInvalidSttEndpoint));
  assert(result.has(qh_voice::ConfigError::kInvalidReplyEndpoint));
  assert(result.has(qh_voice::ConfigError::kInvalidTtsEndpoint));
}

void allowsDisabledQhSyncWithoutCredentials() {
  auto config = validConfig();
  config.qh_sync = {false, "", "", ""};

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(result.ok());
}

void redactedSummaryNeverContainsSecrets() {
  const auto config = validConfig();
  const std::string summary = qh_voice::redactedSummary(config);

  assert(summary.find("wifi-secret") == std::string::npos);
  assert(summary.find("asr-secret") == std::string::npos);
  assert(summary.find("reply-secret") == std::string::npos);
  assert(summary.find("tts-secret") == std::string::npos);
  assert(summary.find("qh-secret") == std::string::npos);
  assert(summary.find("studio-wifi") != std::string::npos);
  assert(summary.find("doubao-asr-v1") != std::string::npos);
  assert(summary.find("configured") != std::string::npos);
}

void rejectsUnsupportedSchemaAndUnsafePreferences() {
  auto config = validConfig();
  config.schema_version = 2;
  config.assistant.max_reply_chars = 0;
  config.preferences.volume_percent = 101;

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(result.has(qh_voice::ConfigError::kUnsupportedSchema));
  assert(result.has(qh_voice::ConfigError::kInvalidReplyLimit));
  assert(result.has(qh_voice::ConfigError::kInvalidVolume));
}

void rejectsUnsupportedAdaptersAndIncompleteProviderIds() {
  auto config = validConfig();
  config.stt.adapter = "custom-asr";
  config.stt.app_key.clear();
  config.reply.adapter = "custom-reply";
  config.reply.model.clear();
  config.tts.adapter = "custom-tts";
  config.tts.speaker.clear();

  const auto result = qh_voice::validateDeviceConfig(config);
  assert(result.has(qh_voice::ConfigError::kUnsupportedSttAdapter));
  assert(result.has(qh_voice::ConfigError::kInvalidSttConfig));
  assert(result.has(qh_voice::ConfigError::kUnsupportedReplyAdapter));
  assert(result.has(qh_voice::ConfigError::kInvalidReplyConfig));
  assert(result.has(qh_voice::ConfigError::kUnsupportedTtsAdapter));
  assert(result.has(qh_voice::ConfigError::kInvalidTtsConfig));
}

}  // namespace

int main() {
  acceptsCompleteConfiguration();
  rejectsMissingRuntimeSecrets();
  rejectsUnsafeProviderEndpoints();
  allowsDisabledQhSyncWithoutCredentials();
  redactedSummaryNeverContainsSecrets();
  rejectsUnsupportedSchemaAndUnsafePreferences();
  rejectsUnsupportedAdaptersAndIncompleteProviderIds();
  return 0;
}
