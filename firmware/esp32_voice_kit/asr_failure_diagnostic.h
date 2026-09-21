#pragma once

#include <cstdint>
#include <string_view>

namespace qh_voice {

enum class AsrFailureCategory {
  kNone,
  kAuth,
  kHandshake,
  kNetwork,
  kProvider,
  kProtocol,
};

struct AsrFailureDiagnostic {
  AsrFailureCategory category = AsrFailureCategory::kNone;
  int http_status = 0;
  uint32_t provider_code = 0;
};

inline AsrFailureDiagnostic classifyAsrDisconnect(std::string_view reason) {
  constexpr std::string_view marker = "HTTP ";
  const std::size_t offset = reason.find(marker);
  if (offset == std::string_view::npos) {
    return {AsrFailureCategory::kNetwork, 0, 0};
  }

  int status = 0;
  std::size_t cursor = offset + marker.size();
  while (cursor < reason.size() && reason[cursor] >= '0' &&
         reason[cursor] <= '9') {
    status = status * 10 + (reason[cursor] - '0');
    ++cursor;
  }
  if (status == 401 || status == 403) {
    return {AsrFailureCategory::kAuth, status, 0};
  }
  return {status > 0 ? AsrFailureCategory::kHandshake
                     : AsrFailureCategory::kNetwork,
          status, 0};
}

inline const char* asrFailureCategoryName(AsrFailureCategory category) {
  switch (category) {
    case AsrFailureCategory::kNone:
      return "none";
    case AsrFailureCategory::kAuth:
      return "auth";
    case AsrFailureCategory::kHandshake:
      return "handshake";
    case AsrFailureCategory::kNetwork:
      return "network";
    case AsrFailureCategory::kProvider:
      return "provider";
    case AsrFailureCategory::kProtocol:
      return "protocol";
  }
  return "protocol";
}

}  // namespace qh_voice
