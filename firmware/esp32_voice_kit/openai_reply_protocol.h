#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "json_string.h"

namespace qh_voice {

struct OpenAiReplyRequest {
  std::string model;
  std::string system_prompt;
  std::string user_text;
  uint16_t max_tokens;
};

struct OpenAiReply {
  std::string text;
};

enum class ReplyError {
  kNone,
  kAuth,
  kRateLimited,
  kProvider,
  kProtocol,
};

inline std::string buildOpenAiReplyBody(const OpenAiReplyRequest& request) {
  std::string body = "{\"model\":\"" + escapeJsonString(request.model) +
                     "\",\"messages\":[{\"role\":\"system\",\"content\":\"" +
                     escapeJsonString(request.system_prompt) +
                     "\"},{\"role\":\"user\",\"content\":\"" +
                     escapeJsonString(request.user_text) +
                     "\"}],\"stream\":false,\"max_tokens\":";
  body += std::to_string(request.max_tokens);
  body += "}";
  return body;
}

inline bool isBlank(std::string_view value) {
  for (const unsigned char character : value) {
    if (!std::isspace(character)) return false;
  }
  return true;
}

inline std::optional<OpenAiReply> parseOpenAiReply(std::string_view json) {
  const std::size_t choices = json.find("\"choices\"");
  if (choices == std::string_view::npos) return std::nullopt;
  const std::size_t message = json.find("\"message\"", choices);
  if (message == std::string_view::npos) return std::nullopt;
  auto content = findJsonString(json, "content", message);
  if (!content || content->empty() || isBlank(*content)) return std::nullopt;
  return OpenAiReply{*content};
}

inline ReplyError mapReplyHttpStatus(int status) {
  if (status >= 200 && status < 300) return ReplyError::kNone;
  if (status == 401 || status == 403) return ReplyError::kAuth;
  if (status == 429) return ReplyError::kRateLimited;
  if (status >= 500 && status < 600) return ReplyError::kProvider;
  return ReplyError::kProtocol;
}

inline std::string truncateUtf8(std::string_view text,
                                std::size_t maximum_characters) {
  std::size_t cursor = 0;
  std::size_t characters = 0;
  while (cursor < text.size() && characters < maximum_characters) {
    const unsigned char lead = static_cast<unsigned char>(text[cursor]);
    std::size_t width = 1;
    if ((lead & 0xE0) == 0xC0) {
      width = 2;
    } else if ((lead & 0xF0) == 0xE0) {
      width = 3;
    } else if ((lead & 0xF8) == 0xF0) {
      width = 4;
    }
    if (width > text.size() - cursor) break;
    bool valid = true;
    for (std::size_t index = 1; index < width; ++index) {
      const unsigned char continuation =
          static_cast<unsigned char>(text[cursor + index]);
      if ((continuation & 0xC0) != 0x80) valid = false;
    }
    cursor += valid ? width : 1;
    ++characters;
  }
  return std::string(text.substr(0, cursor));
}

}  // namespace qh_voice
