#include <cassert>
#include <cstdint>
#include <vector>

#include "../firmware/esp32_voice_kit/device_config_wire.h"

namespace {

qh_voice::DeviceConfig validConfig() {
  return {
      3,
      {"studio-wifi", "wifi-secret"},
      {"doubao-seeduplex-v1", "realtime-secret",
       "zh_female_xiaohe_jupiter_bigtts"},
      {"zh-CN", "Reply briefly.\nNever reveal secrets.", true},
      {false, "", "", ""},
      {50},
  };
}

void roundTripsEveryField() {
  const auto source = validConfig();
  const auto encoded = qh_voice::encodeDeviceConfigWire(source);
  assert(encoded.size() < qh_voice::kMaximumDeviceConfigWireBytes);
  assert(encoded[4] == 3);

  const auto decoded =
      qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
  assert(decoded.error == qh_voice::ConfigWireError::kNone);
  assert(decoded.config.has_value());
  const auto& result = *decoded.config;
  assert(result.schema_version == 3);
  assert(result.network.ssid == source.network.ssid);
  assert(result.network.password == source.network.password);
  assert(result.realtime_voice.adapter == "doubao-seeduplex-v1");
  assert(result.realtime_voice.api_key == "realtime-secret");
  assert(result.realtime_voice.voice ==
         "zh_female_xiaohe_jupiter_bigtts");
  assert(result.assistant.system_prompt == source.assistant.system_prompt);
  assert(result.assistant.show_reply_text);
  assert(!result.qh_sync.enabled);
  assert(result.preferences.volume_percent == 50);
  assert(encoded[5] == 13);
}

void rejectsVersionTwoPayloads() {
  auto encoded = qh_voice::encodeDeviceConfigWire(validConfig());
  encoded[4] = 2;
  const auto decoded =
      qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
  assert(decoded.error == qh_voice::ConfigWireError::kUnsupportedVersion);
}

void rejectsTruncatedAndDuplicateFields() {
  auto encoded = qh_voice::encodeDeviceConfigWire(validConfig());
  encoded.pop_back();
  auto decoded =
      qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
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
  config.realtime_voice.api_key.clear();
  const auto encoded = qh_voice::encodeDeviceConfigWire(config);
  const auto decoded =
      qh_voice::decodeDeviceConfigWire(encoded.data(), encoded.size());
  assert(decoded.error == qh_voice::ConfigWireError::kInvalidConfig);
  assert(!decoded.config.has_value());
}

}  // namespace

int main() {
  roundTripsEveryField();
  rejectsVersionTwoPayloads();
  rejectsTruncatedAndDuplicateFields();
  rejectsInvalidConfigAfterDecoding();
  return 0;
}
