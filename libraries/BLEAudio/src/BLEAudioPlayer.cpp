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

#include "BLEAudioPlayer.h"
#include "BLEAudioPipeline.h"

#include <cstdlib>
#include <cstring>
#include "freertos/queue.h"
#include "esp32-hal-log.h"
#include "esp_timer.h"

/**
 * @file BLEAudioPlayer.cpp
 * @brief Received LC3 SDUs -> jitter buffer -> LC3 decode (with PLC) -> I2S or a PCM sink.
 *
 * The stream tap (BT host task) copies each SDU into a per-stream ring; the
 * codec task waits until the ring holds the jitter depth, then decodes one SDU
 * per interval, paced by the blocking I2S write or, for a PCM sink, by an
 * absolute real-time deadline. A session (buffers, decoders, I2S) is built
 * when a stream starts and
 * torn down when every stream stopped, so a codec change between sessions is
 * picked up automatically.
 *
 * API contract is documented on the declarations in `BLEAudioPlayer.h`; the
 * definitions below carry implementation notes only.
 */

namespace {

constexpr uint8_t kMaxDepth = 16;  // Upper bound on the jitter depth, in SDUs.
// Each ring slot starts with the SDU length; 0 means "conceal this SDU".
constexpr uint16_t kSlotHeader = sizeof(uint16_t);

}  // namespace

struct BLEAudioPlayer::Impl : BLEAudioDataPath {
  // Single producer (tap) / single consumer (codec task). The queue carries
  // filled slot indexes; `slots` = queue length + 1 so the slot the consumer is
  // still decoding is never overwritten.
  struct Ring {
    uint8_t *base = nullptr;        // `slots` x `stride` bytes inside `mem`.
    QueueHandle_t queue = nullptr;  // Filled slot indexes, oldest first.
    size_t expect = 0;              // Valid SDU length; any other length is treated as lost.
    uint8_t head = 0;               // Next slot the producer writes.
  };

  // Configuration (set before start()).
  BLEAudioPCMSink sink;       // When set, PCM goes here instead of I2S.
  uint8_t depthOverride = 0;  // Jitter depth in SDUs, 0 = from the presentation delay.

  // Jitter buffer, shared with the tap.
  Ring ring[2];
  size_t stride = 0;  // Bytes per slot: header + largest SDU, 4-byte aligned.
  uint8_t slots = 0;
  std::atomic<bool> ready{false};  // Rings valid; the tap drops SDUs while false.

  // Session state, codec task only.
  BLEAudioI2SPort i2s;
  Channel ch[2] = {};
  void *dec[2] = {};       // One mono decoder per output channel.
  uint8_t nch = 0;         // Decoded channels (1 or 2).
  uint8_t outCh = 0;       // Output channels: nch for a sink, always 2 for I2S (mono is duplicated).
  uint8_t frames = 1;      // LC3 frames per channel per SDU.
  uint16_t octets = 0;     // Octets per LC3 frame.
  int16_t *pcm = nullptr;  // mono scratch, then the interleaved output frame
  uint32_t pcmCap = 0;     // samples per channel
  uint8_t *mem = nullptr;  // Backing store of every ring.

  // Published to the API.
  std::atomic<uint32_t> rate{0};
  std::atomic<uint8_t> chans{0};
  std::atomic<uint32_t> received{0}, dropped{0}, lost{0}, plc{0}, underruns{0}, late{0};

  Impl() {
    run = [](BLEAudioDataPath *self) {
      static_cast<Impl *>(self)->loop();
    };
  }
  ~Impl() {
    stop();
    detach();
  }

  /** Stream tap (BT host task, once per SDU interval): gate, then push. */
  static void onRecv(void *ctx, const ble_audio_recv_info_t *info, const uint8_t *data, uint16_t len) {
    auto *port = static_cast<Port *>(ctx);  // Never NULL by construction.
    auto *self = static_cast<Impl *>(port->owner);
    if (!self->gate.enter()) {
      return;
    }
    if (self->ready.load()) {
      self->push(port->idx, info, data, len);
    }
    self->gate.leave();
  }

  /**
   * Queue one SDU of stream @p s. A full ring drops the new SDU (the codec
   * task is behind); an invalid, lost or wrongly sized SDU is queued empty so
   * the decoder conceals it and timing is preserved.
   */
  void push(uint8_t s, const ble_audio_recv_info_t *info, const uint8_t *data, uint16_t len) {
    Ring &r = ring[s];
    bleAudioBump(received);
    if (uxQueueMessagesWaiting(r.queue) >= slots - 1u) {
      bleAudioBump(dropped);
      return;
    }
    uint8_t slot = r.head;
    r.head = (slot + 1 == slots) ? 0 : slot + 1;
    uint8_t *e = r.base + (size_t)slot * stride;
    uint16_t n = 0;
    if ((!info || info->status == BLE_AUDIO_SDU_VALID) && data && len == r.expect) {
      memcpy(e + kSlotHeader, data, len);
      n = len;
    } else {
      bleAudioBump(lost);
    }
    memcpy(e, &n, sizeof(n));
    xQueueSend(r.queue, &slot, 0);
    notify();
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
   * Build a session from stream @p p's codec and QoS, play until every stream
   * stops, then tear it down.
   *
   * Jitter depth = presentation delay / SDU interval, clamped to 2..kMaxDepth
   * (or the override). The ring holds twice the depth so a burst after a
   * stall is absorbed. @return false when the session could not be built.
   */
  bool session(int p) {
    BLEAudioCodecConfig cfg = streams[p].codecConfig();
    BLEAudioQos qos = streams[p].qos();
    nch = mapChannels(ch);
    if (!nch) {
      log_w("Player: unsupported channel layout (%u stream(s); use one stream of 1-2 channels or two mono streams)", count);
      return false;
    }
    frames = cfg.framesPerSdu ? cfg.framesPerSdu : 1;
    octets = cfg.octetsPerFrame;
    if (!cfg.samplesPerFrame()) {
      log_e("Player: invalid stream sizes (%u-octet frames)", octets);
      return false;
    }
    // Real SDU time, not the nominal LC3 duration: 44.1 kHz frames are
    // 8163/10884 us on air (see samplesPerFrame()). The negotiated QoS wins;
    // the codec config is only the fallback for unconfigured streams.
    const uint32_t sduUs = (uint32_t)((uint64_t)cfg.samplesPerFrame() * frames * 1000000u / cfg.samplingRateHz);
    uint32_t intervalUs = qos.sduIntervalUs ? qos.sduIntervalUs : sduUs;
    if (!intervalUs) {
      // Both the QoS and the codec config carry a zero interval (a malformed
      // custom config); the jitter-depth division below would fault.
      log_e("Player: invalid SDU interval (0 us)");
      return false;
    }
    const uint32_t reqDepth = depthOverride ? depthOverride : (qos.presentationDelayUs + intervalUs - 1) / intervalUs;
    uint32_t depth = reqDepth;
    if (depth < 2) {
      depth = 2;
    }
    if (depth > kMaxDepth) {
      log_w(
        "Player: jitter depth %lu clamped to %u SDUs; %lu us of presentation delay do not fit and will underrun", (unsigned long)reqDepth, kMaxDepth,
        (unsigned long)qos.presentationDelayUs
      );
      depth = kMaxDepth;
    }
    uint8_t queueLen = (uint8_t)(depth * 2);
    slots = queueLen + 1;
    size_t perStream = (size_t)octets * (count == 1 ? nch : 1) * frames;
    if (!perStream || perStream > UINT16_MAX) {
      log_e("Player: invalid stream sizes (%u octets, %u frame(s)/SDU)", octets, frames);
      return false;
    }
    stride = (kSlotHeader + perStream + 3) & ~size_t(3);
    pcmCap = cfg.samplesPerFrame();
    outCh = sink ? nch : 2;

    // pcm = one mono scratch frame + one interleaved output frame.
    mem = static_cast<uint8_t *>(malloc((size_t)count * slots * stride));
    pcm = static_cast<int16_t *>(malloc((size_t)pcmCap * (1 + outCh) * sizeof(int16_t)));
    const char *failed = (mem && pcm) ? nullptr : "buffer allocation";
    for (uint8_t s = 0; !failed && s < count; s++) {
      ring[s].base = mem + (size_t)s * slots * stride;
      ring[s].queue = xQueueCreate(queueLen, sizeof(uint8_t));
      ring[s].expect = perStream;
      ring[s].head = 0;
      failed = ring[s].queue ? nullptr : "queue allocation";
    }
    for (uint8_t j = 0; !failed && j < nch; j++) {
      dec[j] = bleLC3DecOpen(cfg);
      failed = dec[j] ? nullptr : "LC3 decoder open";
    }
    // DMA: one frame per buffer, SDU frames + 2 of headroom.
    if (!failed && !sink && !i2s.open(pins, true, cfg.samplingRateHz, 2, (uint16_t)pcmCap, frames + 2)) {
      failed = "I2S output open";
    }
    if (failed) {
      log_e("Player: %s failed (%lu Hz, %u ch, %u-octet frames)", failed, (unsigned long)cfg.samplingRateHz, nch, octets);
      close();
      return false;
    }
    rate.store(cfg.samplingRateHz);
    chans.store(outCh);
    ready.store(true);
    log_d("Player: %lu Hz, %u ch, %u frame(s)/SDU, depth %lu", (unsigned long)cfg.samplingRateHz, nch, frames, (unsigned long)depth);

    // Arrival-driven waits (a queued SDU wakes the loop at once); the bounds
    // below only matter when the stream goes quiet. Both use the real SDU time.
    TickType_t wait = pdMS_TO_TICKS((intervalUs + 999) / 1000);
    // One I2S DMA buffer holds one LC3 frame; the write timeout is one real frame.
    const uint32_t frameUs = sduUs / frames;
    uint32_t writeMs = (frameUs + 1999u) / 1000u;
    play(p, (uint8_t)depth, wait ? wait : 1, writeMs ? writeMs : 1, sduUs);
    close();
    return true;
  }

  /**
   * Buffer to @p depth, then render one SDU per interval until no stream
   * delivers anything for 2 x depth intervals, then rebuffer. Returns when
   * quitting or when no stream streams any more.
   *
   * @param wait    Max wait for the primary stream's next SDU (one interval).
   * @param writeMs I2S write timeout (one frame).
   * @param sduUs   Real-time duration of one SDU, used to pace a PCM sink.
   */
  void play(int p, uint8_t depth, TickType_t wait, uint32_t writeMs, uint32_t sduUs) {
    // Without the blocking I2S write, a burst of queued SDUs would reach the
    // PCM sink back to back; pace renders to the stream's real-time rate.
    int64_t nextRenderUs = 0;
    while (!quit.load()) {
      ulTaskNotifyTake(pdTRUE, 0);
      while (uxQueueMessagesWaiting(ring[p].queue) < depth) {
        int cur = primary();
        if (quit.load() || cur < 0 || cur != p) {
          return;
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
      }
      if (!sink && !i2s.start()) {
        log_e("Player: I2S output could not be enabled");
        return;
      }
      // The primary stream paces the loop; after 2 x depth empty intervals, rebuffer.
      uint8_t idle = 0;
      while (!quit.load() && idle < depth * 2) {
        // Rebuild the session when the stream that was pacing us stops; the
        // other attached stream may still be streaming.
        if (primary() != p) {
          i2s.pause();
          return;
        }
        const uint8_t *sdu[2] = {nullptr, nullptr};
        bool got = false;
        for (uint8_t s = 0; s < count; s++) {
          uint8_t slot;
          // Wait up to one interval for every streaming stream, not only the
          // primary: two mono streams must stay aligned.
          TickType_t streamWait = streams[s].isStreaming() ? wait : 0;
          if (xQueueReceive(ring[s].queue, &slot, streamWait) == pdTRUE) {
            const uint8_t *e = ring[s].base + (size_t)slot * stride;
            uint16_t n;
            memcpy(&n, e, sizeof(n));
            sdu[s] = n ? e + kSlotHeader : nullptr;
            got = true;
          } else if (streams[s].isStreaming()) {
            bleAudioBump(underruns);
          }
        }
        if (!got && primary() < 0) {
          i2s.pause();
          return;
        }
        idle = got ? 0 : idle + 1;
        render(sdu, writeMs, sduUs);
        if (sink) {
          const int64_t now = esp_timer_get_time();
          if (nextRenderUs == 0 || now > nextRenderUs + 3 * (int64_t)sduUs) {
            // First SDU, or we fell far behind: restart the timeline instead
            // of rendering a catch-up burst. Counted: tick quantization, not
            // data loss (every queued SDU still renders, just late).
            if (nextRenderUs != 0) {
              bleAudioBump(late);
            }
            nextRenderUs = now;
          }
          nextRenderUs += sduUs;
          const int64_t remaining = nextRenderUs - esp_timer_get_time();
          if (remaining >= 2000) {
            vTaskDelay(pdMS_TO_TICKS((remaining + 999) / 1000));
          }
        }
      }
      i2s.pause();
    }
  }

  /**
   * Decode one SDU per stream (null = conceal) into interleaved PCM, frame by
   * frame, and hand it to the sink or I2S. For I2S a mono stream is written to
   * both channels. A PCM sink that overruns one SDU interval starves the
   * output exactly like an empty queue, so it counts as an underrun.
   */
  void render(const uint8_t *const sdu[2], uint32_t writeMs, uint32_t sduUs) {
    for (uint8_t b = 0; b < frames; b++) {
      size_t n = 0;
      for (uint8_t j = 0; j < nch; j++) {
        const Channel &c = ch[j];
        const uint8_t *in = sdu[c.stream] ? sdu[c.stream] + (size_t)(b * c.count + c.index) * octets : nullptr;
        n = decode(j, in);
        const int16_t *mono = pcm;
        int16_t *out = pcm + pcmCap;
        if (outCh == 1) {
          memcpy(out, mono, n * sizeof(int16_t));
        } else {
          for (size_t i = 0; i < n; i++) {
            out[2 * i + j] = mono[i];
            if (nch == 1) {
              out[2 * i + 1] = mono[i];
            }
          }
        }
      }
      const int16_t *out = pcm + pcmCap;
      if (sink) {
        const int64_t t0 = esp_timer_get_time();
        sink(out, n * outCh);
        if (esp_timer_get_time() - t0 > (int64_t)sduUs) {
          bleAudioBump(underruns);
        }
      } else if (i2s.write(out, n * outCh, writeMs) < n * outCh) {
        bleAudioBump(underruns);
      }
    }
  }

  /**
   * Decode (or conceal) one frame of channel @p j into the mono scratch; returns samples.
   * A frame that fails to decode is concealed; if PLC fails too, silence.
   */
  size_t decode(uint8_t j, const uint8_t *frame) {
    const uint32_t cap = pcmCap * sizeof(int16_t);
    int got = frame ? bleLC3Decode(dec[j], frame, octets, pcm, cap) : 0;
    if (got <= 0) {
      bleAudioBump(plc);
      got = bleLC3Decode(dec[j], nullptr, 0, pcm, cap);
    }
    if (got <= 0) {
      memset(pcm, 0, cap);
      return pcmCap;
    }
    return (size_t)got / sizeof(int16_t);
  }

  /** Tear the session down; the tap is fenced off first so it never touches freed rings. */
  void close() {
    ready.store(false);
    gate.drain();
    rate.store(0);
    chans.store(0);
    i2s.close();
    for (uint8_t j = 0; j < 2; j++) {
      bleLC3DecClose(dec[j]);
      dec[j] = nullptr;
    }
    for (uint8_t s = 0; s < 2; s++) {
      if (ring[s].queue) {
        vQueueDelete(ring[s].queue);
      }
      ring[s] = Ring();
    }
    free(mem);
    mem = nullptr;
    free(pcm);
    pcm = nullptr;
  }
};

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

// The Impl is created lazily so setters may be called before begin().
static BLEAudioPlayer::Impl &ensure(std::shared_ptr<BLEAudioPlayer::Impl> &impl) {
  if (!impl) {
    impl = std::make_shared<BLEAudioPlayer::Impl>();
  }
  return *impl;
}

BLEAudioPlayer::BLEAudioPlayer() = default;

BLEAudioPlayer::operator bool() const {
  return _impl && _impl->begun;
}

BTStatus BLEAudioPlayer::begin() {
  Impl &d = ensure(_impl);
  return d.beginPcm(static_cast<bool>(d.sink), "Player", "begin(bclk, ws, dout)");
}

BTStatus BLEAudioPlayer::begin(int bclk, int ws, int dout, int mclk, int port) {
  Impl &d = ensure(_impl);
  return d.beginI2S(static_cast<bool>(d.sink), bclk, ws, dout, /*tx=*/true, mclk, port, "Player");
}

void BLEAudioPlayer::end() {
  BLEAudioDataPath::endHandle(_impl, "Player");
}

BTStatus BLEAudioPlayer::setPCMSink(BLEAudioPCMSink sink) {
  Impl &d = ensure(_impl);
  if (d.begun || d.running()) {
    log_w("Player: setPCMSink() ignored after begin(); end() first (applies to the next begin)");
    return BTStatus::InvalidState;
  }
  d.sink = std::move(sink);
  return BTStatus::OK;
}

BTStatus BLEAudioPlayer::setJitterDepth(uint8_t sdus) {
  Impl &d = ensure(_impl);
  if (d.running()) {
    log_w("Player: setJitterDepth() ignored while running; stopPlayback() first (applies to the next run)");
    return BTStatus::InvalidState;
  }
  d.depthOverride = sdus;
  return BTStatus::OK;
}

BTStatus BLEAudioPlayer::attach(BLEAudioStream stream) {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  if (!stream) {
    return BTStatus::InvalidParam;
  }
  BLEAudioStreamTap tap;
  tap.recv = Impl::onRecv;
  return _impl->attach(stream, BLEAudioStream(), BLEAudioStream::Direction::Rx, tap);
}

BTStatus BLEAudioPlayer::attach(BLEAudioStream stream, BLEAudioStream right) {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  if (!stream || !right) {
    return BTStatus::InvalidParam;
  }
  BLEAudioStreamTap tap;
  tap.recv = Impl::onRecv;
  return _impl->attach(stream, right, BLEAudioStream::Direction::Rx, tap);
}

BTStatus BLEAudioPlayer::startPlayback() {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  // Statistics restart with each run; a startPlayback() while running keeps them.
  if (!_impl->running()) {
    _impl->received.store(0);
    _impl->dropped.store(0);
    _impl->lost.store(0);
    _impl->plc.store(0);
    _impl->underruns.store(0);
    _impl->late.store(0);
  }
  return _impl->start("ble_lc3_play");
}

void BLEAudioPlayer::stopPlayback() {
  if (_impl) {
    _impl->stop();
  }
}

bool BLEAudioPlayer::isRunning() const {
  return _impl && _impl->running();
}

uint8_t BLEAudioPlayer::channels() const {
  return _impl ? _impl->chans.load() : 0;
}

uint32_t BLEAudioPlayer::sampleRate() const {
  return _impl ? _impl->rate.load() : 0;
}

uint32_t BLEAudioPlayer::sdusReceived() const {
  return _impl ? _impl->received.load(std::memory_order_relaxed) : 0;
}

uint32_t BLEAudioPlayer::sdusDropped() const {
  return _impl ? _impl->dropped.load(std::memory_order_relaxed) : 0;
}

uint32_t BLEAudioPlayer::sdusLost() const {
  return _impl ? _impl->lost.load(std::memory_order_relaxed) : 0;
}

uint32_t BLEAudioPlayer::plcFrames() const {
  return _impl ? _impl->plc.load(std::memory_order_relaxed) : 0;
}

uint32_t BLEAudioPlayer::underruns() const {
  return _impl ? _impl->underruns.load(std::memory_order_relaxed) : 0;
}

uint32_t BLEAudioPlayer::lateResyncs() const {
  return _impl ? _impl->late.load(std::memory_order_relaxed) : 0;
}

#endif /* BLE_AUDIO_LC3_SUPPORTED */
