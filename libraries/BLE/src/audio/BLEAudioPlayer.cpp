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

#include "audio/BLEAudioPlayer.h"
#include "audio/BLEAudioPipeline.h"

#include <cstdlib>
#include <cstring>
#include "freertos/queue.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioPlayer.cpp
 * @brief Received LC3 SDUs -> jitter buffer -> LC3 decode (with PLC) -> I2S or a PCM sink.
 *
 * The stream tap (BT host task) copies each SDU into a per-stream ring; the
 * codec task waits until the ring holds the jitter depth, then decodes one SDU
 * per interval, paced by the primary stream's queue and by the blocking I2S
 * write. A session (buffers, decoders, I2S) is built when a stream starts and
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
    uint16_t expect = 0;            // Valid SDU length; any other length is treated as lost.
    uint8_t head = 0;               // Next slot the producer writes.
  };

  // Configuration (set before start()).
  BLEAudioI2sConfig pins;
  BLEAudioPcmSink sink;        // When set, PCM goes here instead of I2S.
  uint8_t depthOverride = 0;   // Jitter depth in SDUs, 0 = from the presentation delay.
  bool begun = false;

  // Jitter buffer, shared with the tap.
  Ring ring[2];
  uint16_t stride = 0;          // Bytes per slot: header + largest SDU, 4-byte aligned.
  uint8_t slots = 0;
  std::atomic<bool> ready{false};  // Rings valid; the tap drops SDUs while false.

  // Session state, codec task only.
  BLEAudioI2sPort i2s;
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
  std::atomic<uint32_t> received{0}, dropped{0}, lost{0}, plc{0}, underruns{0};

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
    auto *port = static_cast<Port *>(ctx);
    if (!port) {
      return;
    }
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
    uint32_t intervalUs = qos.sduIntervalUs ? qos.sduIntervalUs : (uint32_t)cfg.frameDurationUs * frames;
    uint32_t depth = depthOverride ? depthOverride : qos.presentationDelayUs / intervalUs;
    depth = (depth < 2) ? 2 : (depth > kMaxDepth ? kMaxDepth : depth);
    uint8_t queueLen = (uint8_t)(depth * 2);
    slots = queueLen + 1;
    uint16_t perStream = (uint16_t)(octets * (count == 1 ? nch : 1) * frames);
    uint16_t slotBytes = (qos.maxSdu > perStream) ? qos.maxSdu : perStream;
    stride = (uint16_t)((kSlotHeader + slotBytes + 3) & ~3u);
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
      dec[j] = bleLc3DecOpen(cfg);
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

    TickType_t wait = pdMS_TO_TICKS((intervalUs + 999) / 1000);
    uint32_t writeMs = cfg.frameDurationUs / 1000;
    play(p, (uint8_t)depth, wait ? wait : 1, writeMs ? writeMs : 1);
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
   */
  void play(int p, uint8_t depth, TickType_t wait, uint32_t writeMs) {
    while (!quit.load()) {
      ulTaskNotifyTake(pdTRUE, 0);
      while (uxQueueMessagesWaiting(ring[p].queue) < depth) {
        if (quit.load() || primary() < 0) {
          return;
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
      }
      i2s.start();
      // The primary stream paces the loop; after 2 x depth empty intervals, rebuffer.
      uint8_t idle = 0;
      while (!quit.load() && idle < depth * 2) {
        const uint8_t *sdu[2] = {nullptr, nullptr};
        bool got = false;
        for (uint8_t s = 0; s < count; s++) {
          uint8_t slot;
          if (xQueueReceive(ring[s].queue, &slot, (s == p) ? wait : 0) == pdTRUE) {
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
          return;
        }
        idle = got ? 0 : idle + 1;
        render(sdu, writeMs);
      }
      i2s.pause();
    }
  }

  /**
   * Decode one SDU per stream (null = conceal) into interleaved PCM, frame by
   * frame, and hand it to the sink or I2S. For I2S a mono stream is written to
   * both channels.
   */
  void render(const uint8_t *const sdu[2], uint32_t writeMs) {
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
        sink(out, n * outCh);
      } else {
        i2s.write(out, n * outCh, writeMs);
      }
    }
  }

  /**
   * Decode (or conceal) one frame of channel @p j into the mono scratch; returns samples.
   * A frame that fails to decode is concealed; if PLC fails too, silence.
   */
  size_t decode(uint8_t j, const uint8_t *frame) {
    int got = frame ? decodeInto(j, frame) : 0;
    if (got <= 0) {
      bleAudioBump(plc);
      got = decodeInto(j, nullptr);
    }
    if (got <= 0) {
      memset(pcm, 0, pcmCap * sizeof(int16_t));
      return pcmCap;
    }
    return (size_t)got / sizeof(int16_t);
  }

  /** One decode call; grows the PCM buffers once if the codec asks for more room. */
  int decodeInto(uint8_t j, const uint8_t *frame) {
    int got = bleLc3Decode(dec[j], frame, octets, pcm, pcmCap * sizeof(int16_t));
    if (got < 0) {
      uint32_t need = (uint32_t)(-got) / sizeof(int16_t);
      void *grown = realloc(pcm, (size_t)need * (1 + outCh) * sizeof(int16_t));
      if (!grown) {
        return 0;
      }
      pcm = static_cast<int16_t *>(grown);
      pcmCap = need;
      got = bleLc3Decode(dec[j], frame, octets, pcm, pcmCap * sizeof(int16_t));
    }
    return got;
  }

  /** Tear the session down; the tap is fenced off first so it never touches freed rings. */
  void close() {
    ready.store(false);
    gate.drain();
    rate.store(0);
    chans.store(0);
    i2s.close();
    for (uint8_t j = 0; j < 2; j++) {
      bleLc3DecClose(dec[j]);
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

BTStatus BLEAudioPlayer::begin(const BLEAudioI2sConfig &i2s) {
  Impl &d = ensure(_impl);
  if (d.running()) {
    return BTStatus::InvalidState;
  }
  if (!d.sink && (i2s.bclk < 0 || i2s.ws < 0 || i2s.dout < 0)) {
    log_e("Player: begin() needs BCLK, WS and DOUT pins (or a PCM sink set first)");
    return BTStatus::InvalidParam;
  }
  d.pins = i2s;
  d.begun = true;
  return BTStatus::OK;
}

void BLEAudioPlayer::end() {
  if (_impl) {
    _impl->stop();
    _impl->detach();
    _impl.reset();
  }
}

BLEAudioPlayer &BLEAudioPlayer::setPcmSink(BLEAudioPcmSink sink) {
  Impl &d = ensure(_impl);
  if (!d.running()) {
    d.sink = std::move(sink);
  }
  return *this;
}

BLEAudioPlayer &BLEAudioPlayer::setJitterDepth(uint8_t sdus) {
  ensure(_impl).depthOverride = sdus;
  return *this;
}

BTStatus BLEAudioPlayer::attach(BLEAudioStream stream, BLEAudioStream right) {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  BLEAudioStreamTap tap;
  tap.recv = Impl::onRecv;
  return _impl->attach(stream, right, BLEAudioStream::Direction::Rx, tap);
}

BTStatus BLEAudioPlayer::start() {
  if (!*this) {
    return BTStatus::InvalidState;
  }
  // Statistics restart with each run; a start() while running keeps them.
  if (!_impl->running()) {
    _impl->received.store(0);
    _impl->dropped.store(0);
    _impl->lost.store(0);
    _impl->plc.store(0);
    _impl->underruns.store(0);
  }
  return _impl->start("ble_lc3_play");
}

void BLEAudioPlayer::stop() {
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

#endif /* BLE_AUDIO_LC3_SUPPORTED */
