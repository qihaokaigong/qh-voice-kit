#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/qh_sync_protocol.h"

int main() {
  const qh_voice::QhConversationTurn turn{
      "conversation-device-1", "turn-1", "2026-09-21T20:00:00Z",
      "你好", "你好，世界"};

  const std::string body = qh_voice::buildQhConversationEventBody(turn);
  assert(body.find("\"type\":\"turn.started\"") != std::string::npos);
  assert(body.find("\"type\":\"input.transcribed\"") != std::string::npos);
  assert(body.find("\"type\":\"reply.generated\"") != std::string::npos);
  assert(body.find("\"text\":\"你好，世界\"") != std::string::npos);
  assert(body.find("provider-key") == std::string::npos);
  assert(body.find("audio") == std::string::npos);

  const auto request = qh_voice::buildQhSyncTransportRequest(
      "https://qh.example/api/v1/conversation-events", "qh-device-token");
  assert(request.endpoint ==
         "https://qh.example/api/v1/conversation-events");
  assert(request.headers.size() == 2);
  assert(request.headers[0].name == "Authorization");
  assert(request.headers[0].value == "Bearer qh-device-token");
  assert(request.endpoint.find("qh-device-token") == std::string::npos);
  return 0;
}
