/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S3 && (CONFIG_USE_WAKENET || CONFIG_USE_MULTINET)

#include "ESP_I2S.h"
#include "esp32-hal-sr.h"

typedef void (*sr_cb)(sr_event_t event, int command_id, int phrase_id);

class ESP_SR_Class {
private:
  sr_cb cb;
  I2SClass *i2s;

public:
  ESP_SR_Class();
  ~ESP_SR_Class();

  void onEvent(sr_cb cb);
  bool begin(I2SClass &i2s, const sr_cmd_t *sr_commands, size_t sr_commands_len, sr_channels_t rx_chan = SR_CHANNELS_STEREO, sr_mode_t mode = SR_MODE_WAKEWORD);
  bool end(void);
  bool setMode(sr_mode_t mode);
  bool pause(void);
  bool resume(void);

  void _sr_event(sr_event_t event, int command_id, int phrase_id);
  esp_err_t _fill(void *out, size_t len, size_t *bytes_read, uint32_t timeout_ms);
};

extern ESP_SR_Class ESP_SR;

#endif  // CONFIG_IDF_TARGET_ESP32S3
