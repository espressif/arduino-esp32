// Copyright 2015-2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sdkconfig.h"
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)

#include "esp32-hal-hosted.h"
#include "esp32-hal-log.h"

#include "esp_hosted_transport_config.h"
extern esp_err_t esp_hosted_init();
extern esp_err_t esp_hosted_deinit();

static bool hosted_initialized = false;
static bool hosted_ble_active = false;
static bool hosted_wifi_active = false;

static sdio_pin_config_t sdio_pin_config = {
#ifdef BOARD_HAS_SDIO_ESP_HOSTED
  .pin_clk = BOARD_SDIO_ESP_HOSTED_CLK,
  .pin_cmd = BOARD_SDIO_ESP_HOSTED_CMD,
  .pin_d0 = BOARD_SDIO_ESP_HOSTED_D0,
  .pin_d1 = BOARD_SDIO_ESP_HOSTED_D1,
  .pin_d2 = BOARD_SDIO_ESP_HOSTED_D2,
  .pin_d3 = BOARD_SDIO_ESP_HOSTED_D3,
  .pin_reset = BOARD_SDIO_ESP_HOSTED_RESET
#else
  .pin_clk = CONFIG_ESP_SDIO_PIN_CLK,
  .pin_cmd = CONFIG_ESP_SDIO_PIN_CMD,
  .pin_d0 = CONFIG_ESP_SDIO_PIN_D0,
  .pin_d1 = CONFIG_ESP_SDIO_PIN_D1,
  .pin_d2 = CONFIG_ESP_SDIO_PIN_D2,
  .pin_d3 = CONFIG_ESP_SDIO_PIN_D3,
  .pin_reset = CONFIG_ESP_SDIO_GPIO_RESET_SLAVE
#endif
};

static bool hostedInit() {
  if (!hosted_initialized) {
    log_i("Initializing ESP-Hosted");
    log_d(
      "SDIO pins: clk=%d, cmd=%d, d0=%d, d1=%d, d2=%d, d3=%d, rst=%d", sdio_pin_config.pin_clk, sdio_pin_config.pin_cmd, sdio_pin_config.pin_d0,
      sdio_pin_config.pin_d1, sdio_pin_config.pin_d2, sdio_pin_config.pin_d3, sdio_pin_config.pin_reset
    );
    hosted_initialized = true;
    struct esp_hosted_sdio_config conf = INIT_DEFAULT_HOST_SDIO_CONFIG();
    conf.pin_clk.pin = sdio_pin_config.pin_clk;
    conf.pin_cmd.pin = sdio_pin_config.pin_cmd;
    conf.pin_d0.pin = sdio_pin_config.pin_d0;
    conf.pin_d1.pin = sdio_pin_config.pin_d1;
    conf.pin_d2.pin = sdio_pin_config.pin_d2;
    conf.pin_d3.pin = sdio_pin_config.pin_d3;
    conf.pin_reset.pin = sdio_pin_config.pin_reset;
    // esp_hosted_sdio_set_config() will fail on second attempt but here temporarily to not cause exception on reinit
    if (esp_hosted_sdio_set_config(&conf) != ESP_OK || esp_hosted_init() != ESP_OK) {
      log_e("esp_hosted_init failed!");
      hosted_initialized = false;
      return false;
    }
    log_i("ESP-Hosted initialized!");
  }

  // Attach pins to PeriMan here
  // Slave chip model is CONFIG_IDF_SLAVE_TARGET
  // sdio_pin_config.pin_clk
  // sdio_pin_config.pin_cmd
  // sdio_pin_config.pin_d0
  // sdio_pin_config.pin_d1
  // sdio_pin_config.pin_d2
  // sdio_pin_config.pin_d3
  // sdio_pin_config.pin_reset

  return true;
}

static bool hostedDeinit() {
  if (!hosted_initialized) {
    log_e("ESP-Hosted is not initialized");
    return false;
  }

  if (esp_hosted_deinit() != ESP_OK) {
    log_e("esp_hosted_deinit failed!");
    return false;
  }

  hosted_initialized = false;
  return true;
}

bool hostedInitBLE() {
  log_i("Initializing ESP-Hosted for BLE");
  hosted_ble_active = true;
  return hostedInit();
}

bool hostedInitWiFi() {
  log_i("Initializing ESP-Hosted for WiFi");
  hosted_wifi_active = true;
  return hostedInit();
}

bool hostedDeinitBLE() {
  log_i("Deinitializing ESP-Hosted for BLE");
  hosted_ble_active = false;
  if (!hosted_wifi_active) {
    return hostedDeinit();
  } else {
    log_i("ESP-Hosted is still being used by Wi-Fi. Skipping deinit.");
    return true;
  }
}

bool hostedDeinitWiFi() {
  log_i("Deinitializing ESP-Hosted for WiFi");
  hosted_wifi_active = false;
  if (!hosted_ble_active) {
    return hostedDeinit();
  } else {
    log_i("ESP-Hosted is still being used by BLE. Skipping deinit.");
    return true;
  }
}

bool hostedSetPins(int8_t clk, int8_t cmd, int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t rst) {
  if (clk < 0 || cmd < 0 || d0 < 0 || d1 < 0 || d2 < 0 || d3 < 0 || rst < 0) {
    log_e("All SDIO pins must be defined");
    return false;
  }

  if (hosted_initialized) {
    int8_t current_clk, current_cmd, current_d0, current_d1, current_d2, current_d3, current_rst;
    hostedGetPins(&current_clk, &current_cmd, &current_d0, &current_d1, &current_d2, &current_d3, &current_rst);
    log_e("SDIO pins must be set before ESP-Hosted is initialized");
    log_e(
      "Current pins used: clk=%d, cmd=%d, d0=%d, d1=%d, d2=%d, d3=%d, rst=%d", current_clk, current_cmd, current_d0, current_d1, current_d2, current_d3,
      current_rst
    );
    return false;
  }

  sdio_pin_config.pin_clk = clk;
  sdio_pin_config.pin_cmd = cmd;
  sdio_pin_config.pin_d0 = d0;
  sdio_pin_config.pin_d1 = d1;
  sdio_pin_config.pin_d2 = d2;
  sdio_pin_config.pin_d3 = d3;
  sdio_pin_config.pin_reset = rst;
  return true;
}

void hostedGetPins(int8_t *clk, int8_t *cmd, int8_t *d0, int8_t *d1, int8_t *d2, int8_t *d3, int8_t *rst) {
  *clk = sdio_pin_config.pin_clk;
  *cmd = sdio_pin_config.pin_cmd;
  *d0 = sdio_pin_config.pin_d0;
  *d1 = sdio_pin_config.pin_d1;
  *d2 = sdio_pin_config.pin_d2;
  *d3 = sdio_pin_config.pin_d3;
  *rst = sdio_pin_config.pin_reset;
}

bool hostedIsBLEActive() {
  return hosted_ble_active;
}

bool hostedIsWiFiActive() {
  return hosted_wifi_active;
}

bool hostedIsInitialized() {
  return hosted_initialized;
}

#endif /* defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED) */
