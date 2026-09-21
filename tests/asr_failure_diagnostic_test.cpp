#include <cassert>
#include <string_view>

#include "../firmware/esp32_voice_kit/asr_failure_diagnostic.h"

namespace {

void classifiesAuthenticationHandshakeFailures() {
  const auto unauthorized =
      qh_voice::classifyAsrDisconnect("WebSocket handshake failed - HTTP 401");
  assert(unauthorized.category == qh_voice::AsrFailureCategory::kAuth);
  assert(unauthorized.http_status == 401);

  const auto forbidden = qh_voice::classifyAsrDisconnect("HTTP 403");
  assert(forbidden.category == qh_voice::AsrFailureCategory::kAuth);
  assert(forbidden.http_status == 403);
}

void classifiesOtherHandshakeAndNetworkFailures() {
  const auto rejected = qh_voice::classifyAsrDisconnect("HTTP 429");
  assert(rejected.category == qh_voice::AsrFailureCategory::kHandshake);
  assert(rejected.http_status == 429);

  const auto network = qh_voice::classifyAsrDisconnect("Connection lost");
  assert(network.category == qh_voice::AsrFailureCategory::kNetwork);
  assert(network.http_status == 0);
}

void exposesOnlyStableRedactedLabels() {
  assert(std::string_view(qh_voice::asrFailureCategoryName(
             qh_voice::AsrFailureCategory::kAuth)) == "auth");
  assert(std::string_view(qh_voice::asrFailureCategoryName(
             qh_voice::AsrFailureCategory::kHandshake)) == "handshake");
  assert(std::string_view(qh_voice::asrFailureCategoryName(
             qh_voice::AsrFailureCategory::kNetwork)) == "network");
  assert(std::string_view(qh_voice::asrFailureCategoryName(
             qh_voice::AsrFailureCategory::kProvider)) == "provider");
  assert(std::string_view(qh_voice::asrFailureCategoryName(
             qh_voice::AsrFailureCategory::kProtocol)) == "protocol");
}

}  // namespace

int main() {
  classifiesAuthenticationHandshakeFailures();
  classifiesOtherHandshakeAndNetworkFailures();
  exposesOnlyStableRedactedLabels();
  return 0;
}
