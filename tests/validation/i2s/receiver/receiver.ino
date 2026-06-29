/*
 * I2S Validation Test -- Receiver (multi-DUT, generic_multi_device)
 *
 * Listens for I2S data from the sender device, verifies the received
 * ramp pattern, and reports match results via serial.
 *
 * The master TX DMA pre-fills ~1440 zero samples before user data,
 * so we read a large buffer and use a sliding-window search.
 *
 * Pin-to-pin mapping (generic_multi_device): see ../pins_config.h
 */

#include <Arduino.h>
#include <ESP_I2S.h>
#include "soc/soc_caps.h"
#if SOC_I2S_SUPPORTS_TDM
#include "driver/i2s_tdm.h"
#endif
#include "../pins_config.h"

#if !SOC_I2S_SUPPORTED
#error "I2S not supported on this SoC"
#endif

I2SClass i2s;

static const size_t RAMP_LEN = 64;
static const size_t RX_BUF_SAMPLES = 2048;
static const size_t RX_BUF_MAX_BYTES = 8192;

struct RxVerifyResult {
  int best_matches;
  int full_ramps;
  int zeros;
  size_t total;
};

static void reportResult(const char *tag, const RxVerifyResult &r) {
  Serial.printf("[RECEIVER] %s matches=%d full_ramps=%d zeros=%d total=%u\n", tag, r.best_matches, r.full_ramps, r.zeros, (unsigned)r.total);
}

// --- Mono 16-bit ramp: sample[j] = j * 100 ---
static RxVerifyResult verifyRamp16(const int16_t *buf, size_t n) {
  RxVerifyResult r = {0, 0, 0, n};
  for (size_t i = 0; i < n; i++) {
    if (buf[i] == 0) {
      r.zeros++;
    }
  }
  for (size_t f = 0; f + RAMP_LEN <= n; f++) {
    int m = 0;
    for (size_t j = 0; j < RAMP_LEN; j++) {
      if (buf[f + j] == (int16_t)(j * 100)) {
        m++;
      }
    }
    if (m == (int)RAMP_LEN) {
      r.full_ramps++;
    }
    if (m > r.best_matches) {
      r.best_matches = m;
    }
  }
  return r;
}

// --- Generic ramp verification for any bit width and channel count ---

static int32_t getSample(const void *buf, size_t idx, int bits) {
  switch (bits) {
    case 8:  return ((const int8_t *)buf)[idx];
    case 16: return ((const int16_t *)buf)[idx];
    case 24: return ((const int32_t *)buf)[idx] & 0x00FFFFFF;
    case 32: return ((const int32_t *)buf)[idx];
    default: return 0;
  }
}

static int32_t expectedVal(size_t j, int bits) {
  switch (bits) {
    case 8:  return (int32_t)(j * 2);
    case 16: return (int32_t)(j * 100);
    case 24: return (int32_t)(j * 50000) & 0x00FFFFFF;
    case 32: return (int32_t)(j * 100000);
    default: return 0;
  }
}

static int32_t expectedValR(size_t j, int bits) {
  switch (bits) {
    case 8:  return (int32_t)(j * 2 + 1);
    case 16: return (int32_t)(10000 + (int32_t)(j * 100));
    case 24: return (int32_t)(5000000 + (int32_t)(j * 50000)) & 0x00FFFFFF;
    case 32: return (int32_t)(5000000 + (int32_t)(j * 100000));
    default: return 0;
  }
}

static int32_t expectedValAvg(size_t j, int bits) {
  return (expectedVal(j, bits) + expectedValR(j, bits)) / 2;
}

// ramp_type: 0 = L ramp (default), 1 = R ramp, 2 = averaged (L+R)/2
static RxVerifyResult verifyRampGeneric(const void *buf, size_t n_bytes, int bits, int ch, int ramp_type = 0) {
  size_t sample_bytes = (bits <= 8) ? 1 : (bits <= 16) ? 2 : 4;
  size_t n_samples = n_bytes / sample_bytes;
  size_t n_frames = n_samples / ch;

  RxVerifyResult r = {0, 0, 0, n_frames};

  for (size_t i = 0; i < n_samples; i++) {
    if (getSample(buf, i, bits) == 0) {
      r.zeros++;
    }
  }

  for (size_t f = 0; f + RAMP_LEN <= n_frames; f++) {
    int m = 0;
    for (size_t j = 0; j < RAMP_LEN; j++) {
      size_t idx = (f + j) * ch;

      if (ch == 1) {
        int32_t exp;
        if (ramp_type == 2) {
          exp = expectedValAvg(j, bits);
        } else if (ramp_type == 1) {
          exp = expectedValR(j, bits);
        } else {
          exp = expectedVal(j, bits);
        }
        if (getSample(buf, idx, bits) == exp) {
          m++;
        }
      } else {
        int32_t exp_l = expectedVal(j, bits);
        int32_t exp_r = expectedValR(j, bits);
        if (getSample(buf, idx, bits) == exp_l && getSample(buf, idx + 1, bits) == exp_r) {
          m++;
        }
      }
    }
    if (m == (int)RAMP_LEN) {
      r.full_ramps++;
    }
    if (m > r.best_matches) {
      r.best_matches = m;
    }
  }

  return r;
}

// Helper: run a mono 16-bit RX phase and report.
static void rxMono16(const char *done_tag, const char *fail_tag, const char *listen_tag, uint32_t rate, int8_t slot_mask) {
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  if (!i2s.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, slot_mask, I2S_ROLE_SLAVE)) {
    Serial.printf("[RECEIVER] %s begin failed\n", fail_tag);
    return;
  }
  Serial.printf("[RECEIVER] %s\n", listen_tag);

  int16_t *rx_buf = (int16_t *)calloc(RX_BUF_SAMPLES, sizeof(int16_t));
  if (!rx_buf) {
    Serial.printf("[RECEIVER] %s alloc failed\n", fail_tag);
    i2s.end();
    return;
  }

  size_t read_bytes = i2s.readBytes((char *)rx_buf, RX_BUF_SAMPLES * sizeof(int16_t));
  RxVerifyResult r = verifyRamp16(rx_buf, read_bytes / sizeof(int16_t));

  free(rx_buf);
  i2s.end();
  reportResult(done_tag, r);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("[RECEIVER] Ready");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "START_RX") {
      rxMono16("RX_DONE", "RX_FAIL", "RX_LISTENING", 16000, -1);

    } else if (cmd == "START_RX_LEFT") {
      rxMono16("RX_LEFT_DONE", "RX_LEFT_FAIL", "RX_LEFT_LISTENING", 16000, I2S_STD_SLOT_LEFT);

    } else if (cmd == "START_RX_RIGHT") {
      rxMono16("RX_RIGHT_DONE", "RX_RIGHT_FAIL", "RX_RIGHT_LISTENING", 16000, I2S_STD_SLOT_RIGHT);

    } else if (cmd.startsWith("RX ")) {
      // RX <rate> <bits> <ch> [slot] [ramp_type]
      // ramp_type: 0 = L ramp (default), 1 = R ramp
      int rate, bits, ch, slot = -1, ramp_type = 0;
      int n = sscanf(cmd.c_str() + 3, "%d %d %d %d %d", &rate, &bits, &ch, &slot, &ramp_type);
      if (n < 3) {
        Serial.println("[RECEIVER] RX_PARAM_FAIL parse error");
        return;
      }

      i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
      if (!i2s.begin(I2S_MODE_STD, (uint32_t)rate, (i2s_data_bit_width_t)bits, (i2s_slot_mode_t)ch, (int8_t)slot, I2S_ROLE_SLAVE)) {
        Serial.println("[RECEIVER] RX_PARAM_FAIL begin failed");
        return;
      }
      Serial.println("[RECEIVER] RX_PARAM_LISTENING");
      Serial.flush();

      size_t sample_bytes = (bits <= 8) ? 1 : (bits <= 16) ? 2 : 4;
      size_t frame_bytes = sample_bytes * ch;
      size_t rx_frames = min((size_t)2048, RX_BUF_MAX_BYTES / frame_bytes);
      size_t rx_bytes = rx_frames * frame_bytes;

      uint8_t *rx_buf = (uint8_t *)calloc(1, rx_bytes);
      if (!rx_buf) {
        Serial.println("[RECEIVER] RX_PARAM_FAIL alloc failed");
        i2s.end();
        return;
      }

      size_t read_bytes = i2s.readBytes((char *)rx_buf, rx_bytes);
      RxVerifyResult r = verifyRampGeneric(rx_buf, read_bytes, bits, ch, ramp_type);

      free(rx_buf);
      i2s.end();
      reportResult("RX_PARAM_DONE", r);

    } else if (cmd.startsWith("RX_RECONFIG ")) {
      int rate, bits, ch, slot_init, slot_reconfig;
      int n = sscanf(cmd.c_str() + 12, "%d %d %d %d %d", &rate, &bits, &ch, &slot_init, &slot_reconfig);
      if (n < 5) {
        Serial.println("[RECEIVER] RX_RECONFIG_FAIL parse error");
        return;
      }

      i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
      if (!i2s.begin(I2S_MODE_STD, (uint32_t)rate, (i2s_data_bit_width_t)bits, (i2s_slot_mode_t)ch, (int8_t)slot_init, I2S_ROLE_SLAVE)) {
        Serial.println("[RECEIVER] RX_RECONFIG_FAIL begin failed");
        return;
      }
      if (!i2s.configureRX((uint32_t)rate, (i2s_data_bit_width_t)bits, (i2s_slot_mode_t)ch, I2S_RX_TRANSFORM_NONE, (int8_t)slot_reconfig)) {
        Serial.println("[RECEIVER] RX_RECONFIG_FAIL configureRX failed");
        i2s.end();
        return;
      }
      Serial.println("[RECEIVER] RX_RECONFIG_LISTENING");
      Serial.flush();

      size_t sample_bytes = (bits <= 8) ? 1 : (bits <= 16) ? 2 : 4;
      size_t frame_bytes = sample_bytes * ch;
      size_t rx_frames = min((size_t)2048, RX_BUF_MAX_BYTES / frame_bytes);
      size_t rx_bytes = rx_frames * frame_bytes;

      uint8_t *rx_buf = (uint8_t *)calloc(1, rx_bytes);
      if (!rx_buf) {
        Serial.println("[RECEIVER] RX_RECONFIG_FAIL alloc failed");
        i2s.end();
        return;
      }

      size_t read_bytes = i2s.readBytes((char *)rx_buf, rx_bytes);
      RxVerifyResult r = verifyRampGeneric(rx_buf, read_bytes, bits, ch);

      free(rx_buf);
      i2s.end();
      reportResult("RX_RECONFIG_DONE", r);

    } else if (cmd == "START_RX_SLOT_PRESERVE") {
      // Fix 2 regression: begin with RIGHT, change rate twice with default
      // slot_mask, verify RIGHT ramp is still received (slot preserved).
      i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
      if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT, I2S_ROLE_SLAVE)) {
        Serial.println("[RECEIVER] RX_SLOT_PRESERVE_FAIL begin failed");
        return;
      }
      // Rate change with default slot_mask: must preserve RIGHT
      if (!i2s.configureRX(8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        Serial.println("[RECEIVER] RX_SLOT_PRESERVE_FAIL configureRX(8k) failed");
        i2s.end();
        return;
      }
      // Rate back to 16kHz with default slot_mask: must still be RIGHT
      if (!i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        Serial.println("[RECEIVER] RX_SLOT_PRESERVE_FAIL configureRX(16k) failed");
        i2s.end();
        return;
      }
      Serial.println("[RECEIVER] RX_SLOT_PRESERVE_LISTENING");
      Serial.flush();

      int16_t *rx_buf = (int16_t *)calloc(RX_BUF_SAMPLES, sizeof(int16_t));
      if (!rx_buf) {
        Serial.println("[RECEIVER] RX_SLOT_PRESERVE_FAIL alloc failed");
        i2s.end();
        return;
      }
      size_t read_bytes = i2s.readBytes((char *)rx_buf, RX_BUF_SAMPLES * sizeof(int16_t));
      // Mono TX always sends L ramp pattern (j*100) on all wire slots,
      // so verify against ramp_type=0 even though RX extracts the RIGHT slot.
      RxVerifyResult r = verifyRampGeneric(rx_buf, read_bytes, 16, 1, 0);
      free(rx_buf);
      i2s.end();
      reportResult("RX_SLOT_PRESERVE_DONE", r);

    } else if (cmd == "START_RX_MONO_BOTH") {
      // Bug 1 regression test: begin(MONO, BOTH, 16-bit) on HW v1.
      // On HW v1 the workaround should use _both (averaging), not _left.
      // TX sends stereo [L=ramp, R=offset_ramp]; expected output = (L+R)/2.
      i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
      if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_BOTH, I2S_ROLE_SLAVE)) {
        Serial.println("[RECEIVER] RX_MONO_BOTH_FAIL begin failed");
        return;
      }
      Serial.println("[RECEIVER] RX_MONO_BOTH_LISTENING");
      Serial.flush();

      int16_t *rx_buf = (int16_t *)calloc(RX_BUF_SAMPLES, sizeof(int16_t));
      if (!rx_buf) {
        Serial.println("[RECEIVER] RX_MONO_BOTH_FAIL alloc failed");
        i2s.end();
        return;
      }
      size_t read_bytes = i2s.readBytes((char *)rx_buf, RX_BUF_SAMPLES * sizeof(int16_t));
      RxVerifyResult r = verifyRampGeneric(rx_buf, read_bytes, 16, 1, 2);
      free(rx_buf);
      i2s.end();
      reportResult("RX_MONO_BOTH_DONE", r);

    } else if (cmd == "START_RX_RECONFIG_BOTH") {
      // Bug 2 regression test: begin(MONO, LEFT), then configureRX(MONO, BOTH).
      // On HW v1, slot-only change should switch from _left to _both (averaging).
      i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
      if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT, I2S_ROLE_SLAVE)) {
        Serial.println("[RECEIVER] RX_RECONFIG_BOTH_FAIL begin failed");
        return;
      }
      if (!i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_BOTH)) {
        Serial.println("[RECEIVER] RX_RECONFIG_BOTH_FAIL configureRX failed");
        i2s.end();
        return;
      }
      Serial.println("[RECEIVER] RX_RECONFIG_BOTH_LISTENING");
      Serial.flush();

      int16_t *rx_buf = (int16_t *)calloc(RX_BUF_SAMPLES, sizeof(int16_t));
      if (!rx_buf) {
        Serial.println("[RECEIVER] RX_RECONFIG_BOTH_FAIL alloc failed");
        i2s.end();
        return;
      }
      size_t read_bytes = i2s.readBytes((char *)rx_buf, RX_BUF_SAMPLES * sizeof(int16_t));
      RxVerifyResult r = verifyRampGeneric(rx_buf, read_bytes, 16, 1, 2);
      free(rx_buf);
      i2s.end();
      reportResult("RX_RECONFIG_BOTH_DONE", r);

    } else if (cmd == "START_RX_TDM") {
#if SOC_I2S_SUPPORTS_TDM
      i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
      if (!i2s.begin(I2S_MODE_TDM, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_TDM_SLOT0, I2S_ROLE_SLAVE)) {
        Serial.println("[RECEIVER] RX_TDM_FAIL begin failed");
        return;
      }
      Serial.println("[RECEIVER] RX_TDM_LISTENING");

      int16_t *rx_buf = (int16_t *)calloc(RX_BUF_SAMPLES, sizeof(int16_t));
      if (!rx_buf) {
        Serial.println("[RECEIVER] RX_TDM_FAIL alloc failed");
        i2s.end();
        return;
      }

      size_t read_bytes = i2s.readBytes((char *)rx_buf, RX_BUF_SAMPLES * sizeof(int16_t));
      RxVerifyResult r = verifyRamp16(rx_buf, read_bytes / sizeof(int16_t));

      free(rx_buf);
      i2s.end();
      reportResult("RX_TDM_DONE", r);
#else
      Serial.println("[RECEIVER] RX_TDM_SKIP");
#endif

    } else if (cmd == "DONE") {
      Serial.println("[RECEIVER] DONE");
    }
  }
  delay(10);
}
