#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "json_string.h"
#include "provider_request.h"

namespace qh_voice {

constexpr std::string_view kRealtimeVoiceEndpoint =
    "wss://openspeech.bytedance.com/api/v3/duplex/realtime/dialogue";
constexpr std::string_view kRealtimeVoiceModel = "1.2.6.1";
constexpr uint32_t kRealtimeInputSampleRate = 16000;
constexpr uint32_t kRealtimeOutputSampleRate = 24000;
constexpr uint32_t kRealtimeFrameDurationMs = 20;
constexpr std::size_t kRealtimePcmFrameBytes = 640;

struct RealtimeSessionConfig {
  std::string session_id;
  std::string instructions;
  std::string voice;
};

inline ProviderTransportRequest buildRealtimeTransportRequest(
    std::string_view api_key) {
  return {std::string(kRealtimeVoiceEndpoint),
          {{"X-Api-Key", std::string(api_key)}}};
}

inline std::string encodeBase64(const uint8_t* input, std::size_t size) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  output.reserve(((size + 2) / 3) * 4);
  for (std::size_t cursor = 0; cursor < size; cursor += 3) {
    const uint32_t first = input[cursor];
    const uint32_t second = cursor + 1 < size ? input[cursor + 1] : 0;
    const uint32_t third = cursor + 2 < size ? input[cursor + 2] : 0;
    const uint32_t value = (first << 16) | (second << 8) | third;
    output.push_back(kAlphabet[(value >> 18) & 0x3F]);
    output.push_back(kAlphabet[(value >> 12) & 0x3F]);
    output.push_back(cursor + 1 < size ? kAlphabet[(value >> 6) & 0x3F] : '=');
    output.push_back(cursor + 2 < size ? kAlphabet[value & 0x3F] : '=');
  }
  return output;
}

inline int realtimeBase64Value(char value) {
  if (value >= 'A' && value <= 'Z') return value - 'A';
  if (value >= 'a' && value <= 'z') return value - 'a' + 26;
  if (value >= '0' && value <= '9') return value - '0' + 52;
  if (value == '+') return 62;
  if (value == '/') return 63;
  return -1;
}

inline bool decodeBase64(std::string_view encoded,
                         std::vector<uint8_t>& output) {
  output.clear();
  if (encoded.empty()) return true;
  if (encoded.size() % 4 != 0) return false;
  output.reserve(encoded.size() / 4 * 3);
  for (std::size_t cursor = 0; cursor < encoded.size(); cursor += 4) {
    const bool third_padding = encoded[cursor + 2] == '=';
    const bool fourth_padding = encoded[cursor + 3] == '=';
    if (third_padding && !fourth_padding) return false;
    if ((third_padding || fourth_padding) && cursor + 4 != encoded.size()) {
      return false;
    }
    const int first = realtimeBase64Value(encoded[cursor]);
    const int second = realtimeBase64Value(encoded[cursor + 1]);
    const int third =
        third_padding ? 0 : realtimeBase64Value(encoded[cursor + 2]);
    const int fourth =
        fourth_padding ? 0 : realtimeBase64Value(encoded[cursor + 3]);
    if (first < 0 || second < 0 || third < 0 || fourth < 0) return false;
    const uint32_t value =
        (static_cast<uint32_t>(first) << 18) |
        (static_cast<uint32_t>(second) << 12) |
        (static_cast<uint32_t>(third) << 6) |
        static_cast<uint32_t>(fourth);
    output.push_back(static_cast<uint8_t>(value >> 16));
    if (!third_padding) {
      output.push_back(static_cast<uint8_t>(value >> 8));
    }
    if (!fourth_padding) output.push_back(static_cast<uint8_t>(value));
  }
  return true;
}

inline std::string buildRealtimeSessionCreate(
    const RealtimeSessionConfig& config, std::string_view event_id) {
  std::string output = R"json({"type":"session.create","event_id":")json";
  output += escapeJsonString(event_id);
  output += R"json(","session":{"id":")json";
  output += escapeJsonString(config.session_id);
  output += R"json(","model":")json";
  output += kRealtimeVoiceModel;
  output += R"json(","instructions":")json";
  output += escapeJsonString(config.instructions);
  output +=
      R"json(","audio":{"input":{"format":{"type":"pcm","rate":16000}},"output":{"format":{"type":"pcm_s16le","rate":24000},"voice":")json";
  output += escapeJsonString(config.voice);
  output += R"json("}}}})json";
  return output;
}

inline std::string buildRealtimeAudioAppend(const uint8_t* pcm,
                                            std::size_t size) {
  std::string output =
      R"json({"type":"input_audio_buffer.append","audio":")json";
  output += encodeBase64(pcm, size);
  output += R"json("})json";
  return output;
}

inline std::string buildRealtimeControlEvent(std::string_view type,
                                             std::string_view event_id) {
  std::string output = R"json({"type":")json";
  output += escapeJsonString(type);
  output += R"json(","event_id":")json";
  output += escapeJsonString(event_id);
  output += R"json("})json";
  return output;
}

inline std::string buildRealtimeInputCommit(std::string_view event_id) {
  return buildRealtimeControlEvent("input_audio_buffer.commit", event_id);
}

inline std::string buildRealtimeMuteCommit(std::string_view event_id) {
  return buildRealtimeControlEvent("input_audio_mute.commit", event_id);
}

inline std::string buildRealtimeUnmuteCommit(std::string_view event_id) {
  return buildRealtimeControlEvent("input_audio_unmute.commit", event_id);
}

inline std::string buildRealtimeSessionClose(std::string_view event_id) {
  return buildRealtimeControlEvent("session.close", event_id);
}

enum class RealtimeEventType {
  kUnknown,
  kProtocolError,
  kSessionCreated,
  kSessionClosed,
  kInputCommitted,
  kTranscriptStarted,
  kTranscriptDelta,
  kTranscriptCompleted,
  kTranscriptFailed,
  kOutputTextDelta,
  kOutputTextDone,
  kOutputAudioStarted,
  kOutputAudioDelta,
  kOutputAudioDone,
  kResponseDone,
  kResponseCanceled,
  kError,
};

struct RealtimeEvent {
  RealtimeEventType type = RealtimeEventType::kUnknown;
  std::string session_id;
  std::string text;
  std::string audio_base64;
  std::string error_code;
};

inline RealtimeEventType realtimeEventType(std::string_view type) {
  if (type == "session.created") return RealtimeEventType::kSessionCreated;
  if (type == "session.closed") return RealtimeEventType::kSessionClosed;
  if (type == "input_audio_buffer.committed") {
    return RealtimeEventType::kInputCommitted;
  }
  if (type == "conversation.item.input_audio_transcription.started") {
    return RealtimeEventType::kTranscriptStarted;
  }
  if (type == "conversation.item.input_audio_transcription.delta") {
    return RealtimeEventType::kTranscriptDelta;
  }
  if (type == "conversation.item.input_audio_transcription.completed") {
    return RealtimeEventType::kTranscriptCompleted;
  }
  if (type == "conversation.item.input_audio_transcription.failed") {
    return RealtimeEventType::kTranscriptFailed;
  }
  if (type == "response.output_text.delta") {
    return RealtimeEventType::kOutputTextDelta;
  }
  if (type == "response.output_text.done") {
    return RealtimeEventType::kOutputTextDone;
  }
  if (type == "response.output_audio.started") {
    return RealtimeEventType::kOutputAudioStarted;
  }
  if (type == "response.output_audio.delta") {
    return RealtimeEventType::kOutputAudioDelta;
  }
  if (type == "response.output_audio.done") {
    return RealtimeEventType::kOutputAudioDone;
  }
  if (type == "response.done") return RealtimeEventType::kResponseDone;
  if (type == "response.canceled") {
    return RealtimeEventType::kResponseCanceled;
  }
  if (type == "error") return RealtimeEventType::kError;
  return RealtimeEventType::kUnknown;
}

inline RealtimeEvent parseRealtimeServerEvent(std::string_view json) {
  const auto type = findJsonString(json, "type");
  if (!type) {
    RealtimeEvent error;
    error.type = RealtimeEventType::kProtocolError;
    return error;
  }

  RealtimeEvent event;
  event.type = realtimeEventType(*type);
  switch (event.type) {
    case RealtimeEventType::kSessionCreated: {
      const std::size_t session = json.find(R"json("session")json");
      const auto id = findJsonString(
          json, "id", session == std::string_view::npos ? 0 : session);
      if (!id) {
        event.type = RealtimeEventType::kProtocolError;
        return event;
      }
      event.session_id = *id;
      break;
    }
    case RealtimeEventType::kTranscriptDelta: {
      const auto delta = findJsonString(json, "delta");
      if (delta) event.text = *delta;
      break;
    }
    case RealtimeEventType::kTranscriptCompleted: {
      auto text = findJsonString(json, "transcript");
      if (!text) text = findJsonString(json, "text");
      if (text) event.text = *text;
      break;
    }
    case RealtimeEventType::kOutputTextDelta: {
      const auto delta = findJsonString(json, "delta");
      if (delta) event.text = *delta;
      break;
    }
    case RealtimeEventType::kOutputTextDone: {
      const auto text = findJsonString(json, "text");
      if (text) event.text = *text;
      break;
    }
    case RealtimeEventType::kOutputAudioDelta: {
      const auto delta = findJsonString(json, "delta");
      if (!delta) {
        event.type = RealtimeEventType::kProtocolError;
        return event;
      }
      event.audio_base64 = *delta;
      break;
    }
    case RealtimeEventType::kError: {
      const std::size_t error = json.find(R"json("error")json");
      const auto code = findJsonString(
          json, "code", error == std::string_view::npos ? 0 : error);
      event.error_code = code.value_or("provider_error");
      break;
    }
    default:
      break;
  }
  return event;
}

}  // namespace qh_voice
