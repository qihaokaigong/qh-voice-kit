#include <cassert>

#include "../firmware/esp32_voice_kit/config_transaction.h"

namespace {

qh_voice::DeviceConfig validConfig() {
  return {
      2,
      {"studio-wifi", "wifi-secret"},
      {"doubao-asr-v1",
       "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel", "asr-api-key",
       "volc.bigasr.sauc.duration"},
      {"openai-compatible-v1", "https://api.example.com/v1", "reply-model",
       "reply-secret"},
      {"doubao-tts-v1", "https://openspeech.bytedance.com", "tts-secret",
       "seed-tts-2.0", "speaker-id"},
      {"zh-CN", "Reply briefly.", 120},
      {false, "", "", ""},
      {50},
  };
}

class FakeStore final : public qh_voice::ConfigStore {
 public:
  bool write_result = true;
  bool match_result = true;
  bool activate_result = true;
  int writes = 0;
  int matches = 0;
  int activations = 0;
  int discards = 0;

  bool writePending(const qh_voice::DeviceConfig&) override {
    ++writes;
    return write_result;
  }

  bool pendingMatches(const qh_voice::DeviceConfig&) override {
    ++matches;
    return match_result;
  }

  bool activatePending() override {
    ++activations;
    return activate_result;
  }

  void discardPending() override { ++discards; }
};

void commitsOnlyAfterWriteAndReadback() {
  FakeStore store;
  const auto result = qh_voice::applyDeviceConfig(validConfig(), store);

  assert(result == qh_voice::ConfigApplyResult::kApplied);
  assert(store.writes == 1);
  assert(store.matches == 1);
  assert(store.activations == 1);
  assert(store.discards == 0);
}

void invalidConfigNeverTouchesStorage() {
  FakeStore store;
  auto config = validConfig();
  config.network.password.clear();

  const auto result = qh_voice::applyDeviceConfig(config, store);

  assert(result == qh_voice::ConfigApplyResult::kInvalidConfig);
  assert(store.writes == 0);
  assert(store.matches == 0);
  assert(store.activations == 0);
  assert(store.discards == 0);
}

void failedWriteDiscardsPendingState() {
  FakeStore store;
  store.write_result = false;

  const auto result = qh_voice::applyDeviceConfig(validConfig(), store);

  assert(result == qh_voice::ConfigApplyResult::kWriteFailed);
  assert(store.discards == 1);
  assert(store.matches == 0);
  assert(store.activations == 0);
}

void mismatchedReadbackNeverActivates() {
  FakeStore store;
  store.match_result = false;

  const auto result = qh_voice::applyDeviceConfig(validConfig(), store);

  assert(result == qh_voice::ConfigApplyResult::kReadbackMismatch);
  assert(store.discards == 1);
  assert(store.activations == 0);
}

void failedActivationDiscardsPendingState() {
  FakeStore store;
  store.activate_result = false;

  const auto result = qh_voice::applyDeviceConfig(validConfig(), store);

  assert(result == qh_voice::ConfigApplyResult::kActivationFailed);
  assert(store.discards == 1);
}

}  // namespace

int main() {
  commitsOnlyAfterWriteAndReadback();
  invalidConfigNeverTouchesStorage();
  failedWriteDiscardsPendingState();
  mismatchedReadbackNeverActivates();
  failedActivationDiscardsPendingState();
  return 0;
}
