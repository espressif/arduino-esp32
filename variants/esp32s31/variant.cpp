/*
 * ESP32-S31 Function CoreBoard 1 onboard audio helpers.
 *
 * The onboard ES8311 codec is wired to the I2C bus on SDA1/SCL1 and to the
 * I2S port whose pins the variant defines. It runs as an I2S slave: the ESP32
 * supplies BCLK, LRCK and MCLK (MCLK = 256 * sample rate), so one register
 * setup covers every sample rate.
 *
 * The prototypes live in pins_arduino.h (pulled in by Arduino.h), so these
 * are callable from any sketch on this board with no extra include.
 *
 * This runs on the same I2C HAL as Arduino Wire (bus 0), so a sketch that
 * later calls Wire.begin() on these pins shares the bus instead of
 * conflicting with it.
 *
 * Licensed under the Apache License, Version 2.0.
 */

#include "Arduino.h"
#include "esp32-hal-i2c.h"
#include "pins_arduino.h"

#define ES8311_I2C_BUS        1
#define ES8311_I2C_TIMEOUT_MS 100

namespace {

/** ES8311 register addresses used by the setup below. */
enum : uint8_t {
  ES8311_REG_RESET = 0x00,
  ES8311_REG_CLK1 = 0x01,
  ES8311_REG_CLK2 = 0x02,
  ES8311_REG_CLK3 = 0x03,
  ES8311_REG_CLK4 = 0x04,
  ES8311_REG_CLK5 = 0x05,
  ES8311_REG_CLK6 = 0x06,
  ES8311_REG_CLK7 = 0x07,
  ES8311_REG_CLK8 = 0x08,
  ES8311_REG_SDPIN = 0x09,
  ES8311_REG_SDPOUT = 0x0A,
  ES8311_REG_SYSTEM0D = 0x0D,
  ES8311_REG_SYSTEM0E = 0x0E,
  ES8311_REG_SYSTEM12 = 0x12,
  ES8311_REG_SYSTEM13 = 0x13,
  ES8311_REG_SYSTEM14 = 0x14,
  ES8311_REG_ADC17 = 0x17,
  ES8311_REG_ADC1C = 0x1C,
  ES8311_REG_DAC31 = 0x31,
  ES8311_REG_DAC32 = 0x32,
  ES8311_REG_DAC37 = 0x37,
};

bool es8311Write(uint8_t reg, uint8_t value) {
  const uint8_t buf[2] = {reg, value};
  return i2cWrite(ES8311_I2C_BUS, ES8311_I2C_ADDR, buf, sizeof(buf), ES8311_I2C_TIMEOUT_MS) == ESP_OK;
}

bool es8311Read(uint8_t reg, uint8_t &value) {
  size_t got = 0;
  return i2cWriteReadNonStop(ES8311_I2C_BUS, ES8311_I2C_ADDR, &reg, 1, &value, 1, ES8311_I2C_TIMEOUT_MS, &got) == ESP_OK && got == 1;
}

bool es8311Update(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current;
  if (!es8311Read(reg, current)) {
    return false;
  }
  return es8311Write(reg, (uint8_t)((current & ~mask) | value));
}

}  // namespace

bool es8311Begin() {
  // Share the bus when Wire (or anything else) already owns it.
  if (!i2cIsInit(ES8311_I2C_BUS)) {
    if (i2cInit(ES8311_I2C_BUS, ES8311_I2C_SDA, ES8311_I2C_SCL, 100000) != ESP_OK) {
      return false;
    }
  }

  bool ok = true;
  // Reset, then power on.
  ok &= es8311Write(ES8311_REG_RESET, 0x1F);
  delay(20);
  ok &= es8311Write(ES8311_REG_RESET, 0x00);
  ok &= es8311Write(ES8311_REG_RESET, 0x80);

  // MCLK from the MCLK pin, all internal clocks enabled.
  ok &= es8311Write(ES8311_REG_CLK1, 0x3F);
  // MCLK pre-divider = 1, no pre-multiplier.
  ok &= es8311Update(ES8311_REG_CLK2, 0xF8, 0x00);
  // ADC/DAC oversampling and dividers for MCLK = 256 * fs.
  ok &= es8311Write(ES8311_REG_CLK3, 0x10);
  ok &= es8311Write(ES8311_REG_CLK4, 0x10);
  ok &= es8311Write(ES8311_REG_CLK5, 0x00);
  ok &= es8311Update(ES8311_REG_CLK6, 0x1F, 0x03);
  ok &= es8311Update(ES8311_REG_CLK7, 0x3F, 0x00);
  ok &= es8311Write(ES8311_REG_CLK8, 0xFF);

  // I2S slave, 16-bit samples in and out.
  ok &= es8311Update(ES8311_REG_RESET, 0x40, 0x00);
  ok &= es8311Write(ES8311_REG_SDPIN, 3 << 2);
  ok &= es8311Write(ES8311_REG_SDPOUT, 3 << 2);

  // Power up the analog path, DAC and headphone/output driver.
  ok &= es8311Write(ES8311_REG_SYSTEM0D, 0x01);
  ok &= es8311Write(ES8311_REG_SYSTEM0E, 0x02);
  ok &= es8311Write(ES8311_REG_SYSTEM12, 0x00);
  ok &= es8311Write(ES8311_REG_SYSTEM13, 0x10);
  // ADC: analog microphone, maximum PGA gain, DC offset canceled.
  ok &= es8311Write(ES8311_REG_SYSTEM14, 0x1A);
  ok &= es8311Write(ES8311_REG_ADC17, 0xC8);
  ok &= es8311Write(ES8311_REG_ADC1C, 0x6A);
  // DAC equalizer bypassed, ~75% digital volume, unmuted.
  ok &= es8311Write(ES8311_REG_DAC37, 0x08);
  ok &= es8311Write(ES8311_REG_DAC32, 0xBF);
  ok &= es8311Update(ES8311_REG_DAC31, 0x60, 0x00);
  return ok;
}

bool es8311SetVolume(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  const uint8_t value = percent == 0 ? 0 : (uint8_t)((percent * 256u) / 100u - 1u);
  return es8311Write(ES8311_REG_DAC32, value);
}
