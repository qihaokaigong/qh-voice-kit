#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "../firmware/esp32_voice_kit/doubao_tts_sse.h"

namespace {

void parsesFragmentedAudioAndCompletionEvents() {
  qh_voice::DoubaoTtsSseParser parser(32);

  assert(parser.feed("event: 352\ndata: {\"code\":0,\"data\":\"SU"));
  assert(parser.feed("Qz\"}\n\nevent: 352\ndata: {\"code\":0,\"data\":\"BAU=\"}\n\n"));
  assert(parser.feed("event: 152\ndata: {\"code\":20000000,\"message\":\"OK\",\"data\":null}\n\n"));
  assert(parser.finish());

  const std::vector<uint8_t> expected{'I', 'D', '3', 4, 5};
  assert(parser.audio() == expected);
  assert(parser.status() == qh_voice::DoubaoTtsStatus::kComplete);
  assert(parser.providerCode() == 20000000);
}

void reportsProviderFailureWithoutKeepingAudio() {
  qh_voice::DoubaoTtsSseParser parser(32);

  assert(parser.feed("event: 352\ndata: {\"code\":0,\"data\":\"SUQz\"}\n\n"));
  assert(!parser.feed("event: 153\ndata: {\"code\":45000000,\"message\":\"denied\"}\n\n"));
  assert(!parser.finish());

  assert(parser.status() == qh_voice::DoubaoTtsStatus::kProviderError);
  assert(parser.providerCode() == 45000000);
  assert(parser.audio().empty());
}

void rejectsMalformedBase64() {
  qh_voice::DoubaoTtsSseParser parser(32);

  assert(!parser.feed("event: 352\ndata: {\"code\":0,\"data\":\"%%%\"}\n\n"));
  assert(parser.status() == qh_voice::DoubaoTtsStatus::kProtocolError);
}

void rejectsAudioBeyondConfiguredLimit() {
  qh_voice::DoubaoTtsSseParser parser(2);

  assert(!parser.feed("event: 352\ndata: {\"code\":0,\"data\":\"SUQz\"}\n\n"));
  assert(parser.status() == qh_voice::DoubaoTtsStatus::kTooLarge);
  assert(parser.audio().empty());
}

void requiresSessionFinish() {
  qh_voice::DoubaoTtsSseParser parser(32);
  assert(parser.feed("event: 351\ndata: {\"code\":0,\"data\":null}\n\n"));
  assert(!parser.finish());
  assert(parser.status() == qh_voice::DoubaoTtsStatus::kProtocolError);
}

}  // namespace

int main() {
  parsesFragmentedAudioAndCompletionEvents();
  reportsProviderFailureWithoutKeepingAudio();
  rejectsMalformedBase64();
  rejectsAudioBeyondConfiguredLimit();
  requiresSessionFinish();
  return 0;
}
