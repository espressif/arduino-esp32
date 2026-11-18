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

#include "core/BLEGuards.h"
#if BLE_AUDIO_LC3_SUPPORTED

#include "BLEAudioRecorder.h"
#include "BLEAudioPipeline.h"

#include <cstdlib>
#include <cstring>
#include "esp_timer.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioRecorder.cpp
 * @brief I2S or a PCM source -> LC3 encode -> one SDU per interval on 1-2 Tx streams.
 *
 * A periodic esp_timer at the SDU interval wakes the codec task, which
 * captures one SDU's worth of PCM, encodes it per channel into the BAP block
 * layout and writes it to each streaming stream. The stream's sent callback
 * (BT host task) counts SDUs the controller released, so the task can keep a
 * small, steady backlog in the controller despite clock drift between the
 * timer and the ISO interval (see tick()).
 *
 * API contract is documented on the declarations in `BLEAudioRecorder.h`; the
 * definitions below carry implementation notes only.
 */

namespace {

// Pacing keeps 1-2 SDUs queued in the controller (written minus released).
// Tuned for 10 ms SDU intervals; at 7.5 ms the same tick counts cover
// proportionally less time, which only makes stall detection faster.
constexpr int32_t kAhead = 3;       // skip a tick at or above this backlog (clock drift between timer and ISO)
constexpr uint8_t kStallTicks = 8;  // no releases for this long (~80 ms): assume drained and resync
constexpr uint16_t kExtraGap = 32;  // min ticks between catch-up SDUs so one late release never bursts
constexpr uint32_t kMaxBurst = 3;   // ticks served per wake-up after the codec task was busy

}  // namespace

#if BLE_AUDIO_VALIDATION_HOOKS
namespace {
std::atomic<bool> s_blockReleases{false};
}  // namespace

extern "C" void bleAudioValidationBlockReleases(bool block) {
  s_blockReleases.store(block, std::memory_order_relaxed);
}
#endif

struct BLEAudioRecorder::Impl : BLEAudioDataPath {
  // Configuration (set before start()).
  BLEAudioPCMSource source;            // When set, PCM comes from here instead of I2S.
  esp_timer_handle_t timer = nullptr;  // SDU-interval tick; created once, reused across runs.

  // Shared with the sent tap / published to the API.
  std::atomic<uint32_t> released[2]{};  // SDUs the controller consumed, per stream.
  std::atomic<uint32_t> sent{0}, errors{0}, stalls{0};
  std::atomic<uint32_t> rate{0};
  std::atomic<uint8_t> chans{0};

  // Session state, codec task only.
  BLEAudioI2SPort i2s;
  Channel ch[2] = {};
  void *enc[2] = {};       // One mono encoder per captured channel.
  uint8_t nch = 0;         // Captured channels (1 or 2).
  uint8_t frames = 1;      // LC3 frames per channel per SDU.
  uint16_t octets = 0;     // Octets per LC3 frame.
  uint16_t samples = 0;    // per channel per frame
  int pcmBytes = 0;        // Encoder PCM input per frame (one channel).
  int outBytes = 0;        // Encoder output buffer size it requires.
  uint32_t readMs = 1;     // I2S read timeout (one SDU interval).
  uint8_t *mem = nullptr;  // Backing store of pcm and both SDU buffers.
  int16_t *pcm = nullptr;  // interleaved capture frame, then a mono scratch (stereo)
  uint8_t *sdu[2] = {};    // SDU being assembled per stream.
  size_t sduLen[2] = {};
  uint32_t written[2] = {};  // SDUs accepted by write(), per stream.
  uint8_t stalled = 0;       // Consecutive ticks skipped because the controller is ahead.
  uint16_t sinceExtra = 0;   // Ticks since the last catch-up SDU.

  Impl() {
    run = [](BLEAudioDataPath *self) {
      static_cast<Impl *>(self)->loop();
    };
  }
  ~Impl() {
    stop();
    detach();
    if (timer) {
      esp_timer_delete(timer);
    }
  }

  /** Stream sent tap (BT host task): count one released SDU for the pacing. */
  static void onSent(void *ctx) {
    auto *port = static_cast<Port *>(ctx);  // Never NULL by construction.
    auto *self = static_cast<Impl *>(port->owner);
#if BLE_AUDIO_VALIDATION_HOOKS
    if (s_blockReleases.load(std::memory_order_relaxed)) {
      return;  // Simulated stall: releases stop arriving until unblocked.
    }
#endif
    if (self->gate.enter()) {
      bleAudioBump(self->released[port->idx]);
      self->gate.leave();
    }
  }

  /** Create the tick timer (once); each expiry only notifies the codec task. */
  bool createTimer() {
    if (timer) {
      return true;
    }
    esp_timer_create_args_t args = {};
    args.callback = [](void *arg) {
      static_cast<Impl *>(arg)->notify();
    };
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "ble_lc3_rec";
    return esp_timer_create(&args, &timer) == ESP_OK;
  }

  /** Codec task body: run a session whenever a stream streams, sleep otherwise. */
  void loop() {
    while (!quit.load()) {
      int p = primary();
      if (p < 0 || !session(p)) {
        if (quit.load()) {
          return;
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      }
    }
  }

  /**
   * Build a session from stream @p p's codec and QoS, send until every stream
   * stops, then tear it down. Timer wake-ups missed while the task was busy
   * are served in bursts of at most kMaxBurst. @return false when the session
   * could not be built.
   */
  bool session(int p) {
    BLEAudioCodecConfig cfg = streams[p].codecConfig();
    BLEAudioQos qos = streams[p].qos();
    nch = mapChannels(ch);
    if (!nch) {
      log_w("Recorder: unsupported channel layout (%u stream(s); use one stream of 1-2 channels or two mono streams)", count);
      return false;
    }
    frames = cfg.framesPerSdu ? cfg.framesPerSdu : 1;
    octets = cfg.octetsPerFrame;
    // Real SDU time (see Player::session): the negotiated QoS wins; the codec
    // config is only the fallback for unconfigured streams. A zero interval
    // (malformed custom config) would fault the timer below.
    uint32_t intervalUs = qos.sduIntervalUs ? qos.sduIntervalUs : (uint32_t)cfg.frameDurationUs * frames;
    if (!intervalUs) {
      log_e("Recorder: invalid SDU interval (0 us)");
      close();
      return false;
    }

    const char *failed = nullptr;
    for (uint8_t j = 0; !failed && j < nch; j++) {
      enc[j] = bleLC3EncOpen(cfg, &pcmBytes, &outBytes);
      failed = enc[j] ? nullptr : "LC3 encoder open";
    }
    if (!failed && (pcmBytes <= 0 || outBytes <= 0)) {
      failed = "LC3 encoder frame size";
    }
    if (!failed) {
      // pcm = one interleaved capture frame (+ a mono scratch for stereo),
      // followed by one SDU buffer per stream.
      samples = (uint16_t)(pcmBytes / sizeof(int16_t));
      if (!samples) {
        failed = "invalid PCM frame size";
      }
    }
    if (!failed) {
      size_t pcmSize = (size_t)samples * (nch == 2 ? 3 : 1) * sizeof(int16_t);
      size_t total = pcmSize;
      for (uint8_t s = 0; s < count; s++) {
        sduLen[s] = (size_t)octets * (count == 1 ? nch : 1) * frames;
        if (!sduLen[s] || sduLen[s] > UINT16_MAX) {
          failed = "invalid SDU size";
          break;
        }
        total += sduLen[s] + outBytes;  // tail room: the encoder wants a full output frame
      }
      mem = static_cast<uint8_t *>(malloc(total));
      if (mem) {
        pcm = reinterpret_cast<int16_t *>(mem);
        uint8_t *q = mem + pcmSize;
        for (uint8_t s = 0; s < count; s++) {
          sdu[s] = q;
          q += sduLen[s] + outBytes;
        }
      } else {
        failed = "buffer allocation";
      }
    }
    // DMA: one frame per buffer, SDU frames + 3 of headroom.
    if (!failed && !source && !i2s.open(pins, false, cfg.samplingRateHz, nch, samples, frames + 3)) {
      failed = "I2S input open";
    }
    if (failed) {
      log_e("Recorder: %s failed (%lu Hz, %u ch, %u-octet frames)", failed, (unsigned long)cfg.samplingRateHz, nch, octets);
      close();
      return false;
    }
    // One I2S read is one real PCM frame (44.1 kHz is not 7.5/10 ms exactly).
    // When the QoS was missing above, the timer below should have used this
    // real frame time instead of the nominal codec duration.
    uint32_t frameUs = (uint32_t)((uint64_t)samples * 1000000u / cfg.samplingRateHz);
    if (!qos.sduIntervalUs) {
      intervalUs = frameUs * frames;
    }
    readMs = (frameUs + 1999u) / 1000u;
    rate.store(cfg.samplingRateHz);
    chans.store(nch);
    written[0] = written[1] = 0;
    released[0].store(0);
    released[1].store(0);
    stalled = 0;
    sinceExtra = kExtraGap;
    log_d("Recorder: %lu Hz, %u ch, %u frame(s)/SDU every %lu us", (unsigned long)cfg.samplingRateHz, nch, frames, (unsigned long)intervalUs);

    if (!source && !i2s.start()) {
      log_e("Recorder: I2S input could not be enabled");
      close();
      return false;
    }
    ulTaskNotifyTake(pdTRUE, 0);
    esp_err_t timerErr = esp_timer_start_periodic(timer, intervalUs);
    if (timerErr != ESP_OK) {
      log_e("Recorder: esp_timer_start_periodic failed (%d)", timerErr);
      i2s.pause();
      close();
      return false;
    }
    for (;;) {
      uint32_t ticks = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
      int ref = primary();
      if (quit.load() || ref < 0) {
        break;
      }
      for (ticks = (ticks > kMaxBurst) ? kMaxBurst : ticks; ticks; ticks--) {
        tick((uint8_t)ref);
      }
    }
    esp_timer_stop_blocking(timer, portMAX_DELAY);
    close();
    return true;
  }

  /**
   * One timer period: send one SDU, skip if the controller is ahead, add one if it ran dry.
   *
   * @p ref is the stream whose releases drive the pacing. Until its first
   * release arrives the backlog is unknown and every tick sends exactly one
   * SDU. When skipping lasts kStallTicks (releases stopped), the backlog is
   * assumed drained and counting restarts. Catch-up SDUs are rate limited to
   * one per kExtraGap ticks so a late release does not cause a burst.
   */
  void tick(uint8_t ref) {
    uint32_t done = released[ref].load(std::memory_order_relaxed);
    // Signed math: after a resync `done` can exceed `written`; unsigned
    // subtraction would wrap and the narrowing cast back is fragile.
    int32_t inflight = (int32_t)((int64_t)written[ref] - (int64_t)done);
    bool tracked = done != 0;
    if (sinceExtra < kExtraGap) {
      sinceExtra++;
    }
    if (tracked && inflight >= kAhead) {
      if (++stalled < kStallTicks) {
        return;
      }
      if (stalled == kStallTicks) {
        // Releases stopped but the stream still runs (stalled ISO, link loss
        // without an event): report once per episode instead of spinning
        // silently, then assume drained and keep capturing.
        log_w("Recorder: no controller releases for %u ticks; resyncing (TX may be stalled)", kStallTicks);
        bleAudioBump(stalls);
      }
      written[ref] = done;
    }
    stalled = 0;
    produce();
    if (tracked && inflight <= 0 && sinceExtra >= kExtraGap) {
      sinceExtra = 0;
      produce();
    }
  }

  /**
   * Capture, encode and send one SDU on every streaming stream. A short read
   * is padded with silence so the SDU is always complete; an encode failure
   * counts as a send error and nothing is sent for that interval.
   */
  void produce() {
    const size_t want = (size_t)samples * nch;
    int16_t *mono = pcm + want;
    bool ok = true;
    for (uint8_t b = 0; b < frames; b++) {
      size_t got = source ? source(pcm, want) : i2s.read(pcm, want, readMs);
      if (got < want) {
        memset(pcm + got, 0, (want - got) * sizeof(int16_t));
      }
      for (uint8_t j = 0; j < nch; j++) {
        const Channel &c = ch[j];
        const int16_t *in = pcm;
        if (nch == 2) {
          for (size_t i = 0; i < samples; i++) {
            mono[i] = pcm[2 * i + j];
          }
          in = mono;
        }
        size_t off = (size_t)(b * c.count + c.index) * octets;
        ok = bleLC3Encode(enc[j], in, pcmBytes, sdu[c.stream] + off, sduLen[c.stream] + outBytes - off, octets) && ok;
      }
    }
    for (uint8_t s = 0; s < count; s++) {
      if (!streams[s].isStreaming()) {
        continue;
      }
      if (ok && streams[s].write(sdu[s], sduLen[s])) {
        bleAudioBump(sent);
        written[s]++;
      } else {
        bleAudioBump(errors);
      }
    }
  }

  /** Tear the session down (the timer is already stopped; the tap touches no session state). */
  void close() {
    rate.store(0);
    chans.store(0);
    i2s.close();
    for (uint8_t j = 0; j < 2; j++) {
      bleLC3EncClose(enc[j]);
      enc[j] = nullptr;
    }
    free(mem);
    mem = nullptr;
    pcm = nullptr;
    sdu[0] = sdu[1] = nullptr;
  }
};

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

// The Impl is created lazily so setters may be called before begin().
static BLEAudioRecorder::Impl &ensure(std::shared_ptr<BLEAudioRecorder::Impl> &impl) {
  if (!impl) {
    impl = std::make_shared<BLEAudioRecorder::Impl>();
  }
  return *impl;
}

BLEAudioRecorder::BLEAudioRecorder() = default;

BLEAudioRecorder::operator bool() const {
  return _impl && _impl->begun;
}

BTStatus BLEAudioRecorder::begin() {
  Impl &d = ensure(_impl);
  return d.beginPcm(static_cast<bool>(d.source), "Recorder", "begin(bclk, ws, din)");
}

BTStatus BLEAudioRecorder::begin(int bclk, int ws, int din, int mclk, int port) {
  Impl &d = ensure(_impl);
  return d.beginI2S(static_cast<bool>(d.source), bclk, ws, din, /*tx=*/false, mclk, port, "Recorder");
}

void BLEAudioRecorder::end() {
  BLEAudioDataPath::endHandle(_impl, "Recorder");
}

BTStatus BLEAudioRecorder::setPCMSource(BLEAudioPCMSource source) {
  Impl &d = ensure(_impl);
  if (d.begun || d.running()) {
    log_w("Recorder: setPCMSource() ignored after begin(); end() first (applies to the next begin)");
    return BTStatus::InvalidState;
  }
  d.source = std::move(source);
  return BTStatus::OK;
}

BTStatus BLEAudioRecorder::attach(BLEAudioStream stream) {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  if (!stream) {
    return BTStatus::InvalidParam;
  }
  BLEAudioStreamTap tap;
  tap.sent = Impl::onSent;
  return _impl->attach(stream, BLEAudioStream(), BLEAudioStream::Direction::Tx, tap);
}

BTStatus BLEAudioRecorder::attach(BLEAudioStream stream, BLEAudioStream right) {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  if (!stream || !right) {
    return BTStatus::InvalidParam;
  }
  BLEAudioStreamTap tap;
  tap.sent = Impl::onSent;
  return _impl->attach(stream, right, BLEAudioStream::Direction::Tx, tap);
}

BTStatus BLEAudioRecorder::startCapture() {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  if (_impl->running()) {
    return BTStatus::OK;
  }
  if (!_impl->createTimer()) {
    log_e("Recorder: creating the SDU timer failed");
    return BTStatus::NoMemory;
  }
  _impl->sent.store(0);
  _impl->errors.store(0);
  _impl->stalls.store(0);
  return _impl->start("ble_lc3_rec");
}

void BLEAudioRecorder::stopCapture() {
  if (_impl) {
    _impl->stop();
  }
}

bool BLEAudioRecorder::isRunning() const {
  return _impl && _impl->running();
}

uint8_t BLEAudioRecorder::channels() const {
  return _impl ? _impl->chans.load() : 0;
}

uint32_t BLEAudioRecorder::sampleRate() const {
  return _impl ? _impl->rate.load() : 0;
}

uint32_t BLEAudioRecorder::sdusSent() const {
  return _impl ? _impl->sent.load(std::memory_order_relaxed) : 0;
}

uint32_t BLEAudioRecorder::sendErrors() const {
  return _impl ? _impl->errors.load(std::memory_order_relaxed) : 0;
}

uint32_t BLEAudioRecorder::txStalls() const {
  return _impl ? _impl->stalls.load(std::memory_order_relaxed) : 0;
}

#endif /* BLE_AUDIO_LC3_SUPPORTED */
