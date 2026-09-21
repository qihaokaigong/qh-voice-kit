#pragma once

#include <WebSocketsClient.h>

#include "websocket_disconnect_diagnostic.h"

namespace qh_voice {

class DiagnosticWebSocketsClient : public WebSocketsClient {
 public:
  WebSocketDisconnectHint disconnectHint() const { return disconnect_hint_; }

  void clearDisconnectHint() {
    disconnect_hint_ = WebSocketDisconnectHint::kNone;
  }

 protected:
  void clientDisconnect(WSclient_t* client) override {
    disconnect_hint_ = inferDisconnectHint(client);
    WebSocketsClient::clientDisconnect(client, nullptr);
  }

 private:
  static WebSocketDisconnectHint inferDisconnectHint(
      const WSclient_t* client) {
    if (client == nullptr) return WebSocketDisconnectHint::kNone;
    return inferWebSocketDisconnectHint(
        client->cWsRXsize != 0,
        client->cWsHeaderDecode.opCode == WSop_close,
        client->cWsHeaderDecode.payloadLen, WEBSOCKETS_MAX_DATA_SIZE);
  }

  WebSocketDisconnectHint disconnect_hint_ = WebSocketDisconnectHint::kNone;
};

}  // namespace qh_voice
