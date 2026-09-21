#include <cassert>

#include "../firmware/esp32_voice_kit/voice_turn_state.h"

namespace {

using qh_voice::VoiceTurnEvent;
using qh_voice::VoiceTurnMachine;
using qh_voice::VoiceTurnState;

void followsTheHappyPath() {
  VoiceTurnMachine machine;
  assert(machine.state() == VoiceTurnState::kUnconfigured);
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.state() == VoiceTurnState::kConnectingWifi);
  assert(machine.apply(VoiceTurnEvent::kWifiConnected));
  assert(machine.state() == VoiceTurnState::kSyncingClock);
  assert(machine.apply(VoiceTurnEvent::kClockSynchronized));
  assert(machine.state() == VoiceTurnState::kIdle);
  assert(machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.state() == VoiceTurnState::kRecording);
  assert(machine.apply(VoiceTurnEvent::kButtonReleased));
  assert(machine.state() == VoiceTurnState::kTranscribing);
  assert(machine.apply(VoiceTurnEvent::kTranscriptReady));
  assert(machine.state() == VoiceTurnState::kGeneratingReply);
  assert(machine.apply(VoiceTurnEvent::kReplyReady));
  assert(machine.state() == VoiceTurnState::kSynthesizing);
  assert(machine.apply(VoiceTurnEvent::kAudioReady));
  assert(machine.state() == VoiceTurnState::kPlaying);
  assert(machine.apply(VoiceTurnEvent::kPlaybackFinished));
  assert(machine.state() == VoiceTurnState::kIdle);
}

void rejectsOutOfOrderEventsWithoutChangingState() {
  VoiceTurnMachine machine;
  assert(!machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.state() == VoiceTurnState::kUnconfigured);

  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(!machine.apply(VoiceTurnEvent::kReplyReady));
  assert(machine.state() == VoiceTurnState::kConnectingWifi);
}

void canCancelATooShortRecording() {
  VoiceTurnMachine machine;
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.apply(VoiceTurnEvent::kWifiConnected));
  assert(machine.apply(VoiceTurnEvent::kClockSynchronized));
  assert(machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.apply(VoiceTurnEvent::kRecordingCancelled));
  assert(machine.state() == VoiceTurnState::kIdle);
}

void failureRecoveryAndConfigClearAreExplicit() {
  VoiceTurnMachine machine;
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.apply(VoiceTurnEvent::kWifiConnected));
  assert(machine.apply(VoiceTurnEvent::kClockSynchronized));
  assert(machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.apply(VoiceTurnEvent::kFailure));
  assert(machine.state() == VoiceTurnState::kError);
  assert(machine.apply(VoiceTurnEvent::kRecover));
  assert(machine.state() == VoiceTurnState::kConnectingWifi);

  assert(machine.apply(VoiceTurnEvent::kConfigCleared));
  assert(machine.state() == VoiceTurnState::kUnconfigured);
  assert(!machine.apply(VoiceTurnEvent::kFailure));
}

void exposesStableNonSecretStateNames() {
  VoiceTurnMachine machine;
  assert(machine.stateName() == "unconfigured");
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.stateName() == "connecting_wifi");
}

}  // namespace

int main() {
  followsTheHappyPath();
  rejectsOutOfOrderEventsWithoutChangingState();
  canCancelATooShortRecording();
  failureRecoveryAndConfigClearAreExplicit();
  exposesStableNonSecretStateNames();
  return 0;
}
