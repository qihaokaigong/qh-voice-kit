#include <array>
#include <cassert>
#include <cstdint>

#include "../firmware/esp32_voice_kit/pcm_audio.h"

namespace {

void convertsLeftJustifiedI2sSamplesToPcm16() {
  const std::array<int32_t, 4> input = {
      static_cast<int32_t>(0x7fff0000), static_cast<int32_t>(0x80000000),
      static_cast<int32_t>(0x00010000), static_cast<int32_t>(0xffff0000)};
  std::array<uint8_t, 8> output{};

  const auto bytes = qh_voice::convertI2s32ToPcm16Le(
      input.data(), input.size(), output.data(), output.size());

  assert(bytes == output.size());
  assert((output == std::array<uint8_t, 8>{
                        0xff, 0x7f, 0x00, 0x80, 0x01, 0x00, 0xff, 0xff}));
}

void rejectsSmallOutputBuffers() {
  const int32_t input[] = {0};
  uint8_t output[1] = {};
  assert(qh_voice::convertI2s32ToPcm16Le(input, 1, output, 1) == 0);
}

void scalesLittleEndianPcmWithoutOverflow() {
  std::array<uint8_t, 6> pcm = {0xff, 0x7f, 0x00, 0x80, 0x10, 0x00};
  qh_voice::scalePcm16Le(pcm.data(), pcm.size(), 50);
  assert((pcm == std::array<uint8_t, 6>{0xff, 0x3f, 0x00, 0xc0, 0x08, 0x00}));
}

}  // namespace

int main() {
  convertsLeftJustifiedI2sSamplesToPcm16();
  rejectsSmallOutputBuffers();
  scalesLittleEndianPcmWithoutOverflow();
  return 0;
}
