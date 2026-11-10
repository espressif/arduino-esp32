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

#include "audio/BLEAudioRecorder.h"
#include "audio/BLEAudioPipeline.h"

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
constexpr int32_t kAhead = 3;        // skip a tick at or above this
constexpr uint8_t kStallTicks = 8;   // no releases for this long: resynchronise
constexpr uint16_t kExtraGap = 32;   // min ticks between catch-up SDUs
constexpr uint32_t kMaxBurst = 3;    // ticks served per wake-up after a delay

}  // namespace

struct BLEAudioRecorder::Impl : BLEAudioDataPath {
  // Configuration (set before start()).
  BLEAudioI2sConfig pins;
  BLEAudioPcmSource source;            // When set, PCM comes from here instead of I2S.
  bool begun = false;
  esp_timer_handle_t timer = nullptr;  // SDU-interval tick; created once, reused across runs.

  // Shared with the sent tap / published to the API.
  std::atomic<uint32_t> released[2]{};  // SDUs the controller consumed, per stream.
  std::atomic<uint32_t> sent{0}, errors{0};
  std::atomic<uint32_t> rate{0};
  std::atomic<uint8_t> chans{0};

  // Session state, codec task only.
  BLEAudioI2sPort i2s;
  Channel ch[2] = {};
  void *enc[2] = {};       // One mono encoder per captured channel.
  uint8_t nch = 0;         // Captured channels (1 or 2).
  uint8_t frames = 1;      // LC3 frames per channel per SDU.
  uint16_t octets = 0;     // Octets per LC3 frame.
  uint16_t samples = 0;  // per channel per frame
  int pcmBytes = 0;        // Encoder PCM input per frame (one channel).
  int outBytes = 0;        // Encoder output buffer size it requires.
  uint32_t readMs = 1;     // I2S read timeout (one SDU interval).
  uint8_t *mem = nullptr;  // Backing store of pcm and both SDU buffers.
  int16_t *pcm = nullptr;  // interleaved capture frame, then a mono scratch (stereo)
  uint8_t *sdu[2] = {};    // SDU being assembled per stream.
  uint16_t sduLen[2] = {};
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
    auto *port = static_cast<Port *>(ctx);
    if (!port) {
      return;
    }
    auto *self = static_cast<Impl *>(port->owner);
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
    uint32_t intervalUs = qos.sduIntervalUs ? qos.sduIntervalUs : (uint32_t)cfg.frameDurationUs * frames;

    const char *failed = nullptr;
    for (uint8_t j = 0; !failed && j < nch; j++) {
      enc[j] = bleLc3EncOpen(cfg, &pcmBytes, &outBytes);
      failed = enc[j] ? nullptr : "LC3 encoder open";
    }
    if (!failed) {
      // pcm = one interleaved capture frame (+ a mono scratch for stereo),
      // followed by one SDU buffer per stream.
      samples = (uint16_t)(pcmBytes / sizeof(int16_t));
      size_t pcmSize = (size_t)samples * (nch == 2 ? 3 : 1) * sizeof(int16_t);
      size_t total = pcmSize;
      for (uint8_t s = 0; s < count; s++) {
        sduLen[s] = (uint16_t)(octets * (count == 1 ? nch : 1) * frames);
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
    readMs = intervalUs / 1000 ? intervalUs / 1000 : 1;
    rate.store(cfg.samplingRateHz);
    chans.store(nch);
    written[0] = written[1] = 0;
    released[0].store(0);
    released[1].store(0);
    stalled = 0;
    sinceExtra = kExtraGap;
    log_d("Recorder: %lu Hz, %u ch, %u frame(s)/SDU every %lu us", (unsigned long)cfg.samplingRateHz, nch, frames, (unsigned long)intervalUs);

    i2s.start();
    ulTaskNotifyTake(pdTRUE, 0);
    esp_timer_start_periodic(timer, intervalUs);
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
    int32_t inflight = (int32_t)(written[ref] - done);
    bool tracked = done != 0;
    if (sinceExtra < kExtraGap) {
      sinceExtra++;
    }
    if (tracked && inflight >= kAhead) {
      if (++stalled < kStallTicks) {
        return;
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
        ok = bleLc3Encode(enc[j], in, pcmBytes, sdu[c.stream] + off, sduLen[c.stream] + outBytes - off, octets) && ok;
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
      bleLc3EncClose(enc[j]);
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

BTStatus BLEAudioRecorder::begin(const BLEAudioI2sConfig &i2s) {
  Impl &d = ensure(_impl);
  if (d.running()) {
    return BTStatus::InvalidState;
  }
  if (!d.source && (i2s.bclk < 0 || i2s.ws < 0 || i2s.din < 0)) {
    log_e("Recorder: begin() needs BCLK, WS and DIN pins (or a PCM source set first)");
    return BTStatus::InvalidParam;
  }
  d.pins = i2s;
  d.begun = true;
  return BTStatus::OK;
}

void BLEAudioRecorder::end() {
  if (_impl) {
    _impl->stop();
    _impl->detach();
    _impl.reset();
  }
}

BLEAudioRecorder &BLEAudioRecorder::setPcmSource(BLEAudioPcmSource source) {
  Impl &d = ensure(_impl);
  if (!d.running()) {
    d.source = std::move(source);
  }
  return *this;
}

BTStatus BLEAudioRecorder::attach(BLEAudioStream stream, BLEAudioStream right) {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  BLEAudioStreamTap tap;
  tap.sent = Impl::onSent;
  return _impl->attach(stream, right, BLEAudioStream::Direction::Tx, tap);
}

BTStatus BLEAudioRecorder::start() {
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
  return _impl->start("ble_lc3_rec");
}

void BLEAudioRecorder::stop() {
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

#endif /* BLE_AUDIO_LC3_SUPPORTED */
