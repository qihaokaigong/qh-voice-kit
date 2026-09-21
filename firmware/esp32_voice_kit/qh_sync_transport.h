#pragma once

#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include "device_config.h"
#include "esp32_tls_bundle.h"
#include "qh_sync_protocol.h"

namespace qh_voice {

enum class QhSyncError {
  kNone,
  kInvalidRequest,
  kConnection,
  kAuth,
  kServer,
};

struct QhSyncResult {
  QhSyncError error;
  int http_status;
};

inline QhSyncResult postQhConversationTurn(const QhSyncConfig& config,
                                           const QhConversationTurn& turn) {
  if (!config.enabled) return {QhSyncError::kNone, 0};

  NetworkClientSecure client;
  enableRootCaBundle(client);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  if (!http.begin(client, config.endpoint.c_str())) {
    return {QhSyncError::kInvalidRequest, 0};
  }

  const auto request =
      buildQhSyncTransportRequest(config.endpoint, config.credential);
  for (const auto& header : request.headers) {
    http.addHeader(header.name.c_str(), header.value.c_str());
  }
  const int status = http.POST(buildQhConversationEventBody(turn).c_str());
  http.end();
  if (status <= 0) return {QhSyncError::kConnection, status};
  if (status == 401 || status == 403) {
    return {QhSyncError::kAuth, status};
  }
  if (status < 200 || status >= 300) {
    return {QhSyncError::kServer, status};
  }
  return {QhSyncError::kNone, status};
}

}  // namespace qh_voice
