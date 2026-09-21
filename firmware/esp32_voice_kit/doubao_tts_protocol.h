#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "json_string.h"

namespace qh_voice {

struct DoubaoTtsRequest {
  std::string user_id;
  std::string text;
  std::string speaker;
  std::string audio_format;
  uint32_t sample_rate;
  int16_t speech_rate;
  int16_t loudness_rate;
};

enum class TtsRequestError {
  kNone,
  kMissingUserId,
  kMissingText,
  kMissingSpeaker,
  kUnsupportedFormat,
  kUnsupportedSampleRate,
  kInvalidSpeechRate,
  kInvalidLoudnessRate,
};

inline bool isBlankText(std::string_view value) {
  if (value.empty()) return true;
  for (const unsigned char character : value) {
    if (!std::isspace(character)) return false;
  }
  return true;
}

inline TtsRequestError validateDoubaoTtsRequest(
    const DoubaoTtsRequest& request) {
  if (isBlankText(request.user_id)) return TtsRequestError::kMissingUserId;
  if (isBlankText(request.text)) return TtsRequestError::kMissingText;
  if (isBlankText(request.speaker)) return TtsRequestError::kMissingSpeaker;

  constexpr std::array<std::string_view, 3> formats = {
      "mp3", "pcm", "ogg_opus"};
  bool supported_format = false;
  for (const auto format : formats) {
    if (request.audio_format == format) supported_format = true;
  }
  if (!supported_format) return TtsRequestError::kUnsupportedFormat;

  constexpr std::array<uint32_t, 7> sample_rates = {
      8000, 16000, 22050, 24000, 32000, 44100, 48000};
  bool supported_sample_rate = false;
  for (const auto sample_rate : sample_rates) {
    if (request.sample_rate == sample_rate) supported_sample_rate = true;
  }
  if (!supported_sample_rate) return TtsRequestError::kUnsupportedSampleRate;
  if (request.speech_rate < -50 || request.speech_rate > 100) {
    return TtsRequestError::kInvalidSpeechRate;
  }
  if (request.loudness_rate < -50 || request.loudness_rate > 100) {
    return TtsRequestError::kInvalidLoudnessRate;
  }
  return TtsRequestError::kNone;
}

inline std::string buildDoubaoTtsBody(const DoubaoTtsRequest& request) {
  std::string body = "{\"user\":{\"uid\":\"" +
                     escapeJsonString(request.user_id) +
                     "\"},\"req_params\":{\"text\":\"" +
                     escapeJsonString(request.text) + "\",\"speaker\":\"" +
                     escapeJsonString(request.speaker) +
                     "\",\"sample_rate\":" +
                     std::to_string(request.sample_rate) +
                     ",\"audio_params\":{\"format\":\"" +
                     escapeJsonString(request.audio_format) +
                     "\",\"speech_rate\":" +
                     std::to_string(request.speech_rate) +
                     ",\"loudness_rate\":" +
                     std::to_string(request.loudness_rate) + "}}}";
  return body;
}

}  // namespace qh_voice
