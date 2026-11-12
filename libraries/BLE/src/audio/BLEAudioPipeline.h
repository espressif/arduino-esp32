/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/**
 * @file
 * @brief Internal LC3 data-path plumbing shared by BLEAudioPlayer and BLEAudioRecorder.
 *
 * Stream attachment through the stream tap, the codec task, mono LC3 codec
 * instances (one per channel, so the BAP channel-block layout stays explicit)
 * and the I2S channel. Not part of the public API.
 *
 * Threading: taps run on the BT host task and only copy SDUs into a ring and
 * wake the codec task; all codec and I2S work runs on the codec task, pinned
 * to the core that does not run the host. BLEAudioTapGate makes teardown
 * wait for taps already in flight.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_LC3_SUPPORTED

#include <atomic>
#include <cstdint>
#include <cstddef>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "BTStatus.h"
#include "audio/BLEAudioStream.h"
#include "audio/BLEAudioStreamInternal.h"
#include "audio/BLEAudioI2s.h"

static constexpr uint32_t BLE_AUDIO_CODEC_STACK = 4096;   ///< Codec task stack (bytes); LC3 state lives on the heap.
static constexpr UBaseType_t BLE_AUDIO_CODEC_PRIO = 15;   ///< Above the Arduino loop, below the BT host tasks.

/** @brief Core for the codec task: the one not running the BT host (0 on single-core). */
BaseType_t bleAudioCodecCore();

/** @brief Relaxed increment for statistics counters shared between the tap and the codec task. */
inline void bleAudioBump(std::atomic<uint32_t> &counter) {
  counter.fetch_add(1, std::memory_order_relaxed);
}

/** @brief Mono LC3 decoder with PLC enabled. */
void *bleLc3DecOpen(const BLEAudioCodecConfig &cfg);
/**
 * @brief Decode one frame, or conceal one (PLC) when @p frame is null.
 * @return PCM bytes; -(bytes needed) when @p cap is too small; 0 on error.
 */
int bleLc3Decode(void *dec, const uint8_t *frame, uint16_t len, int16_t *pcm, uint32_t cap);
/** @brief Close a decoder (NULL is ignored). */
void bleLc3DecClose(void *dec);

/** @brief Mono LC3 encoder; reports the codec's PCM input / LC3 output bytes per frame. */
void *bleLc3EncOpen(const BLEAudioCodecConfig &cfg, int *pcmBytes, int *outBytes);
/** @return true when exactly @p octets were written to @p out. */
bool bleLc3Encode(void *enc, const int16_t *pcm, int pcmBytes, uint8_t *out, uint32_t cap, uint16_t octets);
/** @brief Close an encoder (NULL is ignored). */
void bleLc3EncClose(void *enc);

/** @brief 16-bit standard-mode I2S master channel with one LC3 frame per DMA buffer. */
class BLEAudioI2sPort {
public:
  /**
   * @brief Create and configure the channel (closed first if open); it starts disabled.
   * @param tx           true for output (DOUT), false for input (DIN).
   * @param channels     1 = mono slot, 2 = stereo slots.
   * @param frameSamples Samples per channel of one LC3 frame = one DMA buffer.
   * @param dmaFrames    DMA buffers, i.e. frames of output buffering.
   */
  bool open(const BLEAudioI2sConfig &pins, bool tx, uint32_t rateHz, uint8_t channels, uint16_t frameSamples, uint8_t dmaFrames);
  /** @brief Disable and delete the channel (no-op when closed). */
  void close();
  /** @brief Enable; a TX channel is first filled with silence so the next write blocks for one frame. */
  void start();
  /** @brief Disable without deleting (no-op when not enabled). */
  void pause();
  /** @return Samples written (0 while paused). */
  size_t write(const int16_t *pcm, size_t samples, uint32_t timeoutMs);
  /** @return Samples read (0 while paused). */
  size_t read(int16_t *pcm, size_t samples, uint32_t timeoutMs);

private:
  i2s_chan_handle_t _chan = nullptr;
  bool _tx = false;
  bool _on = false;
};

/**
 * @brief Lets teardown wait for tap callbacks (BT host task) already in flight.
 *
 * A tap increments the busy count before checking any flag, so once a flag is
 * cleared and drain() returns no tap can still act on the old state.
 */
class BLEAudioTapGate {
public:
  /** @brief Let taps through. */
  void open() {
    _open.store(true);
  }
  /** @brief Stop new taps and wait for running ones to leave. */
  void close() {
    _open.store(false);
    drain();
  }
  /** @return true when the tap may run; it must then call leave(). */
  bool enter() {
    _busy.fetch_add(1);
    if (_open.load()) {
      return true;
    }
    _busy.fetch_sub(1);
    return false;
  }
  void leave() {
    _busy.fetch_sub(1);
  }
  /** @brief Wait until no tap is inside (polls every tick; taps are short). */
  void drain() const {
    while (_busy.load()) {
      vTaskDelay(1);
    }
  }

private:
  std::atomic<bool> _open{false};
  std::atomic<uint8_t> _busy{0};
};

/**
 * @brief Stream attachment, taps and codec task common to the player and the recorder.
 *
 * Up to two streams: one stream carrying 1-2 channels, or two mono streams
 * (left + right, e.g. two BISes or two CISes). The derived class sets `run`
 * and implements the per-SDU work.
 */
struct BLEAudioDataPath {
  /** Tap context: identifies which attached stream a tap call is for. */
  struct Port {
    BLEAudioDataPath *owner;
    uint8_t idx;  ///< Index into `streams`.
  };
  /** One PCM channel: the stream carrying it, its block index and the stream's channel count. */
  struct Channel {
    uint8_t stream;
    uint8_t index;
    uint8_t count;
  };

  BLEAudioStream streams[2];
  Port ports[2] = {{this, 0}, {this, 1}};
  BLEAudioStreamTap taps[2];                ///< Installed on `streams` by pointer; only rewritten while detached.
  uint8_t count = 0;                        ///< Attached streams (0-2).
  BLEAudioTapGate gate;
  std::atomic<TaskHandle_t> task{nullptr};  ///< Codec task, null when not running.
  std::atomic<bool> quit{false};            ///< Set by stop(); the task body must return.
  /** Codec task body; returns once `quit` is set. */
  void (*run)(BLEAudioDataPath *self) = nullptr;

  BLEAudioDataPath() = default;
  BLEAudioDataPath(const BLEAudioDataPath &) = delete;
  BLEAudioDataPath &operator=(const BLEAudioDataPath &) = delete;

  /**
   * @brief Bind 1-2 streams of direction @p dir and install @p tap (ctx/started/stopped are filled in).
   *
   * Streams whose direction is still unknown (not configured yet) are
   * accepted. While running, only re-attaching the same streams succeeds.
   */
  BTStatus attach(const BLEAudioStream &a, const BLEAudioStream &b, BLEAudioStream::Direction dir, const BLEAudioStreamTap &tap);
  /**
   * @brief Remove our taps (leaving other owners' taps alone) and forget the streams.
   *
   * Returns once no host-task call is still inside one of our taps.
   */
  void detach();
  /** @brief Open the taps and start the codec task named @p name (OK if already running). */
  BTStatus start(const char *name);
  /** @brief Close the taps and join the codec task (safe from the codec task itself). */
  void stop();
  bool running() const {
    return task.load() != nullptr;
  }
  /** @brief Wake the codec task (from a tap or a stream event). */
  void notify();
  /** @brief First attached stream that is streaming, -1 if none. */
  int primary() const;
  /** @brief Map PCM channels onto the streams; 0 if the layout is unsupported. */
  uint8_t mapChannels(Channel out[2]) const;

  /** @brief Stream started/stopped tap: wake the codec task to (re)build or end its session. */
  static void onWake(void *ctx);

private:
  static void entry(void *arg);
};

#endif /* BLE_AUDIO_LC3_SUPPORTED */
