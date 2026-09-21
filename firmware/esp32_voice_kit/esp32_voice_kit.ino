#include <Arduino.h>
#include <ESP_I2S.h>
#include <SHA2Builder.h>
#include <WiFi.h>
#include <esp_system.h>

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <time.h>
#include <vector>

#include "config_transaction.h"
#include "button_debouncer.h"
#include "device_config_wire.h"
#include "doubao_asr_transport.h"
#include "pcm_audio.h"
#include "preferences_config_store.h"
#include "provider_http_transport.h"
#include "provisioning_command.h"
#include "voice_turn_state.h"

namespace {

qh_voice::PreferencesConfigStore config_store;
qh_voice::VoiceTurnMachine voice_turn;
qh_voice::ButtonDebouncer button(/*pressed_level=*/true,
                                 /*debounce_ms=*/40);
qh_voice::DoubaoAsrTransport asr_transport;
I2SClass audio_bus;
std::optional<qh_voice::DeviceConfig> active_config;
std::string command_line;
std::vector<uint8_t> config_payload;
std::size_t expected_payload_size = 0;
std::string expected_sha256;
bool store_ready = false;
uint32_t wifi_attempt_started_ms = 0;
uint32_t wifi_error_started_ms = 0;
uint32_t clock_sync_started_ms = 0;
uint32_t recording_started_ms = 0;
uint32_t provider_stage_started_ms = 0;
int32_t* recording_buffer = nullptr;
std::size_t recording_samples = 0;
std::size_t asr_upload_offset = 0;
bool microphone_running = false;
bool asr_finish_sent = false;
std::string current_turn_id;
std::string current_turn_started_at;

constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiRetryDelayMs = 5000;
constexpr uint32_t kClockSyncTimeoutMs = 20000;
constexpr uint32_t kProviderStageTimeoutMs = 30000;
constexpr uint32_t kMinimumRecordingMs = 300;
constexpr uint32_t kMaximumRecordingMs = 5000;
constexpr uint32_t kInputSampleRate = 16000;
constexpr uint32_t kOutputSampleRate = 24000;
constexpr std::size_t kMaximumRecordingSamples =
    kInputSampleRate * kMaximumRecordingMs / 1000;
constexpr std::size_t kCaptureSamplesPerRead = 256;
constexpr std::size_t kAsrSamplesPerFrame = 3200;
constexpr std::size_t kMaximumTtsAudioBytes = 512 * 1024;
constexpr int kMicSckPin = 4;
constexpr int kMicWsPin = 5;
constexpr int kMicSdPin = 6;
constexpr int kButtonPin = 8;
constexpr int kSpeakerBclkPin = 16;
constexpr int kSpeakerLrcPin = 17;
constexpr int kSpeakerDinPin = 18;

int32_t capture_buffer[kCaptureSamplesPerRead];
uint8_t asr_pcm_buffer[kAsrSamplesPerFrame * sizeof(int16_t)];

void emitError(const char* code) {
  Serial.printf(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"blocked\","
      "\"code\":\"%s\"}\n",
      code);
}

void emitStatus() {
  const auto active = store_ready ? config_store.activeConfig() : std::nullopt;
  Serial.printf(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"ready\","
      "\"configured\":%s,\"runtimeState\":\"%s\","
      "\"firmware\":\"qh-voice-kit-dev\"}\n",
      active ? "true" : "false", voice_turn.stateName().data());
}

void connectWifi() {
  if (!active_config) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(active_config->network.ssid.c_str(),
             active_config->network.password.c_str());
  wifi_attempt_started_ms = millis();
  Serial.println("RUNTIME state=connecting_wifi");
}

std::string makeRequestId() {
  char output[33];
  snprintf(output, sizeof(output), "%08lx%08lx%08lx%08lx",
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));
  return output;
}

std::string makeProviderUserId() {
  char output[32];
  snprintf(output, sizeof(output), "qh-voice-%012llx",
           static_cast<unsigned long long>(ESP.getEfuseMac()));
  return output;
}

std::string formatUtcNow() {
  const time_t now = time(nullptr);
  struct tm utc_time {};
  if (gmtime_r(&now, &utc_time) == nullptr) return "";
  char output[21];
  if (strftime(output, sizeof(output), "%Y-%m-%dT%H:%M:%SZ", &utc_time) ==
      0) {
    return "";
  }
  return output;
}

void stopMicrophone() {
  if (!microphone_running) return;
  audio_bus.end();
  microphone_running = false;
}

void failRuntime(const char* code) {
  stopMicrophone();
  asr_transport.close();
  if (voice_turn.apply(qh_voice::VoiceTurnEvent::kFailure)) {
    wifi_error_started_ms = millis();
  }
  Serial.printf("RUNTIME state=error code=%s\n", code);
}

bool startMicrophone() {
  audio_bus.setPins(kMicSckPin, kMicWsPin, -1, kMicSdPin);
  microphone_running = audio_bus.begin(
      I2S_MODE_STD, kInputSampleRate, I2S_DATA_BIT_WIDTH_32BIT,
      I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
  return microphone_running;
}

void beginRecording() {
  if (recording_buffer == nullptr ||
      !voice_turn.apply(qh_voice::VoiceTurnEvent::kButtonPressed)) {
    return;
  }
  recording_samples = 0;
  asr_upload_offset = 0;
  asr_finish_sent = false;
  current_turn_id = makeRequestId();
  current_turn_started_at = formatUtcNow();
  if (!startMicrophone()) {
    failRuntime("microphone_start_failed");
    return;
  }
  recording_started_ms = millis();
  Serial.println("RUNTIME state=recording");
}

void beginTranscription() {
  stopMicrophone();
  const uint32_t elapsed_ms =
      static_cast<uint32_t>(millis() - recording_started_ms);
  if (elapsed_ms < kMinimumRecordingMs || recording_samples == 0) {
    voice_turn.apply(qh_voice::VoiceTurnEvent::kRecordingCancelled);
    Serial.println("RUNTIME state=idle reason=recording_too_short");
    return;
  }
  if (!voice_turn.apply(qh_voice::VoiceTurnEvent::kButtonReleased) ||
      !active_config ||
      !asr_transport.begin(active_config->stt, makeRequestId(),
                           makeProviderUserId())) {
    failRuntime("asr_start_failed");
    return;
  }
  provider_stage_started_ms = millis();
  Serial.println("RUNTIME state=transcribing");
}

bool playPcm(std::vector<uint8_t>& audio, uint8_t volume_percent) {
  if (audio.empty()) return false;
  qh_voice::scalePcm16Le(audio.data(), audio.size(), volume_percent);
  audio_bus.setPins(kSpeakerBclkPin, kSpeakerLrcPin, kSpeakerDinPin);
  if (!audio_bus.begin(I2S_MODE_STD, kOutputSampleRate,
                       I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    return false;
  }
  const std::size_t written = audio_bus.write(audio.data(), audio.size());
  audio_bus.end();
  return written == audio.size();
}

void completeProviderTurn(const std::string& transcript) {
  using qh_voice::HttpTransportError;
  using qh_voice::VoiceTurnEvent;

  if (!active_config || transcript.empty() ||
      !voice_turn.apply(VoiceTurnEvent::kTranscriptReady)) {
    failRuntime("empty_transcript");
    return;
  }
  Serial.println("RUNTIME state=generating_reply");
  const qh_voice::OpenAiReplyRequest request{
      active_config->reply.model, active_config->assistant.system_prompt,
      transcript, active_config->assistant.max_reply_chars};
  const auto reply = qh_voice::requestOpenAiReply(active_config->reply, request);
  if (reply.error != HttpTransportError::kNone || reply.text.empty()) {
    failRuntime("reply_failed");
    return;
  }
  const std::string reply_text = qh_voice::truncateUtf8(
      reply.text, active_config->assistant.max_reply_chars);
  if (reply_text.empty()) {
    failRuntime("reply_failed");
    return;
  }

  voice_turn.apply(VoiceTurnEvent::kReplyReady);
  Serial.println("RUNTIME state=synthesizing");
  const qh_voice::DoubaoTtsRequest tts_request{
      makeProviderUserId(), reply_text, active_config->tts.speaker, "pcm",
      kOutputSampleRate, 0, 0};
  auto speech = qh_voice::requestDoubaoTts(
      active_config->tts, tts_request, makeRequestId(),
      kMaximumTtsAudioBytes);
  if (speech.error != HttpTransportError::kNone || speech.audio.empty()) {
    failRuntime("tts_failed");
    return;
  }

  voice_turn.apply(VoiceTurnEvent::kAudioReady);
  Serial.println("RUNTIME state=playing");
  if (!playPcm(speech.audio, active_config->preferences.volume_percent)) {
    failRuntime("playback_failed");
    return;
  }
  voice_turn.apply(VoiceTurnEvent::kPlaybackFinished);
  Serial.println("RUNTIME state=idle turn=complete");
  if (active_config->qh_sync.enabled) {
    const qh_voice::QhConversationTurn turn{
        active_config->qh_sync.device_id, current_turn_id,
        current_turn_started_at, transcript, reply_text};
    const auto sync =
        qh_voice::postQhConversationTurn(active_config->qh_sync, turn);
    Serial.printf("QH_SYNC status=%s http=%d\n",
                  sync.error == HttpTransportError::kNone ? "complete"
                                                         : "failed",
                  sync.http_status);
  }
}

void serviceRecording() {
  if (voice_turn.state() != qh_voice::VoiceTurnState::kRecording ||
      !microphone_running) {
    return;
  }
  const std::size_t remaining =
      kMaximumRecordingSamples - recording_samples;
  if (remaining == 0 ||
      static_cast<uint32_t>(millis() - recording_started_ms) >=
          kMaximumRecordingMs) {
    beginTranscription();
    return;
  }
  const std::size_t requested_samples =
      std::min(remaining, kCaptureSamplesPerRead);
  const std::size_t bytes = audio_bus.readBytes(
      reinterpret_cast<char*>(capture_buffer),
      requested_samples * sizeof(int32_t));
  if (bytes == 0) {
    failRuntime("microphone_read_failed");
    return;
  }
  const std::size_t samples = bytes / sizeof(int32_t);
  memcpy(recording_buffer + recording_samples, capture_buffer,
         samples * sizeof(int32_t));
  recording_samples += samples;
}

void serviceTranscription() {
  if (voice_turn.state() != qh_voice::VoiceTurnState::kTranscribing) return;
  asr_transport.loop();
  if (static_cast<uint32_t>(millis() - provider_stage_started_ms) >=
      kProviderStageTimeoutMs) {
    failRuntime("asr_timeout");
    return;
  }
  if (asr_transport.status() == qh_voice::AsrTransportStatus::kReady &&
      asr_upload_offset < recording_samples) {
    const std::size_t sample_count = std::min(
        kAsrSamplesPerFrame, recording_samples - asr_upload_offset);
    const std::size_t pcm_bytes = qh_voice::convertI2s32ToPcm16Le(
        recording_buffer + asr_upload_offset, sample_count, asr_pcm_buffer,
        sizeof(asr_pcm_buffer));
    if (pcm_bytes == 0 ||
        !asr_transport.sendPcm16(asr_pcm_buffer, pcm_bytes)) {
      failRuntime("asr_send_failed");
      return;
    }
    asr_upload_offset += sample_count;
  }
  if (asr_transport.status() == qh_voice::AsrTransportStatus::kReady &&
      asr_upload_offset == recording_samples && !asr_finish_sent) {
    if (!asr_transport.finish()) {
      failRuntime("asr_finish_failed");
      return;
    }
    asr_finish_sent = true;
  }
  if (asr_transport.status() == qh_voice::AsrTransportStatus::kComplete) {
    const std::string transcript = asr_transport.transcript();
    asr_transport.close();
    completeProviderTurn(transcript);
    return;
  }
  if (asr_transport.status() == qh_voice::AsrTransportStatus::kProviderError ||
      asr_transport.status() == qh_voice::AsrTransportStatus::kProtocolError ||
      asr_transport.status() == qh_voice::AsrTransportStatus::kDisconnected) {
    const auto diagnostic = asr_transport.failure();
    failRuntime("asr_failed");
    Serial.printf("ASR_DIAGNOSTIC category=%s http=%d provider=%lu\n",
                  qh_voice::asrFailureCategoryName(diagnostic.category),
                  diagnostic.http_status,
                  static_cast<unsigned long>(diagnostic.provider_code));
  }
}

void serviceButton() {
  const bool raw_level = digitalRead(kButtonPin) == HIGH;
  if (!button.update(raw_level, millis())) return;
  if (button.changedToPressed() &&
      voice_turn.state() == qh_voice::VoiceTurnState::kIdle) {
    beginRecording();
  } else if (button.changedToReleased() &&
             voice_turn.state() == qh_voice::VoiceTurnState::kRecording) {
    beginTranscription();
  }
}

void startConfiguredRuntime() {
  if (!active_config ||
      !voice_turn.apply(qh_voice::VoiceTurnEvent::kConfigLoaded)) {
    return;
  }
  connectWifi();
}

void serviceWifiRuntime() {
  using qh_voice::VoiceTurnEvent;
  using qh_voice::VoiceTurnState;

  if (voice_turn.state() == VoiceTurnState::kConnectingWifi) {
    if (WiFi.status() == WL_CONNECTED) {
      voice_turn.apply(VoiceTurnEvent::kWifiConnected);
      configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
      clock_sync_started_ms = millis();
      Serial.println("RUNTIME state=syncing_clock wifi=connected");
      return;
    }
    if (static_cast<uint32_t>(millis() - wifi_attempt_started_ms) >=
        kWifiConnectTimeoutMs) {
      WiFi.disconnect();
      voice_turn.apply(VoiceTurnEvent::kFailure);
      wifi_error_started_ms = millis();
      Serial.println("RUNTIME state=error code=wifi_connect_timeout");
    }
    return;
  }

  if (voice_turn.state() == VoiceTurnState::kSyncingClock) {
    if (WiFi.status() != WL_CONNECTED) {
      voice_turn.apply(VoiceTurnEvent::kFailure);
      wifi_error_started_ms = millis();
      Serial.println("RUNTIME state=error code=wifi_disconnected");
      return;
    }
    if (time(nullptr) >= 1704067200) {
      voice_turn.apply(VoiceTurnEvent::kClockSynchronized);
      Serial.println("RUNTIME state=idle clock=synchronized");
      return;
    }
    if (static_cast<uint32_t>(millis() - clock_sync_started_ms) >=
        kClockSyncTimeoutMs) {
      voice_turn.apply(VoiceTurnEvent::kFailure);
      wifi_error_started_ms = millis();
      Serial.println("RUNTIME state=error code=clock_sync_timeout");
    }
    return;
  }

  if (voice_turn.state() == VoiceTurnState::kIdle &&
      WiFi.status() != WL_CONNECTED) {
    voice_turn.apply(VoiceTurnEvent::kFailure);
    wifi_error_started_ms = millis();
    Serial.println("RUNTIME state=error code=wifi_disconnected");
    return;
  }

  if (voice_turn.state() == VoiceTurnState::kError &&
      static_cast<uint32_t>(millis() - wifi_error_started_ms) >=
          kWifiRetryDelayMs &&
      voice_turn.apply(VoiceTurnEvent::kRecover)) {
    connectWifi();
  }
}

void resetReceiver() {
  config_payload.clear();
  expected_payload_size = 0;
  expected_sha256.clear();
}

void applyReceivedConfig() {
  SHA256Builder digest;
  digest.begin();
  digest.add(config_payload.data(), config_payload.size());
  digest.calculate();
  if (!digest.toString().equalsIgnoreCase(expected_sha256.c_str())) {
    emitError("checksum_mismatch");
    resetReceiver();
    return;
  }

  const auto decoded = qh_voice::decodeDeviceConfigWire(
      config_payload.data(), config_payload.size());
  if (!decoded.config) {
    emitError("invalid_config");
    resetReceiver();
    return;
  }
  const auto result = qh_voice::applyDeviceConfig(*decoded.config, config_store);
  resetReceiver();
  if (result != qh_voice::ConfigApplyResult::kApplied) {
    emitError("config_storage_failed");
    return;
  }
  Serial.println(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"configured\","
      "\"rebootRequired\":false,\"restarting\":true}");
  Serial.flush();
  delay(250);
  ESP.restart();
}

void handleCommand() {
  const auto command = qh_voice::parseProvisioningCommand(command_line);
  command_line.clear();
  if (command.type == qh_voice::ProvisioningCommandType::kStatus) {
    emitStatus();
    return;
  }
  if (command.type != qh_voice::ProvisioningCommandType::kConfig ||
      !store_ready) {
    emitError("invalid_command");
    return;
  }
  expected_payload_size = command.payload_size;
  expected_sha256 = command.sha256;
  config_payload.clear();
  config_payload.reserve(expected_payload_size);
  Serial.println(
      "{\"protocol\":\"qh-voice-provision/1\",\"status\":\"send_payload\"}");
}

void readProvisioningSerial() {
  while (Serial.available() > 0) {
    const uint8_t value = static_cast<uint8_t>(Serial.read());
    if (expected_payload_size > 0) {
      config_payload.push_back(value);
      if (config_payload.size() == expected_payload_size) applyReceivedConfig();
      continue;
    }
    if (value == '\r') continue;
    if (value == '\n') {
      handleCommand();
      continue;
    }
    if (command_line.size() >= 160) {
      command_line.clear();
      emitError("command_too_long");
      continue;
    }
    command_line.push_back(static_cast<char>(value));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  store_ready = config_store.begin();
  if (!store_ready) {
    emitError("config_store_unavailable");
    return;
  }
  if (!psramFound()) {
    emitError("psram_not_found");
    return;
  }
  recording_buffer = static_cast<int32_t*>(
      ps_malloc(kMaximumRecordingSamples * sizeof(int32_t)));
  if (recording_buffer == nullptr) {
    emitError("recording_buffer_unavailable");
    return;
  }
  pinMode(kButtonPin, INPUT);
  button.begin(digitalRead(kButtonPin) == HIGH, millis());
  active_config = config_store.activeConfig();
  startConfiguredRuntime();
  emitStatus();
}

void loop() {
  readProvisioningSerial();
  serviceWifiRuntime();
  serviceButton();
  serviceRecording();
  serviceTranscription();
  delay(1);
}
