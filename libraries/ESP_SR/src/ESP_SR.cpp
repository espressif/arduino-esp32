/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32S3 && (CONFIG_USE_WAKENET || CONFIG_USE_MULTINET)
#include "ESP_SR.h"

static esp_err_t on_sr_fill(void *arg, void *out, size_t len, size_t *bytes_read, uint32_t timeout_ms) {
  return ((ESP_SR_Class *)arg)->_fill(out, len, bytes_read, timeout_ms);
}

static void on_sr_event(void *arg, sr_event_t event, int command_id, int phrase_id) {
  ((ESP_SR_Class *)arg)->_sr_event(event, command_id, phrase_id);
}

ESP_SR_Class::ESP_SR_Class() : cb(NULL), i2s(NULL) {}

ESP_SR_Class::~ESP_SR_Class() {
  end();
}

void ESP_SR_Class::onEvent(sr_cb event_cb) {
  cb = event_cb;
}

bool ESP_SR_Class::begin(I2SClass &_i2s, const sr_cmd_t *sr_commands, size_t sr_commands_len, sr_channels_t rx_chan, sr_mode_t mode) {
  i2s = &_i2s;
  esp_err_t err = sr_start(on_sr_fill, this, rx_chan, mode, sr_commands, sr_commands_len, on_sr_event, this);
  return (err == ESP_OK);
}

bool ESP_SR_Class::end(void) {
  return sr_stop() == ESP_OK;
}

bool ESP_SR_Class::setMode(sr_mode_t mode) {
  return sr_set_mode(mode) == ESP_OK;
}

bool ESP_SR_Class::pause(void) {
  return sr_pause() == ESP_OK;
}

bool ESP_SR_Class::resume(void) {
  return sr_resume() == ESP_OK;
}

void ESP_SR_Class::_sr_event(sr_event_t event, int command_id, int phrase_id) {
  if (cb) {
    cb(event, command_id, phrase_id);
  }
}

esp_err_t ESP_SR_Class::_fill(void *out, size_t len, size_t *bytes_read, uint32_t timeout_ms) {
  if (i2s == NULL) {
    return ESP_FAIL;
  }
  i2s->setTimeout(timeout_ms);
  *bytes_read = i2s->readBytes((char *)out, len);
  return (esp_err_t)i2s->lastError();
}

ESP_SR_Class ESP_SR;

#endif  // CONFIG_IDF_TARGET_ESP32S3
