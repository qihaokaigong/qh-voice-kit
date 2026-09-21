#pragma once

#include <Preferences.h>

#include <optional>
#include <vector>

#include "config_transaction.h"
#include "device_config_wire.h"

namespace qh_voice {

class PreferencesConfigStore final : public ConfigStore {
 public:
  bool begin() {
    if (!preferences_.begin("qh-voice", false)) return false;
    const uint8_t active = preferences_.getUChar("active", 0);
    active_slot_ = active <= 1 ? active : 0;
    pending_slot_ = active_slot_ == 0 ? 1 : 0;
    return true;
  }

  bool writePending(const DeviceConfig& config) override {
    pending_ = encodeDeviceConfigWire(config);
    return preferences_.putBytes(slotKey(pending_slot_), pending_.data(),
                                 pending_.size()) == pending_.size();
  }

  bool pendingMatches(const DeviceConfig&) override {
    const std::size_t stored_size =
        preferences_.getBytesLength(slotKey(pending_slot_));
    if (stored_size != pending_.size()) return false;
    std::vector<uint8_t> stored(stored_size);
    return preferences_.getBytes(slotKey(pending_slot_), stored.data(),
                                 stored.size()) == stored.size() &&
           stored == pending_;
  }

  bool activatePending() override {
    if (preferences_.putUChar("active", pending_slot_) != 1) return false;
    active_slot_ = pending_slot_;
    pending_slot_ = active_slot_ == 0 ? 1 : 0;
    pending_.clear();
    return true;
  }

  void discardPending() override {
    preferences_.remove(slotKey(pending_slot_));
    pending_.clear();
  }

  std::optional<DeviceConfig> activeConfig() {
    const std::size_t stored_size =
        preferences_.getBytesLength(slotKey(active_slot_));
    if (stored_size == 0 || stored_size > kMaximumDeviceConfigWireBytes) {
      return std::nullopt;
    }
    std::vector<uint8_t> stored(stored_size);
    if (preferences_.getBytes(slotKey(active_slot_), stored.data(),
                              stored.size()) != stored.size()) {
      return std::nullopt;
    }
    auto decoded = decodeDeviceConfigWire(stored.data(), stored.size());
    return decoded.config;
  }

 private:
  static const char* slotKey(uint8_t slot) {
    return slot == 0 ? "cfg0" : "cfg1";
  }

  Preferences preferences_;
  uint8_t active_slot_ = 0;
  uint8_t pending_slot_ = 1;
  std::vector<uint8_t> pending_;
};

}  // namespace qh_voice
