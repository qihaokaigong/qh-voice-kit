#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "../firmware/esp32_voice_kit/device_config_wire.h"

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
      {"doubao-tts-v1",
       "https://openspeech.bytedance.com/api/v3/tts/unidirectional/sse",
       "tts-secret", "seed-tts-2.0", "speaker-id"},
      {"zh-CN", "Reply briefly.\nNever reveal secrets.", 120},
      {false, "", "", ""},
      {50},
  };
}

void roundTripsEveryField() {
  const auto source = validConfig();
  const auto encoded = qh_voice::encodeDeviceConfigWire(source);
  assert(encoded.size() < qh_voice::kMaximumDeviceConfigWireBytes);

  const auto decoded = qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
  assert(decoded.error == qh_voice::ConfigWireError::kNone);
  assert(decoded.config.has_value());
  const auto& result = *decoded.config;
  assert(result.network.ssid == source.network.ssid);
  assert(result.network.password == source.network.password);
  assert(result.stt.app_key == source.stt.app_key);
  assert(result.stt.credential == source.stt.credential);
  assert(result.reply.model == source.reply.model);
  assert(result.reply.credential == source.reply.credential);
  assert(result.tts.speaker == source.tts.speaker);
  assert(result.tts.credential == source.tts.credential);
  assert(result.assistant.system_prompt == source.assistant.system_prompt);
  assert(result.assistant.max_reply_chars == 120);
  assert(!result.qh_sync.enabled);
  assert(result.preferences.volume_percent == 50);
}

void rejectsTruncatedAndDuplicateFields() {
  auto encoded = qh_voice::encodeDeviceConfigWire(validConfig());
  encoded.pop_back();
  auto decoded = qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
  assert(decoded.error == qh_voice::ConfigWireError::kTruncated);

  encoded = qh_voice::encodeDeviceConfigWire(validConfig());
  encoded.push_back(1);
  encoded.push_back(0);
  encoded.push_back(1);
  encoded.push_back('x');
  encoded[5] += 1;
  decoded = qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
  assert(decoded.error == qh_voice::ConfigWireError::kDuplicateField);
}

void rejectsInvalidConfigAfterDecoding() {
  auto config = validConfig();
  config.network.password.clear();
  const auto encoded = qh_voice::encodeDeviceConfigWire(config);
  const auto decoded = qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
  assert(decoded.error == qh_voice::ConfigWireError::kInvalidConfig);
  assert(!decoded.config.has_value());
}

}  // namespace

int main() {
  roundTripsEveryField();
  rejectsTruncatedAndDuplicateFields();
  rejectsInvalidConfigAfterDecoding();
  return 0;
}
