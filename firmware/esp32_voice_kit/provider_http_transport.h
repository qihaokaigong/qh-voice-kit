#pragma once

#include <HTTPClient.h>
#include <NetworkClientSecure.h>

#include <string>
#include <utility>
#include <vector>

#include "device_config.h"
#include "doubao_tts_protocol.h"
#include "doubao_tts_sse.h"
#include "esp32_tls_bundle.h"
#include "openai_reply_protocol.h"
#include "provider_request.h"
#include "qh_sync_protocol.h"

namespace qh_voice {

enum class HttpTransportError {
  kNone,
  kInvalidRequest,
  kConnection,
  kAuth,
  kRateLimited,
  kProvider,
  kProtocol,
  kTooLarge,
};

struct ReplyTransportResult {
  HttpTransportError error;
  int http_status;
  std::string text;
};

struct TtsTransportResult {
  HttpTransportError error;
  int http_status;
  int provider_code;
  std::vector<uint8_t> audio;
};

struct QhSyncTransportResult {
  HttpTransportError error;
  int http_status;
};

inline void addProviderHeaders(HTTPClient& http,
                               const ProviderTransportRequest& request) {
  for (const auto& header : request.headers) {
    http.addHeader(header.name.c_str(), header.value.c_str());
  }
}

inline HttpTransportError mapReplyTransportError(ReplyError error) {
  switch (error) {
    case ReplyError::kNone:
      return HttpTransportError::kNone;
    case ReplyError::kAuth:
      return HttpTransportError::kAuth;
    case ReplyError::kRateLimited:
      return HttpTransportError::kRateLimited;
    case ReplyError::kProvider:
      return HttpTransportError::kProvider;
    case ReplyError::kProtocol:
      return HttpTransportError::kProtocol;
  }
  return HttpTransportError::kProtocol;
}

inline ReplyTransportResult requestOpenAiReply(
    const ReplyConfig& config, const OpenAiReplyRequest& reply_request) {
  NetworkClientSecure client;
  enableRootCaBundle(client);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(20000);
  if (!http.begin(client, config.endpoint.c_str())) {
    return {HttpTransportError::kInvalidRequest, 0, ""};
  }

  const auto transport = buildOpenAiReplyTransportRequest(
      config.endpoint, config.credential);
  addProviderHeaders(http, transport);
  const int status = http.POST(buildOpenAiReplyBody(reply_request).c_str());
  if (status <= 0) {
    http.end();
    return {HttpTransportError::kConnection, status, ""};
  }
  const auto mapped = mapReplyTransportError(mapReplyHttpStatus(status));
  if (mapped != HttpTransportError::kNone) {
    http.end();
    return {mapped, status, ""};
  }
  const String response = http.getString();
  http.end();
  const auto parsed = parseOpenAiReply(
      std::string_view(response.c_str(), response.length()));
  if (!parsed) return {HttpTransportError::kProtocol, status, ""};
  return {HttpTransportError::kNone, status, parsed->text};
}

class TtsSseStream final : public Stream {
 public:
  explicit TtsSseStream(DoubaoTtsSseParser& parser) : parser_(parser) {}

  std::size_t write(uint8_t value) override {
    const char byte = static_cast<char>(value);
    return parser_.feed(std::string_view(&byte, 1)) ? 1 : 0;
  }

  std::size_t write(const uint8_t* buffer, std::size_t size) override {
    return parser_.feed(std::string_view(
               reinterpret_cast<const char*>(buffer), size))
               ? size
               : 0;
  }

  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }

 private:
  DoubaoTtsSseParser& parser_;
};

inline TtsTransportResult requestDoubaoTts(
    const TtsConfig& config, const DoubaoTtsRequest& tts_request,
    std::string request_id, std::size_t max_audio_bytes) {
  if (validateDoubaoTtsRequest(tts_request) != TtsRequestError::kNone) {
    return {HttpTransportError::kInvalidRequest, 0, 0, {}};
  }

  NetworkClientSecure client;
  enableRootCaBundle(client);
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(30000);
  if (!http.begin(client, config.endpoint.c_str())) {
    return {HttpTransportError::kInvalidRequest, 0, 0, {}};
  }
  const auto transport = buildDoubaoTtsTransportRequest(
      config.endpoint, config.credential, config.resource_id,
      std::move(request_id));
  addProviderHeaders(http, transport);
  const int status = http.POST(buildDoubaoTtsBody(tts_request).c_str());
  if (status <= 0) {
    http.end();
    return {HttpTransportError::kConnection, status, 0, {}};
  }
  if (status == 401 || status == 403) {
    http.end();
    return {HttpTransportError::kAuth, status, 0, {}};
  }
  if (status == 429) {
    http.end();
    return {HttpTransportError::kRateLimited, status, 0, {}};
  }
  if (status < 200 || status >= 300) {
    http.end();
    return {status >= 500 ? HttpTransportError::kProvider
                          : HttpTransportError::kProtocol,
            status, 0, {}};
  }

  DoubaoTtsSseParser parser(max_audio_bytes);
  TtsSseStream sink(parser);
  const int streamed = http.writeToStream(&sink);
  http.end();
  if (streamed < 0 || !parser.finish()) {
    const HttpTransportError error =
        parser.status() == DoubaoTtsStatus::kProviderError
            ? HttpTransportError::kProvider
        : parser.status() == DoubaoTtsStatus::kTooLarge
            ? HttpTransportError::kTooLarge
            : HttpTransportError::kProtocol;
    return {error, status, parser.providerCode(), {}};
  }
  return {HttpTransportError::kNone, status, parser.providerCode(),
          parser.takeAudio()};
}

inline QhSyncTransportResult postQhConversationTurn(
    const QhSyncConfig& config, const QhConversationTurn& turn) {
  if (!config.enabled) return {HttpTransportError::kNone, 0};
  NetworkClientSecure client;
  enableRootCaBundle(client);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(client, config.endpoint.c_str())) {
    return {HttpTransportError::kInvalidRequest, 0};
  }
  const auto transport =
      buildQhSyncTransportRequest(config.endpoint, config.credential);
  addProviderHeaders(http, transport);
  const int status = http.POST(buildQhConversationEventBody(turn).c_str());
  http.end();
  if (status <= 0) return {HttpTransportError::kConnection, status};
  if (status == 401 || status == 403) {
    return {HttpTransportError::kAuth, status};
  }
  if (status == 429) return {HttpTransportError::kRateLimited, status};
  if (status < 200 || status >= 300) {
    return {status >= 500 ? HttpTransportError::kProvider
                          : HttpTransportError::kProtocol,
            status};
  }
  return {HttpTransportError::kNone, status};
}

}  // namespace qh_voice
