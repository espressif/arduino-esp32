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

#include "BLEAudioPipeline.h"

#include "esp_lc3_dec.h"
#include "esp_lc3_enc.h"

/**
 * @file BLEAudioPipeline.cpp
 * @brief LC3 codec wrappers, the I2S port and the data path shared by the player and recorder.
 *
 * Contracts are documented on the declarations in `BLEAudioPipeline.h`; the
 * definitions below carry implementation notes only.
 */

// Keep the codec off the host's core so decoding never delays ISO timing.
BaseType_t bleAudioCodecCore() {
#if BLE_HOST_PINNED_TO_CORE_DEFINED
#if BLE_HOST_PINNED_CORE == 0
  if (portNUM_PROCESSORS > 1) {
    return 1;
  }
  /* Single core: the codec shares the host core by construction, so a slow
   * PCM sink/source can delay ISO timing. Said once; the sink/source budget
   * in BLEAudioPCM.h is load-bearing here. */
  static bool warned = false;
  if (!warned) {
    warned = true;
    ESP_LOGW("BLEAudio", "codec task shares the host core (single-core target): keep PCM callbacks fast");
  }
  return 0;
#else
  return 0;
#endif
#else
  return tskNO_AFFINITY;
#endif
}

// ---------------------------------------------------------------------------
// LC3
// ---------------------------------------------------------------------------

/** @brief Frame duration in the codec's 0.1 ms units.
 *
 * LC3 knows only 7.5 and 10 ms frames. The engine reports nominal durations
 * (7500/10000), but custom configs may carry the real 44.1 kHz lengths
 * (8163/10884 us on air are still 7.5/10 ms LC3 frames): split at the
 * midpoint so both spellings map correctly. */
static uint8_t frameDms(const BLEAudioCodecConfig &cfg) {
  return (cfg.frameDurationUs < 8750) ? 75 : 100;
}

// Constant bit rate, raw frames (no length prefix): the BAP SDU layout.
void *bleLC3DecOpen(const BLEAudioCodecConfig &cfg) {
  esp_lc3_dec_cfg_t c = {};
  c.sample_rate = cfg.samplingRateHz;
  c.channel = 1;
  c.bits_per_sample = 16;
  c.frame_dms = frameDms(cfg);
  c.nbyte = cfg.octetsPerFrame;
  c.is_cbr = 1;
  c.len_prefixed = 0;
  c.enable_plc = 1;
  void *h = nullptr;
  return (esp_lc3_dec_open(&c, sizeof(c), &h) == ESP_AUDIO_ERR_OK) ? h : nullptr;
}

// PLC input: the codec ignores it, but a future codec reading input during
// concealment must see silence, not the output buffer it is about to fill
// (aliasing output-as-input would feed decoded audio back into the concealer).
// Sized past the largest preset frame (155 octets) in case a codec reads a full frame of input while concealing.
static const uint8_t kPlcSilence[160] = {};

int bleLC3Decode(void *dec, const uint8_t *frame, uint16_t len, int16_t *pcm, uint32_t cap) {
  esp_audio_dec_in_raw_t raw = {};
  raw.buffer = const_cast<uint8_t *>(frame ? frame : kPlcSilence);
  raw.len = len;
  raw.frame_recover = frame ? ESP_AUDIO_DEC_RECOVERY_NONE : ESP_AUDIO_DEC_RECOVERY_PLC;
  esp_audio_dec_out_frame_t out = {};
  out.buffer = reinterpret_cast<uint8_t *>(pcm);
  out.len = cap;
  esp_audio_dec_info_t info = {};
  esp_audio_err_t err = esp_lc3_dec_decode(dec, &raw, &out, &info);
  return (err == ESP_AUDIO_ERR_OK) ? static_cast<int>(out.decoded_size) : 0;
}

void bleLC3DecClose(void *dec) {
  if (dec) {
    esp_lc3_dec_close(dec);
  }
}

void *bleLC3EncOpen(const BLEAudioCodecConfig &cfg, int *pcmBytes, int *outBytes) {
  esp_lc3_enc_config_t c = {};
  c.sample_rate = cfg.samplingRateHz;
  c.bits_per_sample = 16;
  c.channel = 1;
  c.frame_dms = frameDms(cfg);
  c.nbyte = cfg.octetsPerFrame;
  c.len_prefixed = 0;
  void *h = nullptr;
  if (esp_lc3_enc_open(&c, sizeof(c), &h) != ESP_AUDIO_ERR_OK) {
    return nullptr;
  }
  if (esp_lc3_enc_get_frame_size(h, pcmBytes, outBytes) != ESP_AUDIO_ERR_OK || *pcmBytes <= 0) {
    esp_lc3_enc_close(h);
    return nullptr;
  }
  return h;
}

bool bleLC3Encode(void *enc, const int16_t *pcm, int pcmBytes, uint8_t *out, uint32_t cap, uint16_t octets) {
  esp_audio_enc_in_frame_t in = {};
  in.buffer = reinterpret_cast<uint8_t *>(const_cast<int16_t *>(pcm));
  in.len = static_cast<uint32_t>(pcmBytes);
  esp_audio_enc_out_frame_t o = {};
  o.buffer = out;
  o.len = cap;
  return esp_lc3_enc_process(enc, &in, &o) == ESP_AUDIO_ERR_OK && o.encoded_bytes == octets;
}

void bleLC3EncClose(void *enc) {
  if (enc) {
    esp_lc3_enc_close(enc);
  }
}

// ---------------------------------------------------------------------------
// I2S
// ---------------------------------------------------------------------------

bool BLEAudioI2SPort::open(const BLEAudioI2SConfig &pins, bool tx, uint32_t rateHz, uint8_t channels, uint16_t frameSamples, uint8_t dmaFrames) {
  close();
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(pins.port, I2S_ROLE_MASTER);
  // One LC3 frame per DMA buffer, so every write/read moves exactly one frame.
  cc.dma_desc_num = dmaFrames;
  cc.dma_frame_num = frameSamples;
  // An output underrun plays silence instead of repeating the last buffer.
  cc.auto_clear = tx;
  if (i2s_new_channel(&cc, tx ? &_chan : nullptr, tx ? nullptr : &_chan) != ESP_OK) {
    _chan = nullptr;
    return false;
  }
  i2s_slot_mode_t mode = (channels == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
  i2s_std_config_t sc = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(rateHz),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, mode),
    .gpio_cfg =
      {
        .mclk = static_cast<gpio_num_t>(pins.mclk),
        .bclk = static_cast<gpio_num_t>(pins.bclk),
        .ws = static_cast<gpio_num_t>(pins.ws),
        .dout = tx ? static_cast<gpio_num_t>(pins.dout) : I2S_GPIO_UNUSED,
        .din = tx ? I2S_GPIO_UNUSED : static_cast<gpio_num_t>(pins.din),
        .invert_flags = {0, 0, 0},
      },
  };
  // Newer I2S hardware (S3/S31/P4) leaves slot_mask = BOTH even in mono, so
  // DMA delivers both slots and a mono read spans half the expected audio
  // time (deep + crackling). Same fix as ESP_I2S: force LEFT for true mono,
  // one sample per LRCK period.
  if (sc.slot_cfg.slot_mode == I2S_SLOT_MODE_MONO) {
    sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  }
  if (i2s_channel_init_std_mode(_chan, &sc) != ESP_OK) {
    i2s_del_channel(_chan);
    _chan = nullptr;
    return false;
  }
  _tx = tx;
  _frameSamples = frameSamples;
  _channels = channels;
  return true;
}

void BLEAudioI2SPort::close() {
  pause();
  if (_chan) {
    i2s_del_channel(_chan);
    _chan = nullptr;
  }
}

bool BLEAudioI2SPort::start() {
  if (!_chan) {
    return false;
  }
  if (_on) {
    return true;
  }
  if (_tx) {
    // Prime exactly one DMA buffer: the first write() then blocks for one
    // frame, and the known startup latency stays at one frame instead of the
    // whole ring.
    static const int16_t kSilence[64] = {};
    size_t left = (size_t)_frameSamples * _channels * sizeof(int16_t);
    while (left > 0) {
      size_t chunk = left < sizeof(kSilence) ? left : sizeof(kSilence);
      size_t loaded = 0;
      if (i2s_channel_preload_data(_chan, kSilence, chunk, &loaded) != ESP_OK || loaded == 0) {
        break;
      }
      left -= loaded;
    }
  }
  _on = (i2s_channel_enable(_chan) == ESP_OK);
  return _on;
}

void BLEAudioI2SPort::pause() {
  if (_chan && _on) {
    i2s_channel_disable(_chan);
    _on = false;
  }
}

size_t BLEAudioI2SPort::write(const int16_t *pcm, size_t samples, uint32_t timeoutMs) {
  size_t n = 0;
  if (_on) {
    i2s_channel_write(_chan, pcm, samples * sizeof(int16_t), &n, timeoutMs);
  }
  return n / sizeof(int16_t);
}

size_t BLEAudioI2SPort::read(int16_t *pcm, size_t samples, uint32_t timeoutMs) {
  size_t n = 0;
  if (_on) {
    i2s_channel_read(_chan, pcm, samples * sizeof(int16_t), &n, timeoutMs);
  }
  return n / sizeof(int16_t);
}

// ---------------------------------------------------------------------------
// Data path
// ---------------------------------------------------------------------------

// Each stream gets its own Port as tap context so the tap knows which ring to fill.
BTStatus BLEAudioDataPath::attach(const BLEAudioStream &a, const BLEAudioStream &b, BLEAudioStream::Direction dir, const BLEAudioStreamTap &tap) {
  if (running()) {
    return (a == streams[0] && b == streams[1]) ? BTStatus::OK : BTStatus::InvalidState;
  }
  if (!a || a == b) {
    return BTStatus::InvalidParam;
  }
  const BLEAudioStream *in[2] = {&a, &b};
  for (const BLEAudioStream *s : in) {
    BLEAudioStream::Direction d = s->direction();
    if (*s && d != BLEAudioStream::Direction::Unknown && d != dir) {
      return BTStatus::InvalidParam;
    }
  }
  detach();
  streams[0] = a;
  streams[1] = b;
  count = b ? 2 : 1;
  this->dir = dir;
  // detach() above removed our taps, so taps[] is not visible to the host task here.
  for (uint8_t i = 0; i < count; i++) {
    taps[i] = tap;
    taps[i].started = onWake;
    taps[i].stopped = onWake;
    taps[i].ctx = &ports[i];
    BLEAudioStreamAccess::setTap(streams[i], &taps[i]);
  }
  return BTStatus::OK;
}

void BLEAudioDataPath::detach() {
  for (uint8_t i = 0; i < count; i++) {
    BLEAudioStreamAccess::clearTap(streams[i], &taps[i]);
    streams[i] = BLEAudioStream();
  }
  count = 0;
}

// The task clears `task` itself right before deleting, which is what stop() waits for.
void BLEAudioDataPath::entry(void *arg) {
  auto *self = static_cast<BLEAudioDataPath *>(arg);
  // Wait until start() has published our TaskHandle_t into self->task so an
  // early exit from run() can never clear self->task before start() stores it.
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  self->run(self);
  // 4 KB stack (BLE_AUDIO_CODEC_STACK) holds the LC3 instances on the heap
  // plus decode/encode/I2S call chains: report the margin for the sizing log.
  ESP_LOGD("BLEAudio", "codec task stack high-water: %u words", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  self->task.store(nullptr, std::memory_order_release);
  vTaskDelete(nullptr);
}

BTStatus BLEAudioDataPath::start(const char *name) {
  if (running()) {
    return quit.load(std::memory_order_acquire) ? BTStatus::InvalidState : BTStatus::OK;
  }
  if (!count || !run) {
    return BTStatus::InvalidState;
  }
  quit.store(false, std::memory_order_release);
  gate.open();
  TaskHandle_t h = nullptr;
  if (xTaskCreatePinnedToCore(entry, name, BLE_AUDIO_CODEC_STACK, this, BLE_AUDIO_CODEC_PRIO, &h, bleAudioCodecCore()) != pdPASS) {
    gate.close();
    return BTStatus::NoMemory;
  }
  task.store(h, std::memory_order_release);
  xTaskNotifyGive(h);
  return BTStatus::OK;
}

// Called from the codec task itself (e.g. a user callback), it cannot wait for
// its own exit: `quit` is set and the task ends when its body returns.
void BLEAudioDataPath::stop() {
  gate.close();
  TaskHandle_t t = task.load();
  if (!t) {
    return;
  }
  quit.store(true);
  xTaskNotifyGive(t);
  if (t == xTaskGetCurrentTaskHandle()) {
    return;
  }
  while (task.load()) {
    vTaskDelay(1);
  }
}

void BLEAudioDataPath::notify() {
  TaskHandle_t t = task.load();
  if (t) {
    xTaskNotifyGive(t);
  }
}

int BLEAudioDataPath::primary() const {
  for (uint8_t i = 0; i < count; i++) {
    if (streams[i].isStreaming()) {
      return i;
    }
  }
  return -1;
}

// Supported: one stream with 1 or 2 channels (blocks interleaved per BAP), or two mono streams.
uint8_t BLEAudioDataPath::mapChannels(Channel out[2]) const {
  if (!count) {
    return 0;
  }
  for (uint8_t s = 0; s < count; s++) {
    BLEAudioStream::Direction d = streams[s].direction();
    if (d != BLEAudioStream::Direction::Unknown && d != dir) {
      return 0;
    }
  }
  if (count == 1) {
    uint8_t c = streams[0].codecConfig().channels();
    if (c < 1 || c > 2) {
      return 0;
    }
    for (uint8_t k = 0; k < c; k++) {
      out[k] = {0, k, c};
    }
    return c;
  }
  BLEAudioCodecConfig a = streams[0].codecConfig();
  BLEAudioCodecConfig b = streams[1].codecConfig();
  BLEAudioQos qa = streams[0].qos();
  BLEAudioQos qb = streams[1].qos();
  if (a.channels() != 1 || b.channels() != 1) {
    return 0;
  }
  // Two separate streams must describe the same codec/QoS; the player and
  // recorder build one session from the first stream and map both onto it.
  if (a.samplingRateHz != b.samplingRateHz || a.frameDurationUs != b.frameDurationUs || a.octetsPerFrame != b.octetsPerFrame || a.framesPerSdu != b.framesPerSdu
      || qa.sduIntervalUs != qb.sduIntervalUs) {
    return 0;
  }
  for (uint8_t s = 0; s < 2; s++) {
    out[s] = {s, 0, 1};
  }
  return 2;
}

void BLEAudioDataPath::onWake(void *ctx) {
  // ctx is the registered Port (never NULL by construction; the deref below
  // assumes it, so a null check here would be dead and inconsistent).
  BLEAudioDataPath *self = static_cast<Port *>(ctx)->owner;
  if (self->gate.enter()) {
    self->notify();
    self->gate.leave();
  }
}

#endif /* BLE_AUDIO_LC3_SUPPORTED */
