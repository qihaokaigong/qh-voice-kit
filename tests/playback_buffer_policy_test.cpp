#include <cassert>
#include <string_view>

#include "../firmware/esp32_voice_kit/playback_buffer_policy.h"

using qh_voice::PlaybackBufferPolicy;
using qh_voice::PlaybackEnqueueProgress;
using qh_voice::PlaybackFailureStage;

void waitsForPrebufferBeforePlayback() {
  PlaybackBufferPolicy policy(/*start_watermark_bytes=*/12000);
  policy.beginTurn();

  policy.onEnqueued(4000);
  assert(!policy.shouldStart());
  policy.onEnqueued(7998);
  assert(!policy.shouldStart());
  policy.onEnqueued(2);
  assert(policy.shouldStart());

  policy.onPlaybackStarted();
  policy.onConsumed(6000);
  assert(!policy.drained());
  policy.onInputDone();
  assert(!policy.drained());
  policy.onConsumed(6000);
  assert(policy.drained());
}

void startsShortFinalResponseWithoutWaitingForWatermark() {
  PlaybackBufferPolicy policy(/*start_watermark_bytes=*/12000);
  policy.beginTurn();
  policy.onEnqueued(3200);
  policy.onInputDone();

  assert(policy.shouldStart());
  policy.onPlaybackStarted();
  policy.onConsumed(3200);
  assert(policy.drained());
}

void rebufferingCountsAnUnderrunAndRequiresWatermarkAgain() {
  PlaybackBufferPolicy policy(/*start_watermark_bytes=*/12000);
  policy.beginTurn();
  policy.onEnqueued(12000);
  policy.onPlaybackStarted();
  policy.onConsumed(12000);
  policy.onUnderrun();

  assert(policy.underrunCount() == 1);
  assert(!policy.shouldStart());
  policy.onEnqueued(12000);
  assert(policy.shouldStart());
  assert(policy.maximumQueuedBytes() == 12000);
}

void exposesOnlyStablePlaybackFailureStages() {
  assert(qh_voice::playbackFailureStageName(PlaybackFailureStage::kNone) ==
         std::string_view("none"));
  assert(qh_voice::playbackFailureStageName(PlaybackFailureStage::kPcmDecode) ==
         std::string_view("pcm_decode"));
  assert(qh_voice::playbackFailureStageName(
             PlaybackFailureStage::kSpeakerStart) ==
         std::string_view("speaker_start"));
  assert(qh_voice::playbackFailureStageName(
             PlaybackFailureStage::kBufferTurnStart) ==
         std::string_view("buffer_turn_start"));
  assert(qh_voice::playbackFailureStageName(
             PlaybackFailureStage::kBufferEnqueue) ==
         std::string_view("buffer_enqueue"));
  assert(qh_voice::playbackFailureStageName(PlaybackFailureStage::kI2sWrite) ==
         std::string_view("i2s_write"));
}

void partialBufferWritesRemainPendingUntilTheWholeChunkIsQueued() {
  PlaybackEnqueueProgress progress(/*target_bytes=*/64000);

  assert(!progress.complete());
  assert(progress.remainingBytes() == 64000);
  progress.recordSent(4096);
  assert(!progress.complete());
  assert(progress.sentBytes() == 4096);
  assert(progress.remainingBytes() == 59904);
  progress.recordSent(59904);
  assert(progress.complete());
  assert(progress.sentBytes() == 64000);
  assert(progress.remainingBytes() == 0);
}

int main() {
  waitsForPrebufferBeforePlayback();
  startsShortFinalResponseWithoutWaitingForWatermark();
  rebufferingCountsAnUnderrunAndRequiresWatermarkAgain();
  exposesOnlyStablePlaybackFailureStages();
  partialBufferWritesRemainPendingUntilTheWholeChunkIsQueued();
  return 0;
}
