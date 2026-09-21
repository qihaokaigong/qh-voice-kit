#include <cassert>

#include "../firmware/esp32_voice_kit/secure_endpoint.h"

namespace {

void parsesDefaultAndExplicitPorts() {
  const auto https = qh_voice::parseSecureEndpoint(
      "https://api.example.test/v1/chat?mode=short", "https://", 443);
  assert(https.has_value());
  assert(https->host == "api.example.test");
  assert(https->port == 443);
  assert(https->path == "/v1/chat?mode=short");

  const auto wss = qh_voice::parseSecureEndpoint(
      "wss://speech.example.test:8443/stream", "wss://", 443);
  assert(wss.has_value());
  assert(wss->host == "speech.example.test");
  assert(wss->port == 8443);
  assert(wss->path == "/stream");
}

void suppliesRootPath() {
  const auto endpoint = qh_voice::parseSecureEndpoint(
      "https://api.example.test", "https://", 443);
  assert(endpoint.has_value());
  assert(endpoint->path == "/");
}

void rejectsUnsafeOrAmbiguousAuthorities() {
  assert(!qh_voice::parseSecureEndpoint(
              "http://api.example.test/v1", "https://", 443)
              .has_value());
  assert(!qh_voice::parseSecureEndpoint(
              "https://token@api.example.test/v1", "https://", 443)
              .has_value());
  assert(!qh_voice::parseSecureEndpoint(
              "https://api.example.test:0/v1", "https://", 443)
              .has_value());
  assert(!qh_voice::parseSecureEndpoint(
              "https://api.example.test:70000/v1", "https://", 443)
              .has_value());
  assert(!qh_voice::parseSecureEndpoint(
              "https://api.example.test/v1#secret", "https://", 443)
              .has_value());
}

}  // namespace

int main() {
  parsesDefaultAndExplicitPorts();
  suppliesRootPath();
  rejectsUnsafeOrAmbiguousAuthorities();
  return 0;
}
