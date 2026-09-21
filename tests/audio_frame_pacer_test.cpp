#include <cassert>
#include <cstdint>

#include "../firmware/esp32_voice_kit/audio_frame_pacer.h"

int main() {
  qh_voice::AudioFramePacer pacer(/*period_ms=*/20);
  pacer.reset(1000);
  assert(pacer.due(1000));
  assert(!pacer.due(1001));
  assert(!pacer.due(1019));
  assert(pacer.due(1020));

  assert(pacer.due(1105));
  assert(!pacer.due(1106));
  assert(pacer.due(1125));

  pacer.reset(UINT32_MAX - 10);
  assert(pacer.due(UINT32_MAX - 10));
  assert(!pacer.due(5));
  assert(pacer.due(9));
  return 0;
}
