#pragma once

#include <ESP_I2S.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "playback_buffer_policy.h"

namespace qh_voice {

class BufferedPcmPlayback {
 public:
  explicit BufferedPcmPlayback(I2SClass& output) : output_(output) {}

  bool begin() {
    if (buffer_ != nullptr && task_ != nullptr) return true;
    buffer_ = xStreamBufferCreateWithCaps(
        kCapacityBytes, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buffer_ == nullptr) return false;
    if (xTaskCreatePinnedToCore(taskEntry, "qh_pcm_playback", kTaskStackBytes,
                                this, kTaskPriority, &task_, 0) != pdPASS) {
      vStreamBufferDeleteWithCaps(buffer_);
      buffer_ = nullptr;
      return false;
    }
    return true;
  }

  bool beginTurn() {
    if (buffer_ == nullptr || task_ == nullptr || active_.load()) {
      failure_stage_.store(PlaybackFailureStage::kBufferTurnStart);
      return false;
    }
    cancel();
    if (xStreamBufferReset(buffer_) != pdPASS) {
      failure_stage_.store(PlaybackFailureStage::kBufferTurnStart);
      return false;
    }
    input_done_.store(false);
    playing_.store(false);
    drained_.store(false);
    failed_.store(false);
    failure_stage_.store(PlaybackFailureStage::kNone);
    maximum_queued_bytes_.store(0);
    underrun_count_.store(0);
    total_enqueued_bytes_.store(0);
    active_.store(true);
    return true;
  }

  bool enqueue(const uint8_t* pcm, std::size_t size) {
    if (pcm == nullptr || size == 0 || !active_.load() ||
        input_done_.load()) {
      failure_stage_.store(PlaybackFailureStage::kBufferEnqueue);
      return false;
    }
    PlaybackEnqueueProgress progress(size);
    unsigned int no_progress_attempts = 0;
    while (!progress.complete() && active_.load() &&
           !input_done_.load() && !failed_.load()) {
      const std::size_t sent = xStreamBufferSend(
          buffer_, pcm + progress.sentBytes(), progress.remainingBytes(),
          pdMS_TO_TICKS(kEnqueueWaitMs));
      if (sent == 0) {
        if (++no_progress_attempts >= kMaxNoProgressAttempts) break;
        continue;
      }
      no_progress_attempts = 0;
      progress.recordSent(sent);
      total_enqueued_bytes_.fetch_add(sent);
      updateMaximumQueued(xStreamBufferBytesAvailable(buffer_));
    }
    if (!progress.complete()) {
      if (!failed_.load()) {
        failure_stage_.store(PlaybackFailureStage::kBufferEnqueue);
        failed_.store(true);
      }
      return false;
    }
    return true;
  }

  void finishInput() {
    if (active_.load()) input_done_.store(true);
  }

  void cancel() {
    active_.store(false);
    input_done_.store(true);
    for (int attempt = 0; attempt < 150 && busy_.load(); ++attempt) {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    playing_.store(false);
    drained_.store(false);
    if (buffer_ != nullptr && !busy_.load()) xStreamBufferReset(buffer_);
  }

  bool drained() const { return drained_.load(); }
  bool failed() const { return failed_.load(); }
  void reportFailure(PlaybackFailureStage stage) {
    failure_stage_.store(stage);
    failed_.store(stage != PlaybackFailureStage::kNone);
  }
  PlaybackFailureStage failureStage() const { return failure_stage_.load(); }
  std::size_t maximumQueuedBytes() const {
    return maximum_queued_bytes_.load();
  }
  std::size_t underrunCount() const { return underrun_count_.load(); }
  std::size_t totalEnqueuedBytes() const {
    return total_enqueued_bytes_.load();
  }

 private:
  static constexpr std::size_t kCapacityBytes = 192 * 1024;
  static constexpr std::size_t kStartWatermarkBytes = 12 * 1024;
  static constexpr std::size_t kWriteChunkBytes = 1920;
  static constexpr uint32_t kEnqueueWaitMs = 250;
  static constexpr unsigned int kMaxNoProgressAttempts = 8;
  static constexpr uint32_t kTaskStackBytes = 6144;
  static constexpr UBaseType_t kTaskPriority = 2;

  static void taskEntry(void* context) {
    static_cast<BufferedPcmPlayback*>(context)->taskLoop();
  }

  void updateMaximumQueued(std::size_t queued) {
    std::size_t maximum = maximum_queued_bytes_.load();
    while (queued > maximum &&
           !maximum_queued_bytes_.compare_exchange_weak(maximum, queued)) {
    }
  }

  void finishDrain() {
    playing_.store(false);
    drained_.store(true);
    active_.store(false);
  }

  void taskLoop() {
    uint8_t chunk[kWriteChunkBytes];
    while (true) {
      if (!active_.load()) {
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }

      const std::size_t available = xStreamBufferBytesAvailable(buffer_);
      if (!playing_.load()) {
        if (available >= kStartWatermarkBytes ||
            (input_done_.load() && available > 0)) {
          playing_.store(true);
        } else if (input_done_.load() && available == 0) {
          finishDrain();
          continue;
        } else {
          vTaskDelay(pdMS_TO_TICKS(2));
          continue;
        }
      }

      busy_.store(true);
      const std::size_t received = xStreamBufferReceive(
          buffer_, chunk, sizeof(chunk), pdMS_TO_TICKS(20));
      if (!active_.load()) {
        busy_.store(false);
        continue;
      }
      if (received == 0) {
        busy_.store(false);
        if (input_done_.load() && xStreamBufferBytesAvailable(buffer_) == 0) {
          finishDrain();
        } else {
          playing_.store(false);
          underrun_count_.fetch_add(1);
        }
        continue;
      }

      const std::size_t written = output_.write(chunk, received);
      busy_.store(false);
      if (written != received) {
        failure_stage_.store(PlaybackFailureStage::kI2sWrite);
        failed_.store(true);
        active_.store(false);
        playing_.store(false);
        continue;
      }
      if (input_done_.load() && xStreamBufferBytesAvailable(buffer_) == 0) {
        finishDrain();
      }
    }
  }

  I2SClass& output_;
  StreamBufferHandle_t buffer_ = nullptr;
  TaskHandle_t task_ = nullptr;
  std::atomic<bool> active_{false};
  std::atomic<bool> input_done_{false};
  std::atomic<bool> playing_{false};
  std::atomic<bool> drained_{false};
  std::atomic<bool> failed_{false};
  std::atomic<PlaybackFailureStage> failure_stage_{PlaybackFailureStage::kNone};
  std::atomic<bool> busy_{false};
  std::atomic<std::size_t> maximum_queued_bytes_{0};
  std::atomic<std::size_t> underrun_count_{0};
  std::atomic<std::size_t> total_enqueued_bytes_{0};
};

}  // namespace qh_voice
