#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "json_string.h"

namespace qh_voice {

struct DoubaoAsrMetadata {
  std::string user_id;
  std::string audio_format;
  uint32_t sample_rate;
  uint8_t bits_per_sample;
  uint8_t channels;
  std::string model_name;
  std::string result_type;
  bool show_utterances;
};

enum class DoubaoAsrStatus {
  kIntermediateText,
  kFinalText,
  kAcknowledged,
  kProviderError,
  kProtocolError,
};

struct DoubaoAsrResult {
  DoubaoAsrStatus status;
  bool final_frame;
  int32_t sequence;
  uint32_t provider_code;
  std::string text;
};

inline uint32_t readBigEndian32(const uint8_t* input) {
  return (static_cast<uint32_t>(input[0]) << 24) |
         (static_cast<uint32_t>(input[1]) << 16) |
         (static_cast<uint32_t>(input[2]) << 8) |
         static_cast<uint32_t>(input[3]);
}

inline void appendBigEndian32(std::vector<uint8_t>& output, uint32_t value) {
  output.push_back(static_cast<uint8_t>(value >> 24));
  output.push_back(static_cast<uint8_t>(value >> 16));
  output.push_back(static_cast<uint8_t>(value >> 8));
  output.push_back(static_cast<uint8_t>(value));
}

inline std::vector<uint8_t> frameDoubaoAsrPayload(
    uint8_t message_type, uint8_t flags, uint8_t serialization,
    const uint8_t* payload, std::size_t payload_size) {
  std::vector<uint8_t> frame;
  frame.reserve(8 + payload_size);
  frame.push_back(0x11);  // protocol v1, four-byte header
  frame.push_back(static_cast<uint8_t>((message_type << 4) | flags));
  frame.push_back(static_cast<uint8_t>(serialization << 4));
  frame.push_back(0x00);
  appendBigEndian32(frame, static_cast<uint32_t>(payload_size));
  if (payload_size > 0 && payload != nullptr) {
    frame.insert(frame.end(), payload, payload + payload_size);
  }
  return frame;
}

inline std::vector<uint8_t> buildDoubaoAsrFullClientFrame(
    const DoubaoAsrMetadata& metadata) {
  std::string json = "{\"user\":{\"uid\":\"" +
                     escapeJsonString(metadata.user_id) +
                     "\"},\"audio\":{\"format\":\"" +
                     escapeJsonString(metadata.audio_format) +
                     "\",\"rate\":" + std::to_string(metadata.sample_rate) +
                     ",\"bits\":" +
                     std::to_string(metadata.bits_per_sample) +
                     ",\"channel\":" + std::to_string(metadata.channels) +
                     "},\"request\":{\"model_name\":\"" +
                     escapeJsonString(metadata.model_name) +
                     "\",\"result_type\":\"" +
                     escapeJsonString(metadata.result_type) +
                     "\",\"show_utterances\":" +
                     (metadata.show_utterances ? "true" : "false") + "}}";
  return frameDoubaoAsrPayload(
      0x1, 0x0, 0x1, reinterpret_cast<const uint8_t*>(json.data()), json.size());
}

inline std::vector<uint8_t> buildDoubaoAsrAudioFrame(
    const uint8_t* pcm, std::size_t pcm_size, bool final_frame) {
  return frameDoubaoAsrPayload(0x2, final_frame ? 0x2 : 0x0, 0x0, pcm,
                               pcm_size);
}

inline DoubaoAsrResult asrProtocolError() {
  return {DoubaoAsrStatus::kProtocolError, false, 0, 0, ""};
}

inline DoubaoAsrResult parseDoubaoAsrServerFrame(const uint8_t* frame,
                                                  std::size_t frame_size) {
  if (frame == nullptr || frame_size < 4 || (frame[0] >> 4) != 0x1) {
    return asrProtocolError();
  }
  const std::size_t header_size = (frame[0] & 0x0F) * 4;
  if (header_size < 4 || header_size > frame_size) return asrProtocolError();

  const uint8_t message_type = frame[1] >> 4;
  const uint8_t flags = frame[1] & 0x0F;
  const uint8_t serialization = frame[2] >> 4;
  const uint8_t compression = frame[2] & 0x0F;
  if (compression != 0) return asrProtocolError();

  std::size_t cursor = header_size;
  int32_t sequence = 0;
  if ((flags & 0x1) != 0) {
    if (frame_size - cursor < 4) return asrProtocolError();
    sequence = static_cast<int32_t>(readBigEndian32(frame + cursor));
    cursor += 4;
  }
  const bool final_frame = (flags & 0x2) != 0;

  if (message_type == 0xF) {
    if (frame_size - cursor < 8) return asrProtocolError();
    const uint32_t code = readBigEndian32(frame + cursor);
    const uint32_t payload_size = readBigEndian32(frame + cursor + 4);
    if (payload_size > frame_size - cursor - 8) return asrProtocolError();
    return {DoubaoAsrStatus::kProviderError, final_frame, sequence, code, ""};
  }

  if (message_type == 0xB) {
    return {DoubaoAsrStatus::kAcknowledged, final_frame, sequence, 0, ""};
  }
  if (message_type != 0x9 || serialization != 0x1 ||
      frame_size - cursor < 4) {
    return asrProtocolError();
  }

  const uint32_t payload_size = readBigEndian32(frame + cursor);
  cursor += 4;
  if (payload_size > frame_size - cursor) return asrProtocolError();
  const std::string_view json(reinterpret_cast<const char*>(frame + cursor),
                              payload_size);
  const std::size_t result_offset = json.find("\"result\"");
  if (result_offset == std::string_view::npos) return asrProtocolError();
  const auto text = findJsonString(json, "text", result_offset);
  if (!text) return asrProtocolError();
  return {final_frame ? DoubaoAsrStatus::kFinalText
                      : DoubaoAsrStatus::kIntermediateText,
          final_frame, sequence, 0, *text};
}

}  // namespace qh_voice
