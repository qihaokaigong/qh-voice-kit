#pragma once

#include <WebSocketsClient.h>

#include <deque>
#include <string>
#include <utility>

#include "device_config.h"
#include "esp32_tls_bundle.h"
#include "realtime_voice_protocol.h"
#include "secure_endpoint.h"

namespace qh_voice {

enum class RealtimeTransportStatus {
  kIdle,
  kConnecting,
  kAwaitingSession,
  kReady,
  kProviderError,
  kProtocolError,
  kDisconnected,
};

class RealtimeVoiceTransport {
 public:
  bool begin(const RealtimeVoiceConfig& provider,
             RealtimeSessionConfig session) {
    const auto endpoint =
        parseSecureEndpoint(kRealtimeVoiceEndpoint, "wss://", 443);
    if (!endpoint || provider.api_key.empty() || session.session_id.empty() ||
        session.voice.empty()) {
      status_ = RealtimeTransportStatus::kProtocolError;
      return false;
    }

    close();
    events_.clear();
    provider_error_code_.clear();
    session_ = std::move(session);
    closing_ = false;
    status_ = RealtimeTransportStatus::kConnecting;

    const auto request = buildRealtimeTransportRequest(provider.api_key);
    extra_headers_ = formatWebSocketExtraHeaders(request.headers);

    socket_.onEvent([this](WStype_t type, uint8_t* payload,
                           std::size_t length) {
      handleSocketEvent(type, payload, length);
    });
    socket_.setExtraHeaders(extra_headers_.c_str());
    socket_.setReconnectInterval(0);
    socket_.beginSslWithBundle(
        endpoint->host.c_str(), endpoint->port, endpoint->path.c_str(),
        rootCaBundleData(), rootCaBundleSize(), "");
    return true;
  }

  void loop() { socket_.loop(); }

  bool beginInput(std::string_view event_id) {
    if (status_ != RealtimeTransportStatus::kReady) return false;
    return sendText(buildRealtimeUnmuteCommit(event_id));
  }

  bool sendPcm16(const uint8_t* pcm, std::size_t size) {
    if (status_ != RealtimeTransportStatus::kReady || pcm == nullptr ||
        size != kRealtimePcmFrameBytes) {
      return false;
    }
    return sendText(buildRealtimeAudioAppend(pcm, size));
  }

  bool commitInput(std::string_view commit_event_id,
                   std::string_view mute_event_id) {
    if (status_ != RealtimeTransportStatus::kReady) return false;
    if (!sendText(buildRealtimeInputCommit(commit_event_id))) return false;
    return sendText(buildRealtimeMuteCommit(mute_event_id));
  }

  bool muteInput(std::string_view event_id) {
    if (status_ != RealtimeTransportStatus::kReady) return false;
    return sendText(buildRealtimeMuteCommit(event_id));
  }

  bool popEvent(RealtimeEvent& event) {
    if (events_.empty()) return false;
    event = std::move(events_.front());
    events_.pop_front();
    return true;
  }

  void close() {
    if (status_ == RealtimeTransportStatus::kReady) {
      sendText(buildRealtimeSessionClose("local-session-close"));
    }
    closing_ = true;
    socket_.disconnect();
    events_.clear();
    status_ = RealtimeTransportStatus::kIdle;
  }

  RealtimeTransportStatus status() const { return status_; }
  const std::string& providerErrorCode() const {
    return provider_error_code_;
  }

 private:
  static constexpr std::size_t kMaximumQueuedEvents = 48;

  bool sendText(const std::string& message) {
    return socket_.sendTXT(message.c_str(), message.size());
  }

  void queueEvent(RealtimeEvent event) {
    if (events_.size() >= kMaximumQueuedEvents) {
      events_.clear();
      status_ = RealtimeTransportStatus::kProtocolError;
      RealtimeEvent overflow;
      overflow.type = RealtimeEventType::kProtocolError;
      events_.push_back(std::move(overflow));
      socket_.disconnect();
      return;
    }
    events_.push_back(std::move(event));
  }

  void handleSocketEvent(WStype_t type, uint8_t* payload,
                         std::size_t length) {
    if (type == WStype_CONNECTED) {
      status_ = RealtimeTransportStatus::kAwaitingSession;
      if (!sendText(buildRealtimeSessionCreate(session_, "session-create"))) {
        status_ = RealtimeTransportStatus::kDisconnected;
        socket_.disconnect();
      }
      return;
    }
    if (type == WStype_ERROR) {
      if (!closing_) status_ = RealtimeTransportStatus::kDisconnected;
      return;
    }
    if (type == WStype_DISCONNECTED) {
      if (!closing_ && status_ != RealtimeTransportStatus::kProviderError &&
          status_ != RealtimeTransportStatus::kProtocolError) {
        status_ = RealtimeTransportStatus::kDisconnected;
      }
      return;
    }
    if (type != WStype_TEXT || payload == nullptr || length == 0) return;

    RealtimeEvent event = parseRealtimeServerEvent(std::string_view(
        reinterpret_cast<const char*>(payload), length));
    if (event.type == RealtimeEventType::kSessionCreated) {
      if (!sendText(buildRealtimeMuteCommit("initial-mute"))) {
        status_ = RealtimeTransportStatus::kDisconnected;
        socket_.disconnect();
        return;
      }
      status_ = RealtimeTransportStatus::kReady;
    } else if (event.type == RealtimeEventType::kError) {
      provider_error_code_ = event.error_code;
      status_ = RealtimeTransportStatus::kProviderError;
    } else if (event.type == RealtimeEventType::kProtocolError) {
      status_ = RealtimeTransportStatus::kProtocolError;
    }
    queueEvent(std::move(event));
  }

  WebSocketsClient socket_;
  RealtimeSessionConfig session_;
  RealtimeTransportStatus status_ = RealtimeTransportStatus::kIdle;
  std::string extra_headers_;
  std::string provider_error_code_;
  std::deque<RealtimeEvent> events_;
  bool closing_ = true;
};

}  // namespace qh_voice
