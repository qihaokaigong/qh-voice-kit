#pragma once

#include "device_config.h"

namespace qh_voice {

class ConfigStore {
 public:
  virtual ~ConfigStore() = default;
  virtual bool writePending(const DeviceConfig& config) = 0;
  virtual bool pendingMatches(const DeviceConfig& config) = 0;
  virtual bool activatePending() = 0;
  virtual void discardPending() = 0;
};

enum class ConfigApplyResult {
  kApplied,
  kInvalidConfig,
  kWriteFailed,
  kReadbackMismatch,
  kActivationFailed,
};

inline ConfigApplyResult applyDeviceConfig(const DeviceConfig& config,
                                           ConfigStore& store) {
  if (!validateDeviceConfig(config).ok()) {
    return ConfigApplyResult::kInvalidConfig;
  }
  if (!store.writePending(config)) {
    store.discardPending();
    return ConfigApplyResult::kWriteFailed;
  }
  if (!store.pendingMatches(config)) {
    store.discardPending();
    return ConfigApplyResult::kReadbackMismatch;
  }
  if (!store.activatePending()) {
    store.discardPending();
    return ConfigApplyResult::kActivationFailed;
  }
  return ConfigApplyResult::kApplied;
}

}  // namespace qh_voice
