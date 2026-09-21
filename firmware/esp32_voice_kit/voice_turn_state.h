#pragma once

#include <string_view>

namespace qh_voice {

enum class VoiceTurnState {
  kUnconfigured,
  kConnectingWifi,
  kSyncingClock,
  kIdle,
  kRecording,
  kTranscribing,
  kGeneratingReply,
  kSynthesizing,
  kPlaying,
  kError,
};

enum class VoiceTurnEvent {
  kConfigLoaded,
  kConfigCleared,
  kWifiConnected,
  kClockSynchronized,
  kButtonPressed,
  kButtonReleased,
  kRecordingCancelled,
  kTranscriptReady,
  kReplyReady,
  kAudioReady,
  kPlaybackFinished,
  kFailure,
  kRecover,
};

inline std::string_view voiceTurnStateName(VoiceTurnState state) {
  switch (state) {
    case VoiceTurnState::kUnconfigured:
      return "unconfigured";
    case VoiceTurnState::kConnectingWifi:
      return "connecting_wifi";
    case VoiceTurnState::kSyncingClock:
      return "syncing_clock";
    case VoiceTurnState::kIdle:
      return "idle";
    case VoiceTurnState::kRecording:
      return "recording";
    case VoiceTurnState::kTranscribing:
      return "transcribing";
    case VoiceTurnState::kGeneratingReply:
      return "generating_reply";
    case VoiceTurnState::kSynthesizing:
      return "synthesizing";
    case VoiceTurnState::kPlaying:
      return "playing";
    case VoiceTurnState::kError:
      return "error";
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
          next = VoiceTurnState::kIdle;
        }
        break;
      case VoiceTurnState::kIdle:
        if (event == VoiceTurnEvent::kButtonPressed) {
          next = VoiceTurnState::kRecording;
        }
        break;
      case VoiceTurnState::kRecording:
        if (event == VoiceTurnEvent::kButtonReleased) {
          next = VoiceTurnState::kTranscribing;
        } else if (event == VoiceTurnEvent::kRecordingCancelled) {
          next = VoiceTurnState::kIdle;
        }
        break;
      case VoiceTurnState::kTranscribing:
        if (event == VoiceTurnEvent::kTranscriptReady) {
          next = VoiceTurnState::kGeneratingReply;
        }
        break;
      case VoiceTurnState::kGeneratingReply:
        if (event == VoiceTurnEvent::kReplyReady) {
          next = VoiceTurnState::kSynthesizing;
        }
        break;
      case VoiceTurnState::kSynthesizing:
        if (event == VoiceTurnEvent::kAudioReady) {
          next = VoiceTurnState::kPlaying;
        }
        break;
      case VoiceTurnState::kPlaying:
        if (event == VoiceTurnEvent::kPlaybackFinished) {
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
