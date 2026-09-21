#pragma once

#include <string>
#include <string_view>
#include <cstddef>

namespace qh_voice {

enum class WebSocketDisconnectHint {
  kNone,
  kServerClose,
  kFrameTooLarge,
  kFrameReceiveFailed,
};

inline WebSocketDisconnectHint inferWebSocketDisconnectHint(
    bool frame_active, bool close_frame, std::size_t payload_size,
    std::size_t maximum_payload_size) {
  if (!frame_active) return WebSocketDisconnectHint::kNone;
  if (close_frame) return WebSocketDisconnectHint::kServerClose;
  if (payload_size > maximum_payload_size) {
    return WebSocketDisconnectHint::kFrameTooLarge;
  }
  return WebSocketDisconnectHint::kFrameReceiveFailed;
}

inline bool startsWith(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

inline std::string safeWebSocketDisconnectCode(
    WebSocketDisconnectHint hint, std::string_view library_reason) {
  switch (hint) {
    case WebSocketDisconnectHint::kServerClose:
      return "server_close";
    case WebSocketDisconnectHint::kFrameTooLarge:
      return "frame_too_large";
    case WebSocketDisconnectHint::kFrameReceiveFailed:
      return "frame_receive_failed";
    case WebSocketDisconnectHint::kNone:
      break;
  }

  if (library_reason == "Connection lost") return "connection_lost";
  if (library_reason == "TCP connection cleanup") return "tcp_cleanup";
  if (library_reason == "Header response timeout") {
    return "handshake_timeout";
  }
  if (startsWith(library_reason, "HTTP ") ||
      startsWith(library_reason, "WebSocket handshake failed - HTTP ")) {
    return "handshake_http";
  }
  if (library_reason == "WebSocket handshake failed") {
    return "handshake_failed";
  }
  return "unknown_disconnect";
}

}  // namespace qh_voice
