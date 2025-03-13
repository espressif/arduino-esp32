/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WiFi.h"
#include "WiFiGeneric.h"
#include "WiFiAP.h"
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <esp_err.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <lwip/ip_addr.h>
#include "dhcpserver/dhcpserver_options.h"
#include "esp_netif.h"

esp_netif_t *get_esp_interface_netif(esp_interface_t interface);

static size_t _wifi_strncpy(char *dst, const char *src, size_t dst_len) {
  if (!dst || !src || !dst_len) {
    return 0;
  }
  size_t src_len = strlen(src);
  if (src_len >= dst_len) {
    src_len = dst_len;
  } else {
    src_len += 1;
  }
  memcpy(dst, src, src_len);
  return src_len;
}

/**
 * compare two AP configurations
 * @param lhs softap_config
 * @param rhs softap_config
 * @return equal
 */
static bool softap_config_equal(const wifi_config_t &lhs, const wifi_config_t &rhs) {
  if (strncmp(reinterpret_cast<const char *>(lhs.ap.ssid), reinterpret_cast<const char *>(rhs.ap.ssid), 32) != 0) {
    return false;
  }
  if (strncmp(reinterpret_cast<const char *>(lhs.ap.password), reinterpret_cast<const char *>(rhs.ap.password), 64) != 0) {
    return false;
  }
  if (lhs.ap.channel != rhs.ap.channel) {
    return false;
  }
  if (lhs.ap.authmode != rhs.ap.authmode) {
    return false;
  }
  if (lhs.ap.ssid_hidden != rhs.ap.ssid_hidden) {
    return false;
  }
  if (lhs.ap.max_connection != rhs.ap.max_connection) {
    return false;
  }
  if (lhs.ap.pairwise_cipher != rhs.ap.pairwise_cipher) {
    return false;
  }
  if (lhs.ap.ftm_responder != rhs.ap.ftm_responder) {
    return false;
  }
  return true;
}

void APClass::_onApArduinoEvent(arduino_event_id_t event, const arduino_event_info_t *info) {
  if (event < ARDUINO_EVENT_WIFI_AP_START || event > ARDUINO_EVENT_WIFI_AP_GOT_IP6) {
    return;
  }
  log_v("Arduino AP Event: %d - %s", event, NetworkEvents::eventName(event));
  if (event == ARDUINO_EVENT_WIFI_AP_START) {
#if CONFIG_LWIP_IPV6
    if (getStatusBits() & ESP_NETIF_WANT_IP6_BIT) {
      esp_err_t err = esp_netif_create_ip6_linklocal(netif());
      if (err != ESP_OK) {
        log_e("Failed to enable IPv6 Link Local on AP:  0x%x: %s", err, esp_err_to_name(err));
      } else {
        log_v("Enabled IPv6 Link Local on %s", desc());
      }
    }
#endif
  }
}

void APClass::_onApEvent(int32_t event_id, void *event_data) {
  switch (event_id){
    case WIFI_EVENT_AP_START :
      log_v("AP Started");
      setStatusBits(ESP_NETIF_STARTED_BIT);
      Network.postEvent(ARDUINO_EVENT_WIFI_AP_START);
      return;
    case WIFI_EVENT_AP_STOP :
      log_v("AP Stopped");
      clearStatusBits(ESP_NETIF_STARTED_BIT | ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT);
      Network.postEvent(ARDUINO_EVENT_WIFI_AP_STOP);
      return;
    case WIFI_EVENT_AP_PROBEREQRECVED : {
      #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
      wifi_event_ap_probe_req_rx_t *event = (wifi_event_ap_probe_req_rx_t *)event_data;
      log_v("AP Probe Request: RSSI: %d, MAC: " MACSTR, event->rssi, MAC2STR(event->mac));
      #endif
      arduino_event_info_t i;
      memcpy(&i.wifi_ap_probereqrecved, event_data, sizeof(wifi_event_ap_probe_req_rx_t));
      Network.postEvent(ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED, &i);
      return;
    }
    case WIFI_EVENT_AP_STACONNECTED : {
      #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
      wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
      log_v("AP Station Connected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
      #endif
      setStatusBits(ESP_NETIF_CONNECTED_BIT);
      arduino_event_info_t i;
      memcpy(&i.wifi_ap_staconnected, event_data, sizeof(wifi_event_ap_staconnected_t));
      Network.postEvent(ARDUINO_EVENT_WIFI_AP_STACONNECTED, &i);
      return;
    }
    case WIFI_EVENT_AP_STADISCONNECTED : {
      #if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
      wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
      log_v("AP Station Disconnected: MAC: " MACSTR ", AID: %d", MAC2STR(event->mac), event->aid);
      #endif
      // If no more clients are left
      wifi_sta_list_t clients;
      if (esp_wifi_ap_get_sta_list(&clients) != ESP_OK || clients.num == 0) {
        clearStatusBits(ESP_NETIF_CONNECTED_BIT);
      }
      arduino_event_info_t i;
      memcpy(&i.wifi_ap_stadisconnected, event_data, sizeof(wifi_event_ap_stadisconnected_t));
      Network.postEvent(ARDUINO_EVENT_WIFI_AP_STADISCONNECTED, &i);
      return;
    }
    default :
      return;
  }
}

APClass::APClass() {}

APClass::~APClass() {
  end();
  Network.removeEvent(_evt_handle);
  if (_ap_ev_instance != NULL) {
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _ap_ev_instance);
    _ap_ev_instance = NULL;
  }
}

bool APClass::onEnable() {
  if (!_ap_ev_instance){
    esp_err_t err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
      [](void* self, esp_event_base_t base, int32_t id, void* data) { static_cast<APClass*>(self)->_onApEvent(id, data); },
      this,
      &_ap_ev_instance
    );
    if (err){
      log_e("event_handler_instance_register for WIFI_EVENT Failed!");
      return false;
    }
  }

  if (_esp_netif == NULL) {
    if (!_evt_handle)
      _evt_handle = Network.onSysEvent([this](arduino_event_id_t event, const arduino_event_info_t *info){_onApArduinoEvent(event, info);});
    _esp_netif = get_esp_interface_netif(ESP_IF_WIFI_AP);
    /* attach to receive events */
    initNetif(ESP_NETIF_ID_AP);
  }
  return true;
}

bool APClass::onDisable() {
  Network.removeEvent(_evt_handle);
  _evt_handle = 0;
  // we just set _esp_netif to NULL here, so destroyNetif() does not try to destroy it.
  // That would be done by WiFi.enableAP(false) if STA is not enabled, or when it gets disabled
  _esp_netif = NULL;
  destroyNetif();
  if (_ap_ev_instance != NULL) {
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, _ap_ev_instance);
    _ap_ev_instance = NULL;
  }
  return true;
}

bool APClass::begin() {
  if (!WiFi.enableAP(true)) {
    log_e("AP enable failed!");
    return false;
  }
  if (!waitStatusBits(ESP_NETIF_STARTED_BIT, 1000)) {
    log_e("Failed to start AP!");
    return false;
  }
  return true;
}

bool APClass::end() {
  if (!WiFi.enableAP(false)) {
    log_e("AP disable failed!");
    return false;
  }
  return true;
}

bool APClass::create(
  const char *ssid, const char *passphrase, int channel, int ssid_hidden, int max_connection, bool ftm_responder, wifi_auth_mode_t auth_mode,
  wifi_cipher_type_t cipher
) {
  if (!ssid || *ssid == 0) {
    log_e("SSID missing!");
    return false;
  }

  if (passphrase && (strlen(passphrase) > 0 && strlen(passphrase) < 8)) {
    log_e("passphrase too short!");
    return false;
  }

  if (!begin()) {
    return false;
  }

  wifi_config_t conf;
  memset(&conf, 0, sizeof(wifi_config_t));
  conf.ap.channel = channel;
  conf.ap.max_connection = max_connection;
  conf.ap.beacon_interval = 100;
  conf.ap.ssid_hidden = ssid_hidden;
  conf.ap.ftm_responder = ftm_responder;
  if (ssid != NULL && ssid[0] != 0) {
    _wifi_strncpy((char *)conf.ap.ssid, ssid, 32);
    conf.ap.ssid_len = strlen(ssid);
    if (passphrase != NULL && passphrase[0] != 0) {
      conf.ap.authmode = auth_mode;
      conf.ap.pairwise_cipher = cipher;
      _wifi_strncpy((char *)conf.ap.password, passphrase, 64);
    }
  }

  wifi_config_t conf_current;
  esp_err_t err = esp_wifi_get_config(WIFI_IF_AP, &conf_current);
  if (err) {
    log_e("Get AP config failed! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }
  if (!softap_config_equal(conf, conf_current)) {
    err = esp_wifi_set_config(WIFI_IF_AP, &conf);
    if (err) {
      log_e("Set AP config failed! 0x%x: %s", err, esp_err_to_name(err));
      return false;
    }
  }

  return true;
}

bool APClass::clear() {
  if (!begin()) {
    return false;
  }
  wifi_config_t conf;
  memset(&conf, 0, sizeof(wifi_config_t));
  conf.ap.channel = 1;
  conf.ap.max_connection = 4;
  conf.ap.beacon_interval = 100;
  esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &conf);
  if (err) {
    log_e("Set AP config failed! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool APClass::bandwidth(wifi_bandwidth_t bandwidth) {
  if (!begin()) {
    return false;
  }
  esp_err_t err = esp_wifi_set_bandwidth(WIFI_IF_AP, bandwidth);
  if (err) {
    log_e("Could not set AP bandwidth! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }

  return true;
}

bool APClass::enableNAPT(bool enable) {
  if (!started()) {
    log_e("AP must be first started to enable/disable NAPT");
    return false;
  }
  esp_err_t err = ESP_OK;
  if (enable) {
    err = esp_netif_napt_enable(_esp_netif);
  } else {
    err = esp_netif_napt_disable(_esp_netif);
  }
  if (err) {
    log_e("Could not set enable/disable NAPT! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

String APClass::SSID(void) const {
  if (!started()) {
    return String();
  }
  wifi_config_t info;
  if (!esp_wifi_get_config(WIFI_IF_AP, &info)) {
    return String(reinterpret_cast<char *>(info.ap.ssid));
  }
  return String();
}

uint8_t APClass::stationCount() {
  wifi_sta_list_t clients;
  if (!started()) {
    return 0;
  }
  if (esp_wifi_ap_get_sta_list(&clients) == ESP_OK) {
    return clients.num;
  }
  return 0;
}

size_t APClass::printDriverInfo(Print &out) const {
  size_t bytes = 0;
  wifi_config_t info;
  wifi_sta_list_t clients;
  if (!started()) {
    return bytes;
  }
  if (esp_wifi_get_config(WIFI_IF_AP, &info) != ESP_OK) {
    return bytes;
  }
  bytes += out.print(",");
  bytes += out.print((const char *)info.ap.ssid);
  bytes += out.print(",CH:");
  bytes += out.print(info.ap.channel);

  if (info.ap.authmode == WIFI_AUTH_OPEN) {
    bytes += out.print(",OPEN");
  } else if (info.ap.authmode == WIFI_AUTH_WEP) {
    bytes += out.print(",WEP");
  } else if (info.ap.authmode == WIFI_AUTH_WPA_PSK) {
    bytes += out.print(",WPA_PSK");
  } else if (info.ap.authmode == WIFI_AUTH_WPA2_PSK) {
    bytes += out.print(",WPA2_PSK");
  } else if (info.ap.authmode == WIFI_AUTH_WPA_WPA2_PSK) {
    bytes += out.print(",WPA_WPA2_PSK");
  } else if (info.ap.authmode == WIFI_AUTH_ENTERPRISE) {
    bytes += out.print(",WEAP");
  } else if (info.ap.authmode == WIFI_AUTH_WPA3_PSK) {
    bytes += out.print(",WPA3_PSK");
  } else if (info.ap.authmode == WIFI_AUTH_WPA2_WPA3_PSK) {
    bytes += out.print(",WPA2_WPA3_PSK");
  } else if (info.ap.authmode == WIFI_AUTH_WAPI_PSK) {
    bytes += out.print(",WAPI_PSK");
  } else if (info.ap.authmode == WIFI_AUTH_OWE) {
    bytes += out.print(",OWE");
  } else if (info.ap.authmode == WIFI_AUTH_WPA3_ENT_192) {
    bytes += out.print(",WPA3_ENT_SUITE_B_192_BIT");
  }

  if (esp_wifi_ap_get_sta_list(&clients) == ESP_OK) {
    bytes += out.print(",STA:");
    bytes += out.print(clients.num);
  }
  return bytes;
}

#endif /* SOC_WIFI_SUPPORTED */
