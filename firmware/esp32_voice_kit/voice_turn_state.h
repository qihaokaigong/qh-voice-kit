#pragma once

#include <string_view>

namespace qh_voice {

enum class VoiceTurnState {
  kUnconfigured,
  kConnectingWifi,
  kSyncingClock,
  kConnectingProvider,
  kIdle,
  kRecordingAndStreaming,
  kWaitingResponse,
  kPlaying,
  kError,
};

enum class VoiceTurnEvent {
  kConfigLoaded,
  kConfigCleared,
  kWifiConnected,
  kClockSynchronized,
  kProviderConnected,
  kButtonPressed,
  kButtonReleased,
  kRecordingCancelled,
  kOutputAudioStarted,
  kResponseDone,
  kFailure,
  kRecover,
};

inline std::string_view voiceTurnStateName(VoiceTurnState state) {
  switch (state) {
    case VoiceTurnState::kUnconfigured: return "unconfigured";
    case VoiceTurnState::kConnectingWifi: return "connecting_wifi";
    case VoiceTurnState::kSyncingClock: return "syncing_clock";
    case VoiceTurnState::kConnectingProvider: return "connecting_provider";
    case VoiceTurnState::kIdle: return "idle";
    case VoiceTurnState::kRecordingAndStreaming:
      return "recording_and_streaming";
    case VoiceTurnState::kWaitingResponse: return "waiting_response";
    case VoiceTurnState::kPlaying: return "playing";
    case VoiceTurnState::kError: return "error";
  }
  return "error";
}

class VoiceTurnMachine {
 public:
  VoiceTurnState state() const { return state_; }
  std::string_view stateName() const { return voiceTurnStateName(state_); }

  bool apply(VoiceTurnEvent event) {
    if (event == VoiceTurnEvent::kConfigCleared) {
      state_ = VoiceTurnState::kUnconfigured;
      return true;
    }
    if (event == VoiceTurnEvent::kFailure) {
      if (state_ == VoiceTurnState::kUnconfigured ||
          state_ == VoiceTurnState::kError) {
        return false;
      }
      state_ = VoiceTurnState::kError;
      return true;
    }

    VoiceTurnState next = state_;
    switch (state_) {
      case VoiceTurnState::kUnconfigured:
        if (event == VoiceTurnEvent::kConfigLoaded) {
          next = VoiceTurnState::kConnectingWifi;
        }
        break;
      case VoiceTurnState::kConnectingWifi:
        if (event == VoiceTurnEvent::kWifiConnected) {
          next = VoiceTurnState::kSyncingClock;
        }
        break;
      case VoiceTurnState::kSyncingClock:
        if (event == VoiceTurnEvent::kClockSynchronized) {
          next = VoiceTurnState::kConnectingProvider;
        }
        break;
      case VoiceTurnState::kConnectingProvider:
        if (event == VoiceTurnEvent::kProviderConnected) {
          next = VoiceTurnState::kIdle;
        }
        break;
      case VoiceTurnState::kIdle:
        if (event == VoiceTurnEvent::kButtonPressed) {
          next = VoiceTurnState::kRecordingAndStreaming;
        }
        break;
      case VoiceTurnState::kRecordingAndStreaming:
        if (event == VoiceTurnEvent::kButtonReleased) {
          next = VoiceTurnState::kWaitingResponse;
        } else if (event == VoiceTurnEvent::kRecordingCancelled) {
          next = VoiceTurnState::kIdle;
        }
        break;
      case VoiceTurnState::kWaitingResponse:
        if (event == VoiceTurnEvent::kOutputAudioStarted) {
          next = VoiceTurnState::kPlaying;
        } else if (event == VoiceTurnEvent::kResponseDone) {
          next = VoiceTurnState::kIdle;
        }
        break;
      case VoiceTurnState::kPlaying:
        if (event == VoiceTurnEvent::kResponseDone) {
          next = VoiceTurnState::kIdle;
        }
        break;
      case VoiceTurnState::kError:
        if (event == VoiceTurnEvent::kRecover) {
          next = VoiceTurnState::kConnectingWifi;
        }
        break;
    }
    if (next == state_) return false;
    state_ = next;
    return true;
  }

 private:
  VoiceTurnState state_ = VoiceTurnState::kUnconfigured;
};

}  // namespace qh_voice
