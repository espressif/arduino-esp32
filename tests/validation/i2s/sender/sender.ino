/*
 * I2S Validation Test -- Sender (multi-DUT, generic_multi_device)
 *
 * Runs single-device Unity tests (begin/end, sample rates, bit depth,
 * mono, resource release) then enters serial command mode for cross-device
 * TX loopback with the receiver.
 *
 * Pin-to-pin mapping (generic_multi_device): see ../pins_config.h
 */

#include <Arduino.h>
#include <ESP_I2S.h>
#include <unity.h>
#include "../pins_config.h"

#if !SOC_I2S_SUPPORTED
#error "I2S not supported on this SoC"
#endif

I2SClass i2s;

void setUp(void) {}
void tearDown(void) {}

// ==================== Single-Device Unity Tests ====================

void test_i2s_begin_end(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));
  i2s.end();
}

void test_i2s_sample_rate_8k(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  int16_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = i;
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_sample_rate_44k(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  int16_t buf[128];
  for (int i = 0; i < 128; i++) {
    buf[i] = i;
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_sample_rate_48k(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 48000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  int16_t buf[128];
  for (int i = 0; i < 128; i++) {
    buf[i] = i;
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_32bit(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO));

  int32_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = i * 1000;
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_8bit(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_8BIT, I2S_SLOT_MODE_STEREO));

  int8_t buf[64];
  for (int i = 0; i < 64; i++) {
    buf[i] = (int8_t)(i - 32);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_mono(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  int16_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = i * 50;
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_reopen(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));
  i2s.end();

  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO));
  i2s.end();
}

void test_i2s_slave_mode_begin_end(void) {
  // Verify that begin() accepts I2S_ROLE_SLAVE without error.
  // TX-only: no write is attempted since there is no external clock in this phase.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, -1, I2S_ROLE_SLAVE));
  i2s.end();
}

void test_i2s_mono_aligned_write(void) {
  // Write() must return the full buffer size for sample-aligned 16-bit mono buffers
  // and leave lastError() == 0.  On ESP32 HW v1 the mono workaround is active here;
  // on other targets this is a plain mono write.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  int16_t buf[64];
  for (int i = 0; i < 64; i++) {
    buf[i] = (int16_t)(i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_mono_subbyte_write_no_error(void) {
  // write(uint8_t) is not meaningful for 16-bit PCM; it must return 0
  // and must NOT set an error (fix for sub-sample write in HW v1 workaround path).
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  size_t written = i2s.write((uint8_t)0);
  TEST_ASSERT_EQUAL(0, written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_configure_tx_after_mono_begin(void) {
  // Calling configureTX() while the ESP32 HW v1 mono workaround is active must
  // re-apply the stereo-forced HW config so that write() still works correctly.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  int16_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = (int16_t)(i * 50);
  }

  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);

  // Change sample rate but keep 16-bit mono — workaround must be reapplied.
  TEST_ASSERT_TRUE(i2s.configureTX(8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_configure_rx_after_mono_begin(void) {
  // configureRX() must re-apply the HW v1 workaround (stereo HW + mono SW) when
  // called with 16-bit mono parameters.  RX-only (no DOUT) avoids periman
  // conflict when DIN and DOUT share the same GPIO.
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  // Rate change: re-triggers workaround application inside configureRX.
  TEST_ASSERT_TRUE(i2s.configureRX(8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));
  // Rate back: re-triggers again.
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  i2s.end();
}

void test_i2s_configure_rx_slot_mask(void) {
  // configureRX() with explicit slot_mask must succeed and allow slot changes.
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_RIGHT));
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_LEFT));

  i2s.end();
}

void test_i2s_configure_rx_slot_mask_32bit(void) {
  // Same slot_mask change test at 32-bit (no HW v1 workaround).
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO));

  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_RIGHT));
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_LEFT));

  i2s.end();
}

void test_i2s_stereo_rx_left_slot(void) {
  // Stereo RX with explicit SLOT_LEFT: begin must succeed.
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_LEFT, I2S_ROLE_MASTER));
  i2s.end();
}

void test_i2s_stereo_rx_right_slot(void) {
  // Stereo RX with explicit SLOT_RIGHT: begin must succeed.
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_RIGHT, I2S_ROLE_MASTER));
  i2s.end();
}

void test_i2s_configure_rx_stereo_slot_change(void) {
  // configureRX can switch stereo slot_mask.
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_LEFT));
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_RIGHT));
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_BOTH));

  i2s.end();
}

void test_i2s_configure_rx_mono_both_slot(void) {
  // configureRX with mono BOTH: must succeed (valid use case per user discussion).
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_BOTH));

  i2s.end();
}

void test_i2s_configure_rx_default_slot_preserves(void) {
  // Fix 2 regression: configureRX with default slot_mask (-1) must preserve
  // a previously-set slot instead of resetting to LEFT.
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  // Set slot to RIGHT explicitly
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_RIGHT));

  // Change rate with default slot_mask: slot must stay RIGHT (not reset to LEFT)
  TEST_ASSERT_TRUE(i2s.configureRX(8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  // Change rate back: slot must still be preserved
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  i2s.end();
}

void test_i2s_configure_rx_stereo_to_mono_resets_slot(void) {
  // When transitioning from stereo (where slot_mask is forced to BOTH) to mono
  // with default slot_mask, the slot should reset to LEFT (sensible default).
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  // Switch to mono with default slot_mask: must succeed (resets to LEFT)
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  // Verify we can then set RIGHT explicitly (proves we're in a valid mono state)
  TEST_ASSERT_TRUE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_NONE, I2S_STD_SLOT_RIGHT));

  i2s.end();
}

void test_i2s_configure_rx_reject_wrong_transform_width(void) {
  // Fix 3 regression: I2S_RX_TRANSFORM_16_STEREO_TO_MONO must be rejected
  // when the configured bit width is not 16-bit.
  i2s.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO));

  // 32-bit + stereo-to-mono-16 transform: must fail
  TEST_ASSERT_FALSE(i2s.configureRX(16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO, I2S_RX_TRANSFORM_16_STEREO_TO_MONO));

  i2s.end();
}

// ==================== Slot Mask Combination Tests ====================

void test_i2s_mono_left_slot_write(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT));

  int16_t buf[64];
  for (int i = 0; i < 64; i++) {
    buf[i] = (int16_t)(i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_mono_right_slot_write(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT));

  int16_t buf[64];
  for (int i = 0; i < 64; i++) {
    buf[i] = (int16_t)(i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_mono_both_slot_write(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_BOTH));

  int16_t buf[64];
  for (int i = 0; i < 64; i++) {
    buf[i] = (int16_t)(i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_mono_32bit_write(void) {
  // 32-bit mono must NOT trigger the HW v1 workaround (it only applies to 16-bit).
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO));

  int32_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = i * 1000;
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_mono_right_subbyte_write_no_error(void) {
  // Sub-sample write with explicit RIGHT slot must also return 0 without error.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT));

  size_t written = i2s.write((uint8_t)0);
  TEST_ASSERT_EQUAL(0, written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_stereo_left_slot_write(void) {
  // Stereo + SLOT_LEFT: only left-channel data goes to both wire slots.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_LEFT));

  int16_t buf[128];
  for (int i = 0; i < 64; i++) {
    buf[i * 2] = (int16_t)(i * 100);
    buf[i * 2 + 1] = (int16_t)(10000 + i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_stereo_right_slot_write(void) {
  // Stereo + SLOT_RIGHT: only right-channel data goes to both wire slots.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_RIGHT));

  int16_t buf[128];
  for (int i = 0; i < 64; i++) {
    buf[i * 2] = (int16_t)(i * 100);
    buf[i * 2 + 1] = (int16_t)(10000 + i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_stereo_frame_write(void) {
  // 16-bit stereo: one interleaved frame (left + right int16_t) per write call.
  // This is the pattern used by the Simple_tone example (regression #12668).
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  int16_t frame[2] = {500, 500};
  size_t written = i2s.write((const uint8_t *)frame, sizeof(frame));
  TEST_ASSERT_EQUAL(sizeof(frame), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_stereo_tone_loop_frames(void) {
  // Simulate the Simple_tone loop: multiple stereo frames at 8 kHz.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  int32_t sample = 500;
  for (unsigned int i = 0; i < 16; i++) {
    if (i % 4 == 0) {
      sample = -sample;
    }
    int16_t frame[2] = {(int16_t)sample, (int16_t)sample};
    size_t written = i2s.write((const uint8_t *)frame, sizeof(frame));
    TEST_ASSERT_EQUAL(sizeof(frame), written);
  }
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

static size_t i2s_legacy_byte_pattern_write(I2SClass &bus, int32_t sample) {
  // Pre-3.3.9 Simple_tone pattern: four single-byte writes per stereo sample.
  size_t total = 0;
  total += bus.write((uint8_t)sample);
  total += bus.write((uint8_t)(sample >> 8));
  total += bus.write((uint8_t)sample);
  total += bus.write((uint8_t)(sample >> 8));
  return total;
}

void test_i2s_stereo_legacy_byte_pattern_ignored(void) {
  // Regression #12668: byte-by-byte writes must not send PCM for 16-bit stereo.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  size_t written = i2s_legacy_byte_pattern_write(i2s, 500);
  TEST_ASSERT_EQUAL(0, written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_stereo_subbyte_write_no_error(void) {
  // write(uint8_t) is not meaningful for 16-bit stereo; ignored without error.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  size_t written = i2s.write((uint8_t)0);
  TEST_ASSERT_EQUAL(0, written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_stereo_subsample_buffer_rejected(void) {
  // Buffer writes smaller than one 16-bit sample are ignored.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  uint8_t byte = 0;
  size_t written = i2s.write(&byte, 1);
  TEST_ASSERT_EQUAL(0, written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_stereo_frame_write_after_legacy_pattern(void) {
  // After ignored byte writes, sample-aligned frames must still work.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 8000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  TEST_ASSERT_EQUAL(0, i2s_legacy_byte_pattern_write(i2s, 500));

  int16_t frame[2] = {-500, -500};
  size_t written = i2s.write((const uint8_t *)frame, sizeof(frame));
  TEST_ASSERT_EQUAL(sizeof(frame), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_configure_tx_stereo_to_mono(void) {
  // Start stereo, reconfigure to mono — must succeed and write correctly.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  TEST_ASSERT_TRUE(i2s.configureTX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  int16_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = (int16_t)(i * 50);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_configure_tx_mono_to_stereo(void) {
  // Start mono, reconfigure to stereo — must succeed and write correctly.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO));

  TEST_ASSERT_TRUE(i2s.configureTX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO));

  int16_t buf[64];
  for (int i = 0; i < 64; i++) {
    buf[i] = (int16_t)(i * 50);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_configure_tx_mono_left_to_right(void) {
  // Start mono LEFT, reconfigure to mono RIGHT — both slots must work.
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT));

  int16_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = (int16_t)(i * 50);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);

  TEST_ASSERT_TRUE(i2s.configureTX(16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT));

  written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

// ==================== TDM Mode Tests ====================

#if SOC_I2S_SUPPORTS_TDM

void test_i2s_tdm_begin_end(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_TDM, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_TDM_SLOT0 | I2S_TDM_SLOT1));
  i2s.end();
}

void test_i2s_tdm_stereo_write(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_TDM, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_TDM_SLOT0 | I2S_TDM_SLOT1));

  // Interleaved stereo samples: even index = slot0, odd index = slot1
  int16_t buf[64];
  for (int i = 0; i < 64; i++) {
    buf[i] = (int16_t)(i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_tdm_mono_write(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_TDM, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_TDM_SLOT0));

  int16_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = (int16_t)(i * 100);
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_EQUAL(sizeof(buf), written);
  TEST_ASSERT_EQUAL(0, i2s.lastError());

  i2s.end();
}

void test_i2s_tdm_32bit_write(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_TDM, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO, I2S_TDM_SLOT0 | I2S_TDM_SLOT1));

  int32_t buf[32];
  for (int i = 0; i < 32; i++) {
    buf[i] = i * 1000;
  }
  size_t written = i2s.write((uint8_t *)buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, written);

  i2s.end();
}

void test_i2s_tdm_slave_begin_end(void) {
  i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
  TEST_ASSERT_TRUE(i2s.begin(I2S_MODE_TDM, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_TDM_SLOT0 | I2S_TDM_SLOT1, I2S_ROLE_SLAVE));
  i2s.end();
}

#endif  // SOC_I2S_SUPPORTS_TDM

// ==================== Serial Command Handler (TX Loopback) ====================

static const size_t TX_SAMPLES = 64;
static const int TX_REPEAT = 128;
// DMA has 6 descriptors × 240 frames; pre-fill enough ramp writes to flush
// the auto-clear zeros before signaling the slave to start reading.
static const int TX_PREFILL = 32;

static int32_t expectedVal(size_t j, int bits) {
  switch (bits) {
    case 8:  return (int32_t)(j * 2);
    case 16: return (int32_t)(j * 100);
    case 24: return (int32_t)(j * 50000);
    case 32: return (int32_t)(j * 100000);
    default: return 0;
  }
}

static int32_t expectedValR(size_t j, int bits) {
  switch (bits) {
    case 8:  return (int32_t)(j * 2 + 1);
    case 16: return (int32_t)(10000 + (int32_t)(j * 100));
    case 24: return (int32_t)(5000000 + (int32_t)(j * 50000));
    case 32: return (int32_t)(5000000 + (int32_t)(j * 100000));
    default: return 0;
  }
}

static void fillRamp(void *buf, int bits, int ch) {
  for (size_t j = 0; j < TX_SAMPLES; j++) {
    int32_t val_l = expectedVal(j, bits);
    if (ch == 1) {
      switch (bits) {
        case 8:  ((int8_t *)buf)[j] = (int8_t)val_l; break;
        case 16: ((int16_t *)buf)[j] = (int16_t)val_l; break;
        case 24:
        case 32: ((int32_t *)buf)[j] = val_l; break;
      }
    } else {
      int32_t val_r = expectedValR(j, bits);
      size_t idx = j * 2;
      switch (bits) {
        case 8:
          ((int8_t *)buf)[idx] = (int8_t)val_l;
          ((int8_t *)buf)[idx + 1] = (int8_t)val_r;
          break;
        case 16:
          ((int16_t *)buf)[idx] = (int16_t)val_l;
          ((int16_t *)buf)[idx + 1] = (int16_t)val_r;
          break;
        case 24:
        case 32:
          ((int32_t *)buf)[idx] = val_l;
          ((int32_t *)buf)[idx + 1] = val_r;
          break;
      }
    }
  }
}

void handleSerialCommands(void) {
  Serial.println("[SENDER] TX_READY");

  while (true) {
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();

      if (cmd == "START_TX") {
        i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
        if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
          Serial.println("[SENDER] TX_FAIL begin failed");
          continue;
        }

        int16_t tx_buf[TX_SAMPLES];
        for (size_t i = 0; i < TX_SAMPLES; i++) {
          tx_buf[i] = (int16_t)(i * 100);
        }

        size_t total_written = 0;
        for (int rep = 0; rep < TX_PREFILL; rep++) {
          total_written += i2s.write((uint8_t *)tx_buf, sizeof(tx_buf));
        }

        Serial.println("[SENDER] TX_CLOCKING");
        Serial.flush();

        for (int rep = TX_PREFILL; rep < TX_REPEAT; rep++) {
          total_written += i2s.write((uint8_t *)tx_buf, sizeof(tx_buf));
        }
        delay(100);
        i2s.end();

        Serial.printf("[SENDER] TX_DONE %u\n", (unsigned)total_written);

      } else if (cmd == "START_TX_STEREO_RAMP") {
        i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
        if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
          Serial.println("[SENDER] TX_STEREO_FAIL begin failed");
          continue;
        }

        int16_t stereo_buf[TX_SAMPLES * 2];
        for (size_t i = 0; i < TX_SAMPLES; i++) {
          stereo_buf[i * 2] = 0;
          stereo_buf[i * 2 + 1] = (int16_t)(i * 100);
        }

        size_t total_written = 0;
        for (int rep = 0; rep < TX_PREFILL; rep++) {
          total_written += i2s.write((uint8_t *)stereo_buf, sizeof(stereo_buf));
        }

        Serial.println("[SENDER] TX_STEREO_CLOCKING");
        Serial.flush();

        for (int rep = TX_PREFILL; rep < TX_REPEAT * 2; rep++) {
          total_written += i2s.write((uint8_t *)stereo_buf, sizeof(stereo_buf));
        }
        delay(100);
        i2s.end();

        Serial.printf("[SENDER] TX_STEREO_DONE %u\n", (unsigned)total_written);

      } else if (cmd == "START_TX_MONO_RIGHT") {
        i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
        if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
          Serial.println("[SENDER] TX_MONO_RIGHT_FAIL begin failed");
          continue;
        }

        int16_t tx_buf[TX_SAMPLES];
        for (size_t i = 0; i < TX_SAMPLES; i++) {
          tx_buf[i] = (int16_t)(i * 100);
        }

        size_t total_written = 0;
        for (int rep = 0; rep < TX_PREFILL; rep++) {
          total_written += i2s.write((uint8_t *)tx_buf, sizeof(tx_buf));
        }

        Serial.println("[SENDER] TX_MONO_RIGHT_CLOCKING");
        Serial.flush();

        for (int rep = TX_PREFILL; rep < TX_REPEAT; rep++) {
          total_written += i2s.write((uint8_t *)tx_buf, sizeof(tx_buf));
        }
        delay(100);
        i2s.end();

        Serial.printf("[SENDER] TX_MONO_RIGHT_DONE %u\n", (unsigned)total_written);

      } else if (cmd == "START_TX_LEFT_RAMP") {
        i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
        if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
          Serial.println("[SENDER] TX_LEFT_FAIL begin failed");
          continue;
        }

        int16_t stereo_buf[TX_SAMPLES * 2];
        for (size_t i = 0; i < TX_SAMPLES; i++) {
          stereo_buf[i * 2] = (int16_t)(i * 100);
          stereo_buf[i * 2 + 1] = 0;
        }

        size_t total_written = 0;
        for (int rep = 0; rep < TX_PREFILL; rep++) {
          total_written += i2s.write((uint8_t *)stereo_buf, sizeof(stereo_buf));
        }

        Serial.println("[SENDER] TX_LEFT_CLOCKING");
        Serial.flush();

        for (int rep = TX_PREFILL; rep < TX_REPEAT * 2; rep++) {
          total_written += i2s.write((uint8_t *)stereo_buf, sizeof(stereo_buf));
        }
        delay(100);
        i2s.end();

        Serial.printf("[SENDER] TX_LEFT_DONE %u\n", (unsigned)total_written);

      } else if (cmd.startsWith("TX ")) {
        int rate, bits, ch, slot = -1;
        int n = sscanf(cmd.c_str() + 3, "%d %d %d %d", &rate, &bits, &ch, &slot);
        if (n < 3) {
          Serial.println("[SENDER] TX_PARAM_FAIL parse error");
          continue;
        }

        i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
        if (!i2s.begin(I2S_MODE_STD, (uint32_t)rate, (i2s_data_bit_width_t)bits, (i2s_slot_mode_t)ch, (int8_t)slot)) {
          Serial.println("[SENDER] TX_PARAM_FAIL begin failed");
          continue;
        }

        size_t sample_bytes = (bits <= 8) ? 1 : (bits <= 16) ? 2 : 4;
        size_t buf_bytes = TX_SAMPLES * sample_bytes * ch;
        uint8_t *tx_buf = (uint8_t *)calloc(1, buf_bytes);
        if (!tx_buf) {
          Serial.println("[SENDER] TX_PARAM_FAIL alloc failed");
          i2s.end();
          continue;
        }
        fillRamp(tx_buf, bits, ch);

        int needed = (int)((uint32_t)rate / TX_SAMPLES) + TX_PREFILL;
        int tx_total = (needed > TX_REPEAT) ? needed : TX_REPEAT;

        size_t total_written = 0;
        for (int rep = 0; rep < TX_PREFILL; rep++) {
          total_written += i2s.write(tx_buf, buf_bytes);
        }

        Serial.println("[SENDER] TX_PARAM_CLOCKING");
        Serial.flush();

        for (int rep = TX_PREFILL; rep < tx_total; rep++) {
          total_written += i2s.write(tx_buf, buf_bytes);
        }
        delay(100);
        free(tx_buf);
        i2s.end();

        Serial.printf("[SENDER] TX_PARAM_DONE %u\n", (unsigned)total_written);

      } else if (cmd == "QUERY_HW_V1") {
#if SOC_I2S_HW_VERSION_1
        Serial.println("[SENDER] HW_V1_YES");
#else
        Serial.println("[SENDER] HW_V1_NO");
#endif

      } else if (cmd == "QUERY_TDM_SUPPORT") {
#if SOC_I2S_SUPPORTS_TDM
        Serial.println("[SENDER] TDM_SUPPORTED");
#else
        Serial.println("[SENDER] TDM_NOT_SUPPORTED");
#endif

      } else if (cmd == "START_TX_TDM") {
#if SOC_I2S_SUPPORTS_TDM
        // TDM mono master: single slot (SLOT0), 16-bit, same ramp pattern as START_TX.
        i2s.setPins(I2S_BCLK, I2S_WS, I2S_DOUT, -1);
        if (!i2s.begin(I2S_MODE_TDM, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_TDM_SLOT0)) {
          Serial.println("[SENDER] TX_TDM_FAIL begin failed");
          continue;
        }

        int16_t tdm_buf[TX_SAMPLES];
        for (size_t i = 0; i < TX_SAMPLES; i++) {
          tdm_buf[i] = (int16_t)(i * 100);
        }

        size_t total_written = 0;
        for (int rep = 0; rep < TX_PREFILL; rep++) {
          total_written += i2s.write((uint8_t *)tdm_buf, sizeof(tdm_buf));
        }

        Serial.println("[SENDER] TX_TDM_CLOCKING");
        Serial.flush();

        for (int rep = TX_PREFILL; rep < TX_REPEAT; rep++) {
          total_written += i2s.write((uint8_t *)tdm_buf, sizeof(tdm_buf));
        }
        delay(100);
        i2s.end();

        Serial.printf("[SENDER] TX_TDM_DONE %u\n", (unsigned)total_written);
#else
        Serial.println("[SENDER] TX_TDM_SKIP");
#endif

      } else if (cmd == "DONE") {
        Serial.println("[SENDER] DONE");
        break;
      }
    }
    delay(10);
  }
}

// ==================== Main ====================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("[SENDER] Ready");

  UNITY_BEGIN();

  RUN_TEST(test_i2s_begin_end);
  RUN_TEST(test_i2s_sample_rate_8k);
  RUN_TEST(test_i2s_sample_rate_44k);
  RUN_TEST(test_i2s_sample_rate_48k);
  RUN_TEST(test_i2s_32bit);
  RUN_TEST(test_i2s_8bit);
  RUN_TEST(test_i2s_mono);
  RUN_TEST(test_i2s_reopen);
  RUN_TEST(test_i2s_slave_mode_begin_end);
  RUN_TEST(test_i2s_mono_aligned_write);
  RUN_TEST(test_i2s_mono_subbyte_write_no_error);
  RUN_TEST(test_i2s_configure_tx_after_mono_begin);
  RUN_TEST(test_i2s_configure_rx_after_mono_begin);
  RUN_TEST(test_i2s_configure_rx_slot_mask);
  RUN_TEST(test_i2s_configure_rx_slot_mask_32bit);
  RUN_TEST(test_i2s_mono_left_slot_write);
  RUN_TEST(test_i2s_mono_right_slot_write);
  RUN_TEST(test_i2s_mono_both_slot_write);
  RUN_TEST(test_i2s_stereo_left_slot_write);
  RUN_TEST(test_i2s_stereo_right_slot_write);
  RUN_TEST(test_i2s_stereo_frame_write);
  RUN_TEST(test_i2s_stereo_tone_loop_frames);
  RUN_TEST(test_i2s_stereo_legacy_byte_pattern_ignored);
  RUN_TEST(test_i2s_stereo_subbyte_write_no_error);
  RUN_TEST(test_i2s_stereo_subsample_buffer_rejected);
  RUN_TEST(test_i2s_stereo_frame_write_after_legacy_pattern);
  RUN_TEST(test_i2s_mono_32bit_write);
  RUN_TEST(test_i2s_mono_right_subbyte_write_no_error);
  RUN_TEST(test_i2s_configure_tx_stereo_to_mono);
  RUN_TEST(test_i2s_configure_tx_mono_to_stereo);
  RUN_TEST(test_i2s_configure_tx_mono_left_to_right);
  RUN_TEST(test_i2s_stereo_rx_left_slot);
  RUN_TEST(test_i2s_stereo_rx_right_slot);
  RUN_TEST(test_i2s_configure_rx_stereo_slot_change);
  RUN_TEST(test_i2s_configure_rx_mono_both_slot);
  RUN_TEST(test_i2s_configure_rx_default_slot_preserves);
  RUN_TEST(test_i2s_configure_rx_stereo_to_mono_resets_slot);
  RUN_TEST(test_i2s_configure_rx_reject_wrong_transform_width);
#if SOC_I2S_SUPPORTS_TDM
  RUN_TEST(test_i2s_tdm_begin_end);
  RUN_TEST(test_i2s_tdm_stereo_write);
  RUN_TEST(test_i2s_tdm_mono_write);
  RUN_TEST(test_i2s_tdm_32bit_write);
  RUN_TEST(test_i2s_tdm_slave_begin_end);
#endif

  UNITY_END();

  handleSerialCommands();
}

void loop() {}
