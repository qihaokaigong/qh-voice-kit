#pragma once

#include <WebSocketsClient.h>

#include <string>

#include "device_config.h"
#include "doubao_asr_protocol.h"
#include "esp32_tls_bundle.h"
#include "provider_request.h"
#include "secure_endpoint.h"

namespace qh_voice {

enum class AsrTransportStatus {
  kIdle,
  kConnecting,
  kReady,
  kAwaitingFinal,
  kComplete,
  kProviderError,
  kProtocolError,
  kDisconnected,
};

class DoubaoAsrTransport {
 public:
  bool begin(const SttConfig& config, std::string request_id,
             std::string user_id) {
    const auto endpoint =
        parseSecureEndpoint(config.endpoint, "wss://", 443);
    if (!endpoint) {
      status_ = AsrTransportStatus::kProtocolError;
      return false;
    }

    transcript_.clear();
    provider_code_ = 0;
    status_ = AsrTransportStatus::kConnecting;
    const auto request = buildDoubaoAsrTransportRequest(
        config.endpoint, config.api_key, config.resource_id,
        std::move(request_id));
    extra_headers_.clear();
    for (const auto& header : request.headers) {
      extra_headers_ += header.name + ": " + header.value + "\r\n";
    }

    metadata_ = {std::move(user_id), "pcm", 16000, 16, 1,
                 "bigmodel", "single", true};
    socket_.onEvent([this](WStype_t type, uint8_t* payload,
                           std::size_t length) {
      handleEvent(type, payload, length);
    });
    socket_.setExtraHeaders(extra_headers_.c_str());
    socket_.setReconnectInterval(0);
    socket_.beginSslWithBundle(
        endpoint->host.c_str(), endpoint->port, endpoint->path.c_str(),
        rootCaBundleData(), rootCaBundleSize(), "");
    return true;
  }

  void loop() { socket_.loop(); }

  bool sendPcm16(const uint8_t* pcm, std::size_t size) {
    if (status_ != AsrTransportStatus::kReady || pcm == nullptr || size == 0) {
      return false;
    }
    const auto frame = buildDoubaoAsrAudioFrame(pcm, size, false);
    return socket_.sendBIN(frame.data(), frame.size());
  }

  bool finish() {
    if (status_ != AsrTransportStatus::kReady) return false;
    const auto frame = buildDoubaoAsrAudioFrame(nullptr, 0, true);
    if (!socket_.sendBIN(frame.data(), frame.size())) return false;
    status_ = AsrTransportStatus::kAwaitingFinal;
    return true;
  }

  void close() {
    socket_.disconnect();
    if (status_ != AsrTransportStatus::kComplete) {
      status_ = AsrTransportStatus::kIdle;
    }
  }

  AsrTransportStatus status() const { return status_; }
  const std::string& transcript() const { return transcript_; }
  uint32_t providerCode() const { return provider_code_; }

 private:
  void handleEvent(WStype_t type, uint8_t* payload, std::size_t length) {
    if (type == WStype_CONNECTED) {
      const auto frame = buildDoubaoAsrFullClientFrame(metadata_);
      status_ = socket_.sendBIN(frame.data(), frame.size())
                    ? AsrTransportStatus::kReady
                    : AsrTransportStatus::kDisconnected;
      return;
    }
    if (type == WStype_DISCONNECTED) {
      if (status_ != AsrTransportStatus::kComplete &&
          status_ != AsrTransportStatus::kIdle) {
        status_ = AsrTransportStatus::kDisconnected;
      }
      return;
    }
    if (type != WStype_BIN) return;

    const auto result = parseDoubaoAsrServerFrame(payload, length);
    if (result.status == DoubaoAsrStatus::kIntermediateText) {
      transcript_ = result.text;
      return;
    }
    if (result.status == DoubaoAsrStatus::kFinalText) {
      transcript_ = result.text;
      status_ = AsrTransportStatus::kComplete;
      socket_.disconnect();
      return;
    }
    if (result.status == DoubaoAsrStatus::kProviderError) {
      provider_code_ = result.provider_code;
      status_ = AsrTransportStatus::kProviderError;
      socket_.disconnect();
      return;
    }
    if (result.status == DoubaoAsrStatus::kProtocolError) {
      status_ = AsrTransportStatus::kProtocolError;
      socket_.disconnect();
    }
  }

  WebSocketsClient socket_;
  DoubaoAsrMetadata metadata_;
  AsrTransportStatus status_ = AsrTransportStatus::kIdle;
  std::string extra_headers_;
  std::string transcript_;
  uint32_t provider_code_ = 0;
};

}  // namespace qh_voice
