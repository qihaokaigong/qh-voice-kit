#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "../firmware/esp32_voice_kit/doubao_asr_protocol.h"

namespace {

void appendsBigEndian32(std::vector<uint8_t>& output, uint32_t value) {
  output.push_back(static_cast<uint8_t>(value >> 24));
  output.push_back(static_cast<uint8_t>(value >> 16));
  output.push_back(static_cast<uint8_t>(value >> 8));
  output.push_back(static_cast<uint8_t>(value));
}

void buildsUncompressedFullClientFrame() {
  const qh_voice::DoubaoAsrMetadata metadata{
      "device-001", "pcm", 16000, 16, 1, "bigmodel", "single", true};
  const auto frame = qh_voice::buildDoubaoAsrFullClientFrame(metadata);

  assert(frame.size() > 8);
  assert(frame[0] == 0x11);
  assert(frame[1] == 0x10);
  assert(frame[2] == 0x10);
  assert(frame[3] == 0x00);
  const uint32_t payload_size = qh_voice::readBigEndian32(frame.data() + 4);
  assert(payload_size == frame.size() - 8);

  const std::string json(frame.begin() + 8, frame.end());
  assert(json.find("\"uid\":\"device-001\"") != std::string::npos);
  assert(json.find("\"format\":\"pcm\"") != std::string::npos);
  assert(json.find("\"rate\":16000") != std::string::npos);
  assert(json.find("\"model_name\":\"bigmodel\"") != std::string::npos);
}

void buildsAudioAndFinalAudioFrames() {
  const uint8_t pcm[] = {0x01, 0x02, 0x03, 0x04};
  const auto regular = qh_voice::buildDoubaoAsrAudioFrame(pcm, sizeof(pcm), false);
  assert((regular == std::vector<uint8_t>{0x11, 0x20, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x04,
                                          0x01, 0x02, 0x03, 0x04}));

  const auto final = qh_voice::buildDoubaoAsrAudioFrame(nullptr, 0, true);
  assert((final == std::vector<uint8_t>{0x11, 0x22, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00}));
}

void parsesFinalServerText() {
  const std::string json =
      "{\"result\":{\"text\":\"你好，QH\"},\"audio_info\":{}}";
  std::vector<uint8_t> frame{0x11, 0x93, 0x10, 0x00};
  appendsBigEndian32(frame, static_cast<uint32_t>(-2));
  appendsBigEndian32(frame, static_cast<uint32_t>(json.size()));
  frame.insert(frame.end(), json.begin(), json.end());

  const auto result = qh_voice::parseDoubaoAsrServerFrame(frame.data(), frame.size());
  assert(result.status == qh_voice::DoubaoAsrStatus::kFinalText);
  assert(result.final_frame);
  assert(result.sequence == -2);
  assert(result.text == "你好，QH");
}

void parsesProviderErrorWithoutLeakingPayload() {
  const std::string message = "permission denied";
  std::vector<uint8_t> frame{0x11, 0xF0, 0x00, 0x00};
  appendsBigEndian32(frame, 45000000);
  appendsBigEndian32(frame, static_cast<uint32_t>(message.size()));
  frame.insert(frame.end(), message.begin(), message.end());

  const auto result = qh_voice::parseDoubaoAsrServerFrame(frame.data(), frame.size());
  assert(result.status == qh_voice::DoubaoAsrStatus::kProviderError);
  assert(result.provider_code == 45000000);
  assert(result.text.empty());
}

void rejectsTruncatedFrames() {
  const uint8_t frame[] = {0x11, 0x90, 0x10, 0x00, 0x00};
  const auto result = qh_voice::parseDoubaoAsrServerFrame(frame, sizeof(frame));
  assert(result.status == qh_voice::DoubaoAsrStatus::kProtocolError);
}

}  // namespace

int main() {
  buildsUncompressedFullClientFrame();
  buildsAudioAndFinalAudioFrames();
  parsesFinalServerText();
  parsesProviderErrorWithoutLeakingPayload();
  rejectsTruncatedFrames();
  return 0;
}
