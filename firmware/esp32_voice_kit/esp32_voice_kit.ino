#include <Arduino.h>
#include <ESP_I2S.h>
#include <SHA2Builder.h>
#include <WiFi.h>
#include <esp_system.h>

#include <optional>
#include <string>
#include <time.h>
#include <vector>

#include "audio_frame_pacer.h"
#include "buffered_pcm_playback.h"
#include "button_debouncer.h"
#include "config_transaction.h"
#include "device_config_wire.h"
#include "device_display.h"
#include "pcm_audio.h"
#include "preferences_config_store.h"
#include "provisioning_command.h"
#include "qh_sync_transport.h"
#include "realtime_voice_protocol.h"
#include "realtime_voice_transport.h"
#include "voice_turn_state.h"

namespace {

qh_voice::PreferencesConfigStore config_store;
qh_voice::VoiceTurnMachine voice_turn;
qh_voice::ButtonDebouncer button(/*pressed_level=*/true, /*debounce_ms=*/40);
qh_voice::RealtimeVoiceTransport realtime_transport;
qh_voice::AudioFramePacer frame_pacer(qh_voice::kRealtimeFrameDurationMs);
qh_voice::DeviceDisplay device_display;
I2SClass audio_bus;
qh_voice::BufferedPcmPlayback buffered_playback(audio_bus);

std::optional<qh_voice::DeviceConfig> active_config;
std::string command_line;
std::vector<uint8_t> config_payload;
std::size_t expected_payload_size = 0;
std::string expected_sha256;
bool store_ready = false;

uint32_t wifi_attempt_started_ms = 0;
uint32_t runtime_error_started_ms = 0;
uint32_t clock_sync_started_ms = 0;
uint32_t provider_connect_started_ms = 0;
uint32_t recording_started_ms = 0;
uint32_t response_started_ms = 0;
uint32_t success_display_started_ms = 0;

bool microphone_running = false;
bool speaker_running = false;
bool response_done_received = false;
bool output_audio_done_received = false;
bool success_display_active = false;
std::size_t capture_samples = 0;
std::size_t streamed_frames = 0;
std::string current_turn_id;
std::string current_turn_started_at;
std::string current_transcript;
std::string current_reply;

constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kRuntimeRetryDelayMs = 5000;
constexpr uint32_t kClockSyncTimeoutMs = 20000;
constexpr uint32_t kProviderConnectTimeoutMs = 20000;
constexpr uint32_t kProviderResponseTimeoutMs = 45000;
constexpr uint32_t kMinimumRecordingMs = 300;
constexpr uint32_t kMaximumRecordingMs = 10000;
constexpr uint32_t kSuccessDisplayMs = 1200;
constexpr int kMicSckPin = 4;
constexpr int kMicWsPin = 5;
constexpr int kMicSdPin = 6;
constexpr int kButtonPin = 8;
constexpr int kSpeakerBclkPin = 16;
constexpr int kSpeakerLrcPin = 17;
constexpr int kSpeakerDinPin = 18;
constexpr std::size_t kCaptureSamplesPerFrame =
    qh_voice::kRealtimePcmFrameBytes / sizeof(int16_t);

int32_t capture_buffer[kCaptureSamplesPerFrame];
uint8_t pcm_frame[qh_voice::kRealtimePcmFrameBytes];

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
      "\"firmware\":\"qh-voice-kit-realtime-dev\"}\n",
      active ? "true" : "false", voice_turn.stateName().data());
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

void stopAudioBus() {
  if (!microphone_running && !speaker_running) return;
  if (speaker_running) buffered_playback.cancel();
  audio_bus.end();
  microphone_running = false;
  speaker_running = false;
}

void connectWifi() {
  if (!active_config) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(active_config->network.ssid.c_str(),
             active_config->network.password.c_str());
  wifi_attempt_started_ms = millis();
  device_display.show(qh_voice::UiState::kConnecting);
  Serial.println("RUNTIME state=connecting_wifi");
}

void failRuntime(const char* code) {
  stopAudioBus();
  realtime_transport.close();
  voice_turn.apply(qh_voice::VoiceTurnEvent::kFailure);
  runtime_error_started_ms = millis();
  device_display.showError(code);
  Serial.printf("RUNTIME state=error code=%s\n", code);
}

bool startMicrophone() {
  stopAudioBus();
  audio_bus.setPins(kMicSckPin, kMicWsPin, -1, kMicSdPin);
  microphone_running = audio_bus.begin(
      I2S_MODE_STD, qh_voice::kRealtimeInputSampleRate,
      I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT);
  return microphone_running;
}

bool startSpeaker() {
  if (speaker_running) return true;
  stopAudioBus();
  audio_bus.setPins(kSpeakerBclkPin, kSpeakerLrcPin, kSpeakerDinPin);
  speaker_running = audio_bus.begin(
      I2S_MODE_STD, qh_voice::kRealtimeOutputSampleRate,
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  return speaker_running;
}

void startProviderConnection() {
  if (!active_config ||
      voice_turn.state() != qh_voice::VoiceTurnState::kConnectingProvider) {
    return;
  }
  const qh_voice::RealtimeSessionConfig session{
      makeRequestId(), active_config->assistant.system_prompt,
      active_config->realtime_voice.voice};
  if (!realtime_transport.begin(active_config->realtime_voice, session)) {
    failRuntime("provider_config_invalid");
    return;
  }
  provider_connect_started_ms = millis();
  device_display.show(qh_voice::UiState::kConnecting);
  Serial.println("RUNTIME state=connecting_provider");
}

void beginRecording() {
  if (!active_config ||
      realtime_transport.status() !=
          qh_voice::RealtimeTransportStatus::kReady ||
      !voice_turn.apply(qh_voice::VoiceTurnEvent::kButtonPressed)) {
    return;
  }
  current_turn_id = makeRequestId();
  current_turn_started_at = formatUtcNow();
  current_transcript.clear();
  current_reply.clear();
  response_done_received = false;
  output_audio_done_received = false;
  capture_samples = 0;
  streamed_frames = 0;
  if (!startMicrophone()) {
    failRuntime("microphone_start_failed");
    return;
  }
  if (!realtime_transport.beginInput(makeRequestId())) {
    failRuntime("provider_send_failed");
    return;
  }
  recording_started_ms = millis();
  frame_pacer.reset(recording_started_ms);
  success_display_active = false;
  device_display.show(qh_voice::UiState::kRecording);
  Serial.println("RUNTIME state=recording_and_streaming");
}

void finishRecording() {
  if (voice_turn.state() !=
      qh_voice::VoiceTurnState::kRecordingAndStreaming) {
    return;
  }
  stopAudioBus();
  const uint32_t duration_ms =
      static_cast<uint32_t>(millis() - recording_started_ms);
  if (duration_ms < kMinimumRecordingMs || streamed_frames == 0) {
    realtime_transport.muteInput(makeRequestId());
    voice_turn.apply(qh_voice::VoiceTurnEvent::kRecordingCancelled);
    device_display.show(qh_voice::UiState::kIdle);
    Serial.println("RUNTIME state=idle reason=recording_too_short");
    return;
  }
  if (!realtime_transport.commitInput(makeRequestId(), makeRequestId()) ||
      !voice_turn.apply(qh_voice::VoiceTurnEvent::kButtonReleased)) {
    failRuntime("provider_commit_failed");
    return;
  }
  response_started_ms = millis();
  device_display.show(qh_voice::UiState::kThinking);
  Serial.printf("RUNTIME state=waiting_response frames=%lu\n",
                static_cast<unsigned long>(streamed_frames));
}

void serviceRecording() {
  if (voice_turn.state() !=
          qh_voice::VoiceTurnState::kRecordingAndStreaming ||
      !microphone_running) {
    return;
  }
  if (static_cast<uint32_t>(millis() - recording_started_ms) >=
      kMaximumRecordingMs) {
    finishRecording();
    return;
  }

  if (capture_samples < kCaptureSamplesPerFrame) {
    const std::size_t remaining = kCaptureSamplesPerFrame - capture_samples;
    const std::size_t bytes = audio_bus.readBytes(
        reinterpret_cast<char*>(capture_buffer + capture_samples),
        remaining * sizeof(int32_t));
    if (bytes == 0 || bytes % sizeof(int32_t) != 0) {
      failRuntime("microphone_read_failed");
      return;
    }
    capture_samples += bytes / sizeof(int32_t);
  }
  if (capture_samples != kCaptureSamplesPerFrame || !frame_pacer.due(millis())) {
    return;
  }
  const std::size_t pcm_bytes = qh_voice::convertI2s32ToPcm16Le(
      capture_buffer, capture_samples, pcm_frame, sizeof(pcm_frame));
  if (pcm_bytes != qh_voice::kRealtimePcmFrameBytes ||
      !realtime_transport.sendPcm16(pcm_frame, pcm_bytes)) {
    failRuntime("provider_send_failed");
    return;
  }
  capture_samples = 0;
  ++streamed_frames;
}

bool writeOutputAudio(std::string_view encoded) {
  if (!active_config) return false;
  std::vector<uint8_t> pcm;
  if (!qh_voice::decodeBase64(encoded, pcm) || pcm.empty() ||
      pcm.size() % sizeof(int16_t) != 0) {
    buffered_playback.reportFailure(
        qh_voice::PlaybackFailureStage::kPcmDecode);
    return false;
  }
  if (!speaker_running) {
    if (!startSpeaker()) {
      buffered_playback.reportFailure(
          qh_voice::PlaybackFailureStage::kSpeakerStart);
      return false;
    }
    if (!buffered_playback.beginTurn()) return false;
  }
  if (voice_turn.state() == qh_voice::VoiceTurnState::kWaitingResponse) {
    voice_turn.apply(qh_voice::VoiceTurnEvent::kOutputAudioStarted);
  }
  qh_voice::scalePcm16Le(pcm.data(), pcm.size(),
                         active_config->preferences.volume_percent);
  device_display.show(qh_voice::UiState::kPlaying);
  return buffered_playback.enqueue(pcm.data(), pcm.size());
}

void emitPlaybackFailure() {
  Serial.printf(
      "PLAYBACK failure_stage=%s bytes=%lu queue_peak=%lu underruns=%lu\n",
      qh_voice::playbackFailureStageName(buffered_playback.failureStage())
          .data(),
      static_cast<unsigned long>(buffered_playback.totalEnqueuedBytes()),
      static_cast<unsigned long>(buffered_playback.maximumQueuedBytes()),
      static_cast<unsigned long>(buffered_playback.underrunCount()));
}

void syncCompletedTurn() {
  if (!active_config || !active_config->qh_sync.enabled) return;
  const qh_voice::QhConversationTurn turn{
      active_config->qh_sync.device_id, current_turn_id,
      current_turn_started_at, current_transcript, current_reply};
  const auto result =
      qh_voice::postQhConversationTurn(active_config->qh_sync, turn);
  Serial.printf("QH_SYNC status=%s http=%d\n",
                result.error == qh_voice::QhSyncError::kNone ? "complete"
                                                             : "failed",
                result.http_status);
}

void completeTurn() {
  const std::size_t playback_bytes = buffered_playback.totalEnqueuedBytes();
  const std::size_t playback_peak = buffered_playback.maximumQueuedBytes();
  const std::size_t playback_underruns = buffered_playback.underrunCount();
  stopAudioBus();
  if (!voice_turn.apply(qh_voice::VoiceTurnEvent::kResponseDone)) {
    failRuntime("provider_event_order_invalid");
    return;
  }
  device_display.show(qh_voice::UiState::kSuccess);
  success_display_active = true;
  success_display_started_ms = millis();
  Serial.printf(
      "PLAYBACK bytes=%lu queue_peak=%lu underruns=%lu\n",
      static_cast<unsigned long>(playback_bytes),
      static_cast<unsigned long>(playback_peak),
      static_cast<unsigned long>(playback_underruns));
  Serial.println("RUNTIME state=idle turn=complete");
  syncCompletedTurn();
}

const char* providerErrorCode(std::string_view code) {
  if (code.find("auth") != std::string_view::npos ||
      code.find("401") != std::string_view::npos ||
      code.find("credential") != std::string_view::npos) {
    return "provider_auth_failed";
  }
  return "provider_rejected";
}

void serviceRealtimeEvents() {
  qh_voice::RealtimeEvent event;
  while (realtime_transport.popEvent(event)) {
    switch (event.type) {
      case qh_voice::RealtimeEventType::kTranscriptDelta:
        current_transcript += event.text;
        break;
      case qh_voice::RealtimeEventType::kTranscriptCompleted:
        if (!event.text.empty()) current_transcript = event.text;
        if (active_config && active_config->assistant.show_reply_text &&
            !current_transcript.empty()) {
          device_display.showConversation("我听到了", current_transcript,
                                          ST77XX_YELLOW);
        }
        break;
      case qh_voice::RealtimeEventType::kOutputTextDelta:
        current_reply += event.text;
        break;
      case qh_voice::RealtimeEventType::kOutputTextDone:
        if (!event.text.empty()) current_reply = event.text;
        if (active_config && active_config->assistant.show_reply_text &&
            !current_reply.empty()) {
          device_display.showConversation("回答", current_reply,
                                          ST77XX_GREEN);
        }
        break;
      case qh_voice::RealtimeEventType::kOutputAudioStarted:
        if (voice_turn.state() == qh_voice::VoiceTurnState::kWaitingResponse) {
          voice_turn.apply(qh_voice::VoiceTurnEvent::kOutputAudioStarted);
        }
        device_display.show(qh_voice::UiState::kPlaying);
        break;
      case qh_voice::RealtimeEventType::kOutputAudioDelta:
        if (!writeOutputAudio(event.audio_base64)) {
          emitPlaybackFailure();
          failRuntime("playback_failed");
          return;
        }
        break;
      case qh_voice::RealtimeEventType::kOutputAudioDone:
        output_audio_done_received = true;
        buffered_playback.finishInput();
        break;
      case qh_voice::RealtimeEventType::kResponseDone:
        response_done_received = true;
        if (!speaker_running) completeTurn();
        break;
      case qh_voice::RealtimeEventType::kResponseCanceled:
      case qh_voice::RealtimeEventType::kTranscriptFailed:
        failRuntime("provider_response_failed");
        return;
      case qh_voice::RealtimeEventType::kError:
        event.error_code = qh_voice::safeRealtimeErrorCode(event.error_code);
        Serial.printf("RUNTIME provider_error_code=%s\n",
                      event.error_code.c_str());
        failRuntime(providerErrorCode(event.error_code));
        return;
      case qh_voice::RealtimeEventType::kProtocolError:
        failRuntime("provider_protocol_failed");
        return;
      default:
        break;
    }
  }
}

void serviceProvider() {
  using qh_voice::RealtimeTransportStatus;
  using qh_voice::VoiceTurnEvent;
  using qh_voice::VoiceTurnState;

  if (realtime_transport.status() != RealtimeTransportStatus::kIdle) {
    realtime_transport.loop();
  }
  serviceRealtimeEvents();

  if (voice_turn.state() == VoiceTurnState::kConnectingProvider) {
    if (realtime_transport.status() == RealtimeTransportStatus::kReady) {
      voice_turn.apply(VoiceTurnEvent::kProviderConnected);
      device_display.show(qh_voice::UiState::kIdle);
      Serial.println("RUNTIME state=idle provider=connected");
      return;
    }
    if (static_cast<uint32_t>(millis() - provider_connect_started_ms) >=
        kProviderConnectTimeoutMs) {
      failRuntime("provider_connect_timeout");
      return;
    }
  }

  const auto transport_status = realtime_transport.status();
  if (transport_status == RealtimeTransportStatus::kProviderError) {
    const std::string error_code = qh_voice::safeRealtimeErrorCode(
        realtime_transport.providerErrorCode());
    Serial.printf("RUNTIME provider_error_code=%s\n", error_code.c_str());
    failRuntime(providerErrorCode(error_code));
    return;
  }
  if (transport_status == RealtimeTransportStatus::kProtocolError) {
    failRuntime("provider_protocol_failed");
    return;
  }
  if (transport_status == RealtimeTransportStatus::kDisconnected &&
      voice_turn.state() != VoiceTurnState::kError) {
    Serial.printf("RUNTIME provider_disconnect=%s last_event=%s\n",
                  realtime_transport.disconnectCode().c_str(),
                  qh_voice::realtimeEventDiagnosticName(
                      realtime_transport.lastEventType()));
    failRuntime("provider_disconnected");
    return;
  }

  if (voice_turn.state() == VoiceTurnState::kPlaying &&
      buffered_playback.failed()) {
    emitPlaybackFailure();
    failRuntime("playback_failed");
    return;
  }

  if ((voice_turn.state() == VoiceTurnState::kWaitingResponse ||
       voice_turn.state() == VoiceTurnState::kPlaying) &&
      static_cast<uint32_t>(millis() - response_started_ms) >=
          kProviderResponseTimeoutMs) {
    failRuntime("provider_response_timeout");
    return;
  }
  if (speaker_running && response_done_received && output_audio_done_received &&
      buffered_playback.drained()) {
    completeTurn();
  }
}

void serviceButton() {
  const bool raw_level = digitalRead(kButtonPin) == HIGH;
  if (!button.update(raw_level, millis())) return;
  if (button.changedToPressed() &&
      voice_turn.state() == qh_voice::VoiceTurnState::kIdle) {
    beginRecording();
  } else if (button.changedToReleased() &&
             voice_turn.state() ==
                 qh_voice::VoiceTurnState::kRecordingAndStreaming) {
    finishRecording();
  }
}

void startConfiguredRuntime() {
  if (!active_config) {
    device_display.show(qh_voice::UiState::kSetupRequired);
    return;
  }
  if (voice_turn.apply(qh_voice::VoiceTurnEvent::kConfigLoaded)) connectWifi();
}

void serviceNetworkRuntime() {
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
      failRuntime("wifi_connect_timeout");
    }
    return;
  }

  if (voice_turn.state() == VoiceTurnState::kSyncingClock) {
    if (WiFi.status() != WL_CONNECTED) {
      failRuntime("wifi_disconnected");
      return;
    }
    if (time(nullptr) >= 1704067200) {
      voice_turn.apply(VoiceTurnEvent::kClockSynchronized);
      startProviderConnection();
      return;
    }
    if (static_cast<uint32_t>(millis() - clock_sync_started_ms) >=
        kClockSyncTimeoutMs) {
      failRuntime("clock_sync_timeout");
    }
    return;
  }

  if ((voice_turn.state() == VoiceTurnState::kConnectingProvider ||
       voice_turn.state() == VoiceTurnState::kIdle ||
       voice_turn.state() == VoiceTurnState::kRecordingAndStreaming ||
       voice_turn.state() == VoiceTurnState::kWaitingResponse ||
       voice_turn.state() == VoiceTurnState::kPlaying) &&
      WiFi.status() != WL_CONNECTED) {
    failRuntime("wifi_disconnected");
    return;
  }

  if (voice_turn.state() == VoiceTurnState::kError &&
      static_cast<uint32_t>(millis() - runtime_error_started_ms) >=
          kRuntimeRetryDelayMs &&
      voice_turn.apply(VoiceTurnEvent::kRecover)) {
    connectWifi();
  }

  if (success_display_active &&
      voice_turn.state() == VoiceTurnState::kIdle &&
      static_cast<uint32_t>(millis() - success_display_started_ms) >=
          kSuccessDisplayMs) {
    success_display_active = false;
    device_display.show(qh_voice::UiState::kIdle);
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
  device_display.begin();
  delay(250);
  if (!buffered_playback.begin()) {
    device_display.showError("playback_buffer_unavailable");
    emitError("playback_buffer_unavailable");
    return;
  }
  store_ready = config_store.begin();
  if (!store_ready) {
    device_display.showError("config_store_unavailable");
    emitError("config_store_unavailable");
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
  serviceNetworkRuntime();
  serviceProvider();
  serviceButton();
  serviceRecording();
  delay(1);
}
