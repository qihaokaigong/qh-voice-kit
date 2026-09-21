#pragma once

#include <cstdint>

namespace qh_voice {

class AudioFramePacer {
 public:
  explicit AudioFramePacer(uint32_t period_ms) : period_ms_(period_ms) {}

  void reset(uint32_t now_ms) { last_sent_ms_ = now_ms - period_ms_; }

  bool due(uint32_t now_ms) {
    if (static_cast<uint32_t>(now_ms - last_sent_ms_) < period_ms_) {
      return false;
    }
    last_sent_ms_ = now_ms;
    return true;
  }

 private:
  uint32_t period_ms_;
  uint32_t last_sent_ms_ = 0;
};

}  // namespace qh_voice
