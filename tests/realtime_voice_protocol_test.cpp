#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "../firmware/esp32_voice_kit/realtime_voice_protocol.h"

namespace {

void buildsOfficialSessionCreateShape() {
  const qh_voice::RealtimeSessionConfig config{
      "session-1", "用简短中文回答。", "zh_female_xiaohe_jupiter_bigtts"};
  const std::string event =
      qh_voice::buildRealtimeSessionCreate(config, "event-1");

  assert(event.find("\"type\":\"session.create\"") != std::string::npos);
  assert(event.find("\"event_id\":\"event-1\"") != std::string::npos);
  assert(event.find("\"id\":\"session-1\"") != std::string::npos);
  assert(event.find("\"model\":\"1.2.6.1\"") != std::string::npos);
  assert(event.find("\"type\":\"pcm\",\"rate\":16000") !=
         std::string::npos);
  assert(event.find("\"type\":\"pcm_s16le\",\"rate\":24000") !=
         std::string::npos);
  assert(event.find("zh_female_xiaohe_jupiter_bigtts") != std::string::npos);
}

void buildsBase64AudioAndControlEvents() {
  const uint8_t pcm[] = {0x00, 0x01, 0xfe, 0xff};
  const std::string append =
      qh_voice::buildRealtimeAudioAppend(pcm, sizeof(pcm));
  assert(append ==
         "{\"type\":\"input_audio_buffer.append\",\"audio\":\"AAH+/w==\"}");
  assert(qh_voice::buildRealtimeInputCommit("event-2") ==
         "{\"type\":\"input_audio_buffer.commit\",\"event_id\":\"event-2\"}");
  assert(qh_voice::buildRealtimeMuteCommit("event-3") ==
         "{\"type\":\"input_audio_mute.commit\",\"event_id\":\"event-3\"}");
  assert(qh_voice::buildRealtimeUnmuteCommit("event-4") ==
         "{\"type\":\"input_audio_unmute.commit\",\"event_id\":\"event-4\"}");
}

void decodesOutputAudioAndBuildsSecretHeader() {
  std::vector<uint8_t> decoded;
  assert(qh_voice::decodeBase64("AAH+/w==", decoded));
  assert((decoded == std::vector<uint8_t>{0x00, 0x01, 0xfe, 0xff}));
  assert(!qh_voice::decodeBase64("bad!", decoded));

  const auto request =
      qh_voice::buildRealtimeTransportRequest("realtime-api-key");
  assert(request.endpoint == qh_voice::kRealtimeVoiceEndpoint);
  assert(request.headers.size() == 1);
  assert(request.headers[0].name == "X-Api-Key");
  assert(request.headers[0].value == "realtime-api-key");
}

void parsesConversationEventsWithoutRawProviderPayload() {
  auto event = qh_voice::parseRealtimeServerEvent(
      "{\"type\":\"session.created\",\"session\":{\"id\":\"dialog-1\"}}");
  assert(event.type == qh_voice::RealtimeEventType::kSessionCreated);
  assert(event.session_id == "dialog-1");

  event = qh_voice::parseRealtimeServerEvent(
      "{\"type\":\"conversation.item.input_audio_transcription.completed\","
      "\"transcript\":\"你好\"}");
  assert(event.type == qh_voice::RealtimeEventType::kTranscriptCompleted);
  assert(event.text == "你好");

  event = qh_voice::parseRealtimeServerEvent(
      "{\"type\":\"response.output_text.done\",\"text\":\"你好呀\"}");
  assert(event.type == qh_voice::RealtimeEventType::kOutputTextDone);
  assert(event.text == "你好呀");

  event = qh_voice::parseRealtimeServerEvent(
      "{\"type\":\"response.output_audio.delta\",\"delta\":\"AAE=\"}");
  assert(event.type == qh_voice::RealtimeEventType::kOutputAudioDelta);
  assert(event.audio_base64 == "AAE=");

  event = qh_voice::parseRealtimeServerEvent(
      "{\"type\":\"error\",\"error\":{\"code\":\"quota_exceeded\","
      "\"message\":\"secret provider detail\"}}");
  assert(event.type == qh_voice::RealtimeEventType::kError);
  assert(event.error_code == "quota_exceeded");
  assert(event.text.empty());

  event = qh_voice::parseRealtimeServerEvent(
      "{\"type\":\"error\",\"status_code\":45000001,"
      "\"message\":\"sensitive provider detail\"}");
  assert(event.type == qh_voice::RealtimeEventType::kError);
  assert(event.error_code == "45000001");
  assert(event.text.empty());
}

void rejectsMissingOrUnknownTypes() {
  assert(qh_voice::parseRealtimeServerEvent("{}").type ==
         qh_voice::RealtimeEventType::kProtocolError);
  assert(qh_voice::parseRealtimeServerEvent("{\"type\":\"future.event\"}")
             .type == qh_voice::RealtimeEventType::kUnknown);
}

void sanitizesProviderCodesBeforeSerialDiagnostics() {
  assert(qh_voice::safeRealtimeErrorCode("45000001") == "45000001");
  assert(qh_voice::safeRealtimeErrorCode("quota_exceeded") ==
         "quota_exceeded");
  assert(qh_voice::safeRealtimeErrorCode("secret key: value") ==
         "provider_error");
  assert(qh_voice::safeRealtimeErrorCode(std::string(49, 'a')) ==
         "provider_error");
}

void namesProtocolStagesWithoutContent() {
  assert(std::string(qh_voice::realtimeEventDiagnosticName(
             qh_voice::RealtimeEventType::kInputCommitted)) ==
         "input_committed");
  assert(std::string(qh_voice::realtimeEventDiagnosticName(
             qh_voice::RealtimeEventType::kTranscriptCompleted)) ==
         "transcript_completed");
  assert(std::string(qh_voice::realtimeEventDiagnosticName(
             qh_voice::RealtimeEventType::kOutputAudioDelta)) ==
         "output_audio_delta");
  assert(std::string(qh_voice::realtimeEventDiagnosticName(
             qh_voice::RealtimeEventType::kUnknown)) == "none");
  assert(qh_voice::rememberRealtimeEventType(
             qh_voice::RealtimeEventType::kInputCommitted,
             qh_voice::RealtimeEventType::kUnknown) ==
         qh_voice::RealtimeEventType::kInputCommitted);
  assert(qh_voice::rememberRealtimeEventType(
             qh_voice::RealtimeEventType::kInputCommitted,
             qh_voice::RealtimeEventType::kTranscriptStarted) ==
         qh_voice::RealtimeEventType::kTranscriptStarted);
}

}  // namespace

int main() {
  buildsOfficialSessionCreateShape();
  buildsBase64AudioAndControlEvents();
  decodesOutputAudioAndBuildsSecretHeader();
  parsesConversationEventsWithoutRawProviderPayload();
  rejectsMissingOrUnknownTypes();
  sanitizesProviderCodesBeforeSerialDiagnostics();
  namesProtocolStagesWithoutContent();
  return 0;
}
