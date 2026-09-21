#pragma once

#include <cstddef>
#include <cstdint>

namespace qh_voice {

inline std::size_t convertI2s32ToPcm16Le(const int32_t* input,
                                         std::size_t sample_count,
                                         uint8_t* output,
                                         std::size_t output_capacity) {
  if (input == nullptr || output == nullptr || sample_count == 0 ||
      sample_count > output_capacity / 2) {
    return 0;
  }
  for (std::size_t index = 0; index < sample_count; ++index) {
    const int16_t sample = static_cast<int16_t>(input[index] >> 16);
    output[index * 2] = static_cast<uint8_t>(sample & 0xff);
    output[index * 2 + 1] =
        static_cast<uint8_t>((static_cast<uint16_t>(sample) >> 8) & 0xff);
  }
  return sample_count * 2;
}

inline void scalePcm16Le(uint8_t* pcm, std::size_t size,
                         uint8_t volume_percent) {
  if (pcm == nullptr) return;
  if (volume_percent > 100) volume_percent = 100;
  const std::size_t sample_count = size / 2;
  for (std::size_t index = 0; index < sample_count; ++index) {
    const uint16_t encoded =
        static_cast<uint16_t>(pcm[index * 2]) |
        (static_cast<uint16_t>(pcm[index * 2 + 1]) << 8);
    const int16_t sample = static_cast<int16_t>(encoded);
    const int16_t scaled = static_cast<int16_t>(
        (static_cast<int32_t>(sample) * volume_percent) / 100);
    pcm[index * 2] = static_cast<uint8_t>(scaled & 0xff);
    pcm[index * 2 + 1] =
        static_cast<uint8_t>((static_cast<uint16_t>(scaled) >> 8) & 0xff);
  }
}

}  // namespace qh_voice
