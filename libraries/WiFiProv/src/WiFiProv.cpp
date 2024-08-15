/*
    WiFiProv.cpp - WiFiProv class for provisioning
    All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/
#include "soc/soc_caps.h"
#if SOC_WIFI_SUPPORTED

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp32-hal.h>
#if __has_include("qrcode.h")
#include "qrcode.h"
#endif

#include <nvs_flash.h>
#if CONFIG_BLUEDROID_ENABLED
#include "network_provisioning/scheme_ble.h"
#endif
#include <network_provisioning/scheme_softap.h>
#include <network_provisioning/manager.h>
#undef IPADDR_NONE
#include "WiFiProv.h"
#if CONFIG_IDF_TARGET_ESP32
#include "SimpleBLE.h"
#endif

bool wifiLowLevelInit(bool persistent);

#if CONFIG_BLUEDROID_ENABLED
static const uint8_t custom_service_uuid[16] = {
  0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
};
#endif

#define SERV_NAME_PREFIX_PROV "PROV_"

static void get_device_service_name(prov_scheme_t prov_scheme, char *service_name, size_t max) {
  uint8_t eth_mac[6] = {0, 0, 0, 0, 0, 0};
  if (esp_wifi_get_mac((wifi_interface_t)WIFI_IF_STA, eth_mac) != ESP_OK) {
    log_e("esp_wifi_get_mac failed!");
    return;
  }
#if CONFIG_IDF_TARGET_ESP32 && defined(CONFIG_BLUEDROID_ENABLED)
  if (prov_scheme == NETWORK_PROV_SCHEME_BLE) {
    snprintf(service_name, max, "%s%02X%02X%02X", SERV_NAME_PREFIX_PROV, eth_mac[3], eth_mac[4], eth_mac[5]);
  } else {
#endif
    snprintf(service_name, max, "%s%02X%02X%02X", SERV_NAME_PREFIX_PROV, eth_mac[3], eth_mac[4], eth_mac[5]);
#if CONFIG_IDF_TARGET_ESP32 && defined(CONFIG_BLUEDROID_ENABLED)
  }
#endif
}

void WiFiProvClass ::beginProvision(
  prov_scheme_t prov_scheme, scheme_handler_t scheme_handler, network_prov_security_t security, const char *pop, const char *service_name,
  const char *service_key, uint8_t *uuid, bool reset_provisioned
) {
  bool provisioned = false;
  static char service_name_temp[32];

  network_prov_mgr_config_t config;
#if CONFIG_BLUEDROID_ENABLED
  if (prov_scheme == NETWORK_PROV_SCHEME_BLE) {
    config.scheme = network_prov_scheme_ble;
  } else {
#endif
    config.scheme = network_prov_scheme_softap;
#if CONFIG_BLUEDROID_ENABLED
  }

  if (scheme_handler == NETWORK_PROV_SCHEME_HANDLER_NONE) {
#endif
    network_prov_event_handler_t scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE;
    memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(network_prov_event_handler_t));
#if CONFIG_BLUEDROID_ENABLED
  } else if (scheme_handler == NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM) {
    network_prov_event_handler_t scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
    memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(network_prov_event_handler_t));
  } else if (scheme_handler == NETWORK_PROV_SCHEME_HANDLER_FREE_BT) {
    network_prov_event_handler_t scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BT;
    memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(network_prov_event_handler_t));
  } else if (scheme_handler == NETWORK_PROV_SCHEME_HANDLER_FREE_BLE) {
    network_prov_event_handler_t scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BLE;
    memcpy(&config.scheme_event_handler, &scheme_event_handler, sizeof(network_prov_event_handler_t));
  } else {
    log_e("Unknown scheme handler!");
    return;
  }
#endif
  config.app_event_handler.event_cb = NULL;
  config.app_event_handler.user_data = NULL;
  WiFi.STA.begin(false);
  if (network_prov_mgr_init(config) != ESP_OK) {
    log_e("network_prov_mgr_init failed!");
    return;
  }
  if (reset_provisioned) {
    log_i("Resetting provisioned data.");
    network_prov_mgr_reset_wifi_provisioning();
  } else if (network_prov_mgr_is_wifi_provisioned(&provisioned) != ESP_OK) {
    log_e("network_prov_mgr_is_wifi_provisioned failed!");
    network_prov_mgr_deinit();
    return;
  }
  if (provisioned == false) {
#if CONFIG_BLUEDROID_ENABLED
    if (prov_scheme == NETWORK_PROV_SCHEME_BLE) {
      service_key = NULL;
      if (uuid == NULL) {
        uuid = (uint8_t *)custom_service_uuid;
      }
      network_prov_scheme_ble_set_service_uuid(uuid);
    }
#endif

    if (service_name == NULL) {
      get_device_service_name(prov_scheme, service_name_temp, 32);
      service_name = (const char *)service_name_temp;
    }

#if CONFIG_BLUEDROID_ENABLED
    if (prov_scheme == NETWORK_PROV_SCHEME_BLE) {
      log_i("Starting AP using BLE. service_name : %s, pop : %s", service_name, pop);
    } else {
#endif
      if (service_key == NULL) {
        log_i("Starting provisioning AP using SOFTAP. service_name : %s, pop : %s", service_name, pop);
      } else {
        log_i("Starting provisioning AP using SOFTAP. service_name : %s, password : %s, pop : %s", service_name, service_key, pop);
      }
#if CONFIG_BLUEDROID_ENABLED
    }
#endif
    if (network_prov_mgr_start_provisioning(security, pop, service_name, service_key) != ESP_OK) {
      log_e("network_prov_mgr_start_provisioning failed!");
      return;
    }
  } else {
    log_i("Already Provisioned");
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
    static wifi_config_t conf;
    esp_wifi_get_config((wifi_interface_t)WIFI_IF_STA, &conf);
    log_i("Attempting connect to AP: %s\n", conf.sta.ssid);
#endif
    esp_wifi_start();
    network_prov_mgr_deinit();
    WiFi.begin();
  }
}

void WiFiProvClass::endProvision() {
  network_prov_mgr_stop_provisioning();
}

bool WiFiProvClass::disableAutoStop(uint32_t cleanup_delay) {
  esp_err_t err = network_prov_mgr_disable_auto_stop(cleanup_delay);
  if (err != ESP_OK) {
    log_e("disable_auto_stop failed!");
  }
  return err == ESP_OK;
}

// Copied from IDF example

#if __has_include("qrcode.h")
static const char *lt[] = {
  /* 0 */ "  ",
  /* 1 */ "\u2580 ",
  /* 2 */ " \u2580",
  /* 3 */ "\u2580\u2580",
  /* 4 */ "\u2584 ",
  /* 5 */ "\u2588 ",
  /* 6 */ "\u2584\u2580",
  /* 7 */ "\u2588\u2580",
  /* 8 */ " \u2584",
  /* 9 */ "\u2580\u2584",
  /* 10 */ " \u2588",
  /* 11 */ "\u2580\u2588",
  /* 12 */ "\u2584\u2584",
  /* 13 */ "\u2588\u2584",
  /* 14 */ "\u2584\u2588",
  /* 15 */ "\u2588\u2588",
};

static Print *qr_out = NULL;

static void _qrcode_print_console(esp_qrcode_handle_t qrcode) {
  int size = esp_qrcode_get_size(qrcode);
  int border = 2;
  unsigned char num = 0;

  if (qr_out == NULL) {
    return;
  }

  for (int y = -border; y < size + border; y += 2) {
    for (int x = -border; x < size + border; x += 2) {
      num = 0;
      if (esp_qrcode_get_module(qrcode, x, y)) {
        num |= 1 << 0;
      }
      if ((x < size + border) && esp_qrcode_get_module(qrcode, x + 1, y)) {
        num |= 1 << 1;
      }
      if ((y < size + border) && esp_qrcode_get_module(qrcode, x, y + 1)) {
        num |= 1 << 2;
      }
      if ((x < size + border) && (y < size + border) && esp_qrcode_get_module(qrcode, x + 1, y + 1)) {
        num |= 1 << 3;
      }
      qr_out->print(lt[num]);
    }
    qr_out->print("\n");
  }
  qr_out->print("\n");
}
#endif

void WiFiProvClass::printQR(const char *name, const char *pop, const char *transport, Print &out) {
  if (!name || !transport) {
    log_w("Cannot generate QR code payload. Data missing.");
    return;
  }
  char payload[150] = {0};
  if (pop) {
    snprintf(
      payload, sizeof(payload),
      "{\"ver\":\"%s\",\"name\":\"%s\""
      ",\"pop\":\"%s\",\"transport\":\"%s\"}",
      "v1", name, pop, transport
    );
  } else {
    snprintf(
      payload, sizeof(payload),
      "{\"ver\":\"%s\",\"name\":\"%s\""
      ",\"transport\":\"%s\"}",
      "v1", name, transport
    );
  }
#if __has_include("qrcode.h")
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = _qrcode_print_console;
  out.printf("Scan this QR code from the provisioning application for Provisioning.\n");
  qr_out = &out;
  esp_qrcode_generate(&cfg, payload);
  qr_out = NULL;
  out.printf("If QR code is not visible, copy paste the below URL in a browser.\nhttps://rainmaker.espressif.com/qrcode.html?data=%s\n", payload);
#else
  out.println("If you are using Arduino as IDF component, install ESP Rainmaker:\nhttps://github.com/espressif/esp-rainmaker");
#endif
}

WiFiProvClass WiFiProv;

#endif /* SOC_WIFI_SUPPORTED */
