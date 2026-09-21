#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace qh_voice {

enum class PlaybackFailureStage {
  kNone,
  kPcmDecode,
  kSpeakerStart,
  kBufferTurnStart,
  kBufferEnqueue,
  kI2sWrite,
};

class PlaybackEnqueueProgress {
 public:
  explicit PlaybackEnqueueProgress(std::size_t target_bytes)
      : target_bytes_(target_bytes) {}

  void recordSent(std::size_t bytes) {
    sent_bytes_ = std::min(target_bytes_, sent_bytes_ + bytes);
  }

  bool complete() const { return sent_bytes_ == target_bytes_; }
  std::size_t sentBytes() const { return sent_bytes_; }
  std::size_t remainingBytes() const { return target_bytes_ - sent_bytes_; }

 private:
  std::size_t target_bytes_;
  std::size_t sent_bytes_ = 0;
};

inline std::string_view playbackFailureStageName(PlaybackFailureStage stage) {
  switch (stage) {
    case PlaybackFailureStage::kNone: return "none";
    case PlaybackFailureStage::kPcmDecode: return "pcm_decode";
    case PlaybackFailureStage::kSpeakerStart: return "speaker_start";
    case PlaybackFailureStage::kBufferTurnStart: return "buffer_turn_start";
    case PlaybackFailureStage::kBufferEnqueue: return "buffer_enqueue";
    case PlaybackFailureStage::kI2sWrite: return "i2s_write";
  }
  return "unknown";
}

class PlaybackBufferPolicy {
 public:
  explicit PlaybackBufferPolicy(std::size_t start_watermark_bytes)
      : start_watermark_bytes_(start_watermark_bytes) {}

  void beginTurn() {
    queued_bytes_ = 0;
    maximum_queued_bytes_ = 0;
    underrun_count_ = 0;
    input_done_ = false;
    playing_ = false;
  }

  void onEnqueued(std::size_t bytes) {
    queued_bytes_ += bytes;
    maximum_queued_bytes_ = std::max(maximum_queued_bytes_, queued_bytes_);
  }

  bool shouldStart() const {
    return !playing_ && queued_bytes_ > 0 &&
           (queued_bytes_ >= start_watermark_bytes_ || input_done_);
  }

  void onPlaybackStarted() { playing_ = true; }

  void onConsumed(std::size_t bytes) {
    queued_bytes_ = bytes >= queued_bytes_ ? 0 : queued_bytes_ - bytes;
  }

  void onInputDone() { input_done_ = true; }

  void onUnderrun() {
    playing_ = false;
    ++underrun_count_;
  }

  bool drained() const { return input_done_ && queued_bytes_ == 0; }
  std::size_t queuedBytes() const { return queued_bytes_; }
  std::size_t maximumQueuedBytes() const { return maximum_queued_bytes_; }
  std::size_t underrunCount() const { return underrun_count_; }

 private:
  std::size_t start_watermark_bytes_;
  std::size_t queued_bytes_ = 0;
  std::size_t maximum_queued_bytes_ = 0;
  std::size_t underrun_count_ = 0;
  bool input_done_ = false;
  bool playing_ = false;
};

}  // namespace qh_voice
