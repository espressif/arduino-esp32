/*
 * Per-target GPIO selection for i2s (generic_multi_device).
 *
 * Pins must be valid for this sketch per .github/CI_README.md (DevKit GPIO
 * Reference): no strapping / flash / USB-JTAG pins, and no conflict with
 * peripherals used here (I2S only — Wire, SPI, and Touch are not initialized).
 * Sender and receiver use the same GPIO numbers (pin-to-pin wiring).
 */

#ifndef I2S_PINS_CONFIG_H
#define I2S_PINS_CONFIG_H

#if CONFIG_IDF_TARGET_ESP32
#define I2S_BCLK 4
#define I2S_WS   13
#define I2S_DOUT 18
#elif CONFIG_IDF_TARGET_ESP32S2
#define I2S_BCLK 4
#define I2S_WS   5
#define I2S_DOUT 19 /* not GPIO18 (LED_BUILTIN / DAC2) */
#elif CONFIG_IDF_TARGET_ESP32S3
#define I2S_BCLK 4
#define I2S_WS   5
#define I2S_DOUT 21
#elif CONFIG_IDF_TARGET_ESP32C3
#define I2S_BCLK 4
#define I2S_WS   5
#define I2S_DOUT 6
#elif CONFIG_IDF_TARGET_ESP32C5
#define I2S_BCLK 1
#define I2S_WS   9
#define I2S_DOUT 10
#elif CONFIG_IDF_TARGET_ESP32C6
#define I2S_BCLK 6
#define I2S_WS   7
#define I2S_DOUT 10
#elif CONFIG_IDF_TARGET_ESP32H2
#define I2S_BCLK 4
#define I2S_WS   5
#define I2S_DOUT 10
#elif CONFIG_IDF_TARGET_ESP32P4
#define I2S_BCLK 20
#define I2S_WS   21
#define I2S_DOUT 22
#else
#error "i2s: add pins for this target in pins_config.h"
#endif

#define I2S_DIN I2S_DOUT

#endif /* I2S_PINS_CONFIG_H */
