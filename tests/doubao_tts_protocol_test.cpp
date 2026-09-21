#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/doubao_tts_protocol.h"

using qh_voice::DoubaoTtsRequest;
using qh_voice::TtsRequestError;

int main() {
  const DoubaoTtsRequest request{
      "device-001",
      "你好，\"QH\"。",
      "zh_female_vv_uranus_bigtts",
      "mp3",
      24000,
      0,
      0,
  };

  const auto validation = qh_voice::validateDoubaoTtsRequest(request);
  assert(validation == TtsRequestError::kNone);

  const std::string body = qh_voice::buildDoubaoTtsBody(request);
  assert(body.find("\"user\":{\"uid\":\"device-001\"}") !=
         std::string::npos);
  assert(body.find("\"text\":\"你好，\\\"QH\\\"。\"") !=
         std::string::npos);
  assert(body.find("\"speaker\":\"zh_female_vv_uranus_bigtts\"") !=
         std::string::npos);
  assert(body.find("\"sample_rate\":24000") != std::string::npos);
  assert(body.find("\"audio_params\":{\"format\":\"mp3\"") !=
         std::string::npos);
  assert(body.find("api-key-must-never-enter-the-body") == std::string::npos);

  auto invalid = request;
  invalid.text = "  \n";
  assert(qh_voice::validateDoubaoTtsRequest(invalid) ==
         TtsRequestError::kMissingText);

  invalid = request;
  invalid.audio_format = "wav";
  assert(qh_voice::validateDoubaoTtsRequest(invalid) ==
         TtsRequestError::kUnsupportedFormat);

  invalid = request;
  invalid.sample_rate = 12345;
  assert(qh_voice::validateDoubaoTtsRequest(invalid) ==
         TtsRequestError::kUnsupportedSampleRate);

  invalid = request;
  invalid.speech_rate = 101;
  assert(qh_voice::validateDoubaoTtsRequest(invalid) ==
         TtsRequestError::kInvalidSpeechRate);

  invalid = request;
  invalid.loudness_rate = -51;
  assert(qh_voice::validateDoubaoTtsRequest(invalid) ==
         TtsRequestError::kInvalidLoudnessRate);

  return 0;
}
