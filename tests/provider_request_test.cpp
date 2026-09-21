#include <cassert>
#include <string>

#include "../firmware/esp32_voice_kit/provider_request.h"

namespace {

using qh_voice::HeaderValue;

const std::string* findHeader(const std::vector<HeaderValue>& headers,
                              const std::string& name) {
  for (const auto& header : headers) {
    if (header.name == name) return &header.value;
  }
  return nullptr;
}

void buildsDoubaoAsrHeaders() {
  const auto request = qh_voice::buildDoubaoAsrTransportRequest(
      "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel", "app-key",
      "access-key", "volc.bigasr.sauc.duration", "request-id");

  assert(request.endpoint ==
         "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel");
  assert(*findHeader(request.headers, "X-Api-App-Key") == "app-key");
  assert(*findHeader(request.headers, "X-Api-Access-Key") == "access-key");
  assert(*findHeader(request.headers, "X-Api-Resource-Id") ==
         "volc.bigasr.sauc.duration");
  assert(*findHeader(request.headers, "X-Api-Connect-Id") == "request-id");
  assert(findHeader(request.headers, "X-Api-Request-Id") == nullptr);
  assert(request.endpoint.find("access-key") == std::string::npos);
}

void buildsOpenAiCompatibleReplyHeaders() {
  const auto request = qh_voice::buildOpenAiReplyTransportRequest(
      "https://example.test/v1/chat/completions", "secret-token");

  assert(*findHeader(request.headers, "Authorization") ==
         "Bearer secret-token");
  assert(*findHeader(request.headers, "Content-Type") == "application/json");
  assert(request.endpoint.find("secret-token") == std::string::npos);
}

void buildsDoubaoTtsHeaders() {
  const auto request = qh_voice::buildDoubaoTtsTransportRequest(
      "https://openspeech.bytedance.com/api/v3/tts/unidirectional/sse",
      "tts-key", "volc.service_type.10029", "request-id");

  assert(*findHeader(request.headers, "X-Api-Key") == "tts-key");
  assert(*findHeader(request.headers, "X-Api-Resource-Id") ==
         "volc.service_type.10029");
  assert(*findHeader(request.headers, "X-Api-Request-Id") == "request-id");
  assert(*findHeader(request.headers, "Accept") == "text/event-stream");
  assert(*findHeader(request.headers, "Content-Type") == "application/json");
}

void summariesNeverContainCredentialsOrHeaderValues() {
  const auto request = qh_voice::buildOpenAiReplyTransportRequest(
      "https://example.test/v1/chat/completions", "do-not-log-this");
  const std::string summary = qh_voice::redactedTransportSummary("reply", request);

  assert(summary ==
         "provider=reply endpoint=https://example.test/v1/chat/completions "
         "headers=2");
  assert(summary.find("do-not-log-this") == std::string::npos);
  assert(summary.find("Authorization") == std::string::npos);
}

}  // namespace

int main() {
  buildsDoubaoAsrHeaders();
  buildsOpenAiCompatibleReplyHeaders();
  buildsDoubaoTtsHeaders();
  summariesNeverContainCredentialsOrHeaderValues();
  return 0;
}
