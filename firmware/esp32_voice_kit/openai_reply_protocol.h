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

}  // namespace qh_voice
