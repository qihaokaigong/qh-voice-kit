#include <Arduino.h>
#include <SHA2Builder.h>

#include <string>
#include <vector>

#include "config_transaction.h"
#include "device_config_wire.h"
#include "preferences_config_store.h"
#include "provisioning_command.h"

namespace {

qh_voice::PreferencesConfigStore config_store;
std::string command_line;
std::vector<uint8_t> config_payload;
std::size_t expected_payload_size = 0;
std::string expected_sha256;
bool store_ready = false;

void emitError(const char* code) {
  Serial.printf(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"blocked\","
      "\"code\":\"%s\"}\n",
      code);
}

void emitStatus() {
  const auto active = store_ready ? config_store.activeConfig() : std::nullopt;
  Serial.printf(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"ready\","
      "\"configured\":%s,\"firmware\":\"qh-voice-kit-dev\"}\n",
      active ? "true" : "false");
}

void resetReceiver() {
  config_payload.clear();
  expected_payload_size = 0;
  expected_sha256.clear();
}

void applyReceivedConfig() {
  SHA256Builder digest;
  digest.begin();
  digest.add(config_payload.data(), config_payload.size());
  digest.calculate();
  if (!digest.toString().equalsIgnoreCase(expected_sha256.c_str())) {
    emitError("checksum_mismatch");
    resetReceiver();
    return;
  }

  const auto decoded = qh_voice::decodeDeviceConfigWire(
      config_payload.data(), config_payload.size());
  if (!decoded.config) {
    emitError("invalid_config");
    resetReceiver();
    return;
  }
  const auto result = qh_voice::applyDeviceConfig(*decoded.config, config_store);
  resetReceiver();
  if (result != qh_voice::ConfigApplyResult::kApplied) {
    emitError("config_storage_failed");
    return;
  }
  Serial.println(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"configured\","
      "\"rebootRequired\":true}");
}

void handleCommand() {
  const auto command = qh_voice::parseProvisioningCommand(command_line);
  command_line.clear();
  if (command.type == qh_voice::ProvisioningCommandType::kStatus) {
    emitStatus();
    return;
  }
  if (command.type != qh_voice::ProvisioningCommandType::kConfig ||
      !store_ready) {
    emitError("invalid_command");
    return;
  }
  expected_payload_size = command.payload_size;
  expected_sha256 = command.sha256;
  config_payload.clear();
  config_payload.reserve(expected_payload_size);
  Serial.println(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"send_payload\"}");
}

void readProvisioningSerial() {
  while (Serial.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(Serial.read());
    if (expected_payload_size > 0) {
      config_payload.push_back(value);
      if (config_payload.size() == expected_payload_size) applyReceivedConfig();
      continue;
    }
    if (value == '\r') continue;
    if (value == '\n') {
      handleCommand();
      continue;
    }
    if (command_line.size() >= 160) {
      command_line.clear();
      emitError("command_too_long");
      continue;
    }
    command_line.push_back(static_cast<char>(value));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  store_ready = config_store.begin();
  if (!store_ready) {
    emitError("config_store_unavailable");
    return;
  }
  emitStatus();
}

void loop() {
  readProvisioningSerial();
  delay(1);
}
