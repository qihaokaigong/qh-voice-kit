#include <cassert>

#include "../firmware/esp32_voice_kit/voice_turn_state.h"

namespace {

using qh_voice::VoiceTurnEvent;
using qh_voice::VoiceTurnMachine;
using qh_voice::VoiceTurnState;

void followsRealtimeHalfDuplexHappyPath() {
  VoiceTurnMachine machine;
  assert(machine.state() == VoiceTurnState::kUnconfigured);
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.state() == VoiceTurnState::kConnectingWifi);
  assert(machine.apply(VoiceTurnEvent::kWifiConnected));
  assert(machine.state() == VoiceTurnState::kSyncingClock);
  assert(machine.apply(VoiceTurnEvent::kClockSynchronized));
  assert(machine.state() == VoiceTurnState::kConnectingProvider);
  assert(machine.apply(VoiceTurnEvent::kProviderConnected));
  assert(machine.state() == VoiceTurnState::kIdle);
  assert(machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.state() == VoiceTurnState::kRecordingAndStreaming);
  assert(machine.apply(VoiceTurnEvent::kButtonReleased));
  assert(machine.state() == VoiceTurnState::kWaitingResponse);
  assert(machine.apply(VoiceTurnEvent::kOutputAudioStarted));
  assert(machine.state() == VoiceTurnState::kPlaying);
  assert(machine.apply(VoiceTurnEvent::kResponseDone));
  assert(machine.state() == VoiceTurnState::kIdle);
}

void canCompleteTextOnlyResponse() {
  VoiceTurnMachine machine;
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.apply(VoiceTurnEvent::kWifiConnected));
  assert(machine.apply(VoiceTurnEvent::kClockSynchronized));
  assert(machine.apply(VoiceTurnEvent::kProviderConnected));
  assert(machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.apply(VoiceTurnEvent::kButtonReleased));
  assert(machine.apply(VoiceTurnEvent::kResponseDone));
  assert(machine.state() == VoiceTurnState::kIdle);
}

void rejectsOutOfOrderEventsWithoutChangingState() {
  VoiceTurnMachine machine;
  assert(!machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.state() == VoiceTurnState::kUnconfigured);
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(!machine.apply(VoiceTurnEvent::kOutputAudioStarted));
  assert(machine.state() == VoiceTurnState::kConnectingWifi);
}

void failureRecoveryReconnectsProvider() {
  VoiceTurnMachine machine;
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.apply(VoiceTurnEvent::kWifiConnected));
  assert(machine.apply(VoiceTurnEvent::kClockSynchronized));
  assert(machine.apply(VoiceTurnEvent::kProviderConnected));
  assert(machine.apply(VoiceTurnEvent::kButtonPressed));
  assert(machine.apply(VoiceTurnEvent::kFailure));
  assert(machine.state() == VoiceTurnState::kError);
  assert(machine.apply(VoiceTurnEvent::kRecover));
  assert(machine.state() == VoiceTurnState::kConnectingWifi);
}

void exposesStableStateNames() {
  VoiceTurnMachine machine;
  assert(machine.stateName() == "unconfigured");
  assert(machine.apply(VoiceTurnEvent::kConfigLoaded));
  assert(machine.stateName() == "connecting_wifi");
}

}  // namespace

int main() {
  followsRealtimeHalfDuplexHappyPath();
  canCompleteTextOnlyResponse();
  rejectsOutOfOrderEventsWithoutChangingState();
  failureRecoveryReconnectsProvider();
  exposesStableStateNames();
  return 0;
}
