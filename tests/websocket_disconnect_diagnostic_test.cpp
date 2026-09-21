#include <cassert>
#include <string_view>

#include "../firmware/esp32_voice_kit/websocket_disconnect_diagnostic.h"

namespace {

void classifiesLibraryAndFrameDisconnectsWithoutReturningRawReasons() {
  using qh_voice::WebSocketDisconnectHint;

  assert(qh_voice::safeWebSocketDisconnectCode(
             WebSocketDisconnectHint::kFrameTooLarge, {}) ==
         "frame_too_large");
  assert(qh_voice::safeWebSocketDisconnectCode(
             WebSocketDisconnectHint::kServerClose, {}) == "server_close");
  assert(qh_voice::safeWebSocketDisconnectCode(
             WebSocketDisconnectHint::kNone, "Connection lost") ==
         "connection_lost");
  assert(qh_voice::safeWebSocketDisconnectCode(
             WebSocketDisconnectHint::kNone, "Header response timeout") ==
         "handshake_timeout");
  assert(qh_voice::safeWebSocketDisconnectCode(
             WebSocketDisconnectHint::kNone,
             "WebSocket handshake failed - HTTP 403") ==
         "handshake_http");
  assert(qh_voice::safeWebSocketDisconnectCode(
             WebSocketDisconnectHint::kNone,
             "raw provider reason containing user data") ==
         "unknown_disconnect");
}

void infersDisconnectHintsFromSafeFrameMetadata() {
  using qh_voice::WebSocketDisconnectHint;

  assert(qh_voice::inferWebSocketDisconnectHint(false, false, 0, 15360) ==
         WebSocketDisconnectHint::kNone);
  assert(qh_voice::inferWebSocketDisconnectHint(true, true, 12, 15360) ==
         WebSocketDisconnectHint::kServerClose);
  assert(qh_voice::inferWebSocketDisconnectHint(true, false, 15361, 15360) ==
         WebSocketDisconnectHint::kFrameTooLarge);
  assert(qh_voice::inferWebSocketDisconnectHint(true, false, 800, 15360) ==
         WebSocketDisconnectHint::kFrameReceiveFailed);
}

}  // namespace

int main() {
  classifiesLibraryAndFrameDisconnectsWithoutReturningRawReasons();
  infersDisconnectHintsFromSafeFrameMetadata();
  return 0;
}
