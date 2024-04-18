/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "WiFi.h"
#include "WiFiGeneric.h"
#include "WiFiSTA.h"
#if SOC_WIFI_SUPPORTED
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp32-hal.h>
#include <lwip/ip_addr.h>
#include "lwip/err.h"
#include "lwip/dns.h"
#include <esp_smartconfig.h>
#include <esp_netif.h>
#include "esp_mac.h"

#if __has_include("esp_eap_client.h")
#include "esp_eap_client.h"
#else
#include "esp_wpa2.h"
#endif

esp_netif_t* get_esp_interface_netif(esp_interface_t interface);

static size_t _wifi_strncpy(char* dst, const char* src, size_t dst_len) {
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

static STAClass* _sta_network_if = NULL;

static esp_event_handler_instance_t _sta_ev_instance = NULL;
static void _sta_event_cb(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT) {
    ((STAClass*)arg)->_onStaEvent(event_id, event_data);
  }
}

static bool _is_staReconnectableReason(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
    //Timeouts (retry)
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
    case WIFI_REASON_802_1X_AUTH_FAILED:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    //Transient error (reconnect)
    case WIFI_REASON_AUTH_LEAVE:
    case WIFI_REASON_ASSOC_EXPIRE:
    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_NOT_AUTHED:
    case WIFI_REASON_NOT_ASSOCED:
    case WIFI_REASON_ASSOC_NOT_AUTHED:
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
    case WIFI_REASON_INVALID_PMKID:
    case WIFI_REASON_BEACON_TIMEOUT:
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_CONNECTION_FAIL:
    case WIFI_REASON_AP_TSF_RESET:
    case WIFI_REASON_ROAMING:
      return true;
    default:
      return false;
  }
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
static const char* auth_mode_str(int authmode) {
  switch (authmode) {
    case WIFI_AUTH_OPEN:
      return ("OPEN");
      break;
    case WIFI_AUTH_WEP:
      return ("WEP");
      break;
    case WIFI_AUTH_WPA_PSK:
      return ("WPA_PSK");
      break;
    case WIFI_AUTH_WPA2_PSK:
      return ("WPA2_PSK");
      break;
    case WIFI_AUTH_WPA_WPA2_PSK:
      return ("WPA_WPA2_PSK");
      break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return ("WPA2_ENTERPRISE");
      break;
    case WIFI_AUTH_WPA3_PSK:
      return ("WPA3_PSK");
      break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return ("WPA2_WPA3_PSK");
      break;
    case WIFI_AUTH_WAPI_PSK:
      return ("WPAPI_PSK");
      break;
    default:
      break;
  }
  return ("UNKNOWN");
}
#endif

static void _onStaArduinoEvent(arduino_event_t* ev) {
  if (_sta_network_if == NULL || ev->event_id < ARDUINO_EVENT_WIFI_STA_START || ev->event_id > ARDUINO_EVENT_WIFI_STA_LOST_IP) {
    return;
  }
  static bool first_connect = true;
  log_d("Arduino STA Event: %d - %s", ev->event_id, Network.eventName(ev->event_id));

  if (ev->event_id == ARDUINO_EVENT_WIFI_STA_START) {
    _sta_network_if->_setStatus(WL_DISCONNECTED);
    if (esp_wifi_set_ps(WiFi.getSleep()) != ESP_OK) {
      log_e("esp_wifi_set_ps failed");
    }
  } else if (ev->event_id == ARDUINO_EVENT_WIFI_STA_STOP) {
    _sta_network_if->_setStatus(WL_STOPPED);
  } else if (ev->event_id == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    _sta_network_if->_setStatus(WL_IDLE_STATUS);
    if (_sta_network_if->getStatusBits() & ESP_NETIF_WANT_IP6_BIT) {
      esp_err_t err = esp_netif_create_ip6_linklocal(_sta_network_if->netif());
      if (err != ESP_OK) {
        log_e("Failed to enable IPv6 Link Local on STA:  0x%x: %s", err, esp_err_to_name(err));
      } else {
        log_v("Enabled IPv6 Link Local on %s", _sta_network_if->desc());
      }
    }
  } else if (ev->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    uint8_t reason = ev->event_info.wifi_sta_disconnected.reason;
    // Reason 0 causes crash, use reason 1 (UNSPECIFIED) instead
    if (!reason)
      reason = WIFI_REASON_UNSPECIFIED;
    log_w("Reason: %u - %s", reason, WiFi.STA.disconnectReasonName((wifi_err_reason_t)reason));
    if (reason == WIFI_REASON_NO_AP_FOUND) {
      _sta_network_if->_setStatus(WL_NO_SSID_AVAIL);
    } else if ((reason == WIFI_REASON_AUTH_FAIL) && !first_connect) {
      _sta_network_if->_setStatus(WL_CONNECT_FAILED);
    } else if (reason == WIFI_REASON_BEACON_TIMEOUT || reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
      _sta_network_if->_setStatus(WL_CONNECTION_LOST);
    } else if (reason == WIFI_REASON_AUTH_EXPIRE) {

    } else {
      _sta_network_if->_setStatus(WL_DISCONNECTED);
    }

    bool DoReconnect = false;
    if (reason == WIFI_REASON_ASSOC_LEAVE) {  //Voluntarily disconnected. Don't reconnect!
    } else if (first_connect) {               //Retry once for all failure reasons
      first_connect = false;
      DoReconnect = true;
      log_d("WiFi Reconnect Running");
    } else if (_sta_network_if->getAutoReconnect() && _is_staReconnectableReason(reason)) {
      DoReconnect = true;
      log_d("WiFi AutoReconnect Running");
    } else if (reason == WIFI_REASON_ASSOC_FAIL) {
      _sta_network_if->_setStatus(WL_CONNECT_FAILED);
    }
    if (DoReconnect) {
      _sta_network_if->disconnect();
      _sta_network_if->connect();
    }
  } else if (ev->event_id == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    uint8_t* ip = (uint8_t*)&(ev->event_info.got_ip.ip_info.ip.addr);
    uint8_t* mask = (uint8_t*)&(ev->event_info.got_ip.ip_info.netmask.addr);
    uint8_t* gw = (uint8_t*)&(ev->event_info.got_ip.ip_info.gw.addr);
    log_d("STA IP: %u.%u.%u.%u, MASK: %u.%u.%u.%u, GW: %u.%u.%u.%u",
          ip[0], ip[1], ip[2], ip[3],
          mask[0], mask[1], mask[2], mask[3],
          gw[0], gw[1], gw[2], gw[3]);
#endif
    _sta_network_if->_setStatus(WL_CONNECTED);
  } else if (ev->event_id == ARDUINO_EVENT_WIFI_STA_LOST_IP) {
    _sta_network_if->_setStatus(WL_IDLE_STATUS);
  }
}

void STAClass::_onStaEvent(int32_t event_id, void* event_data) {
  arduino_event_t arduino_event;
  arduino_event.event_id = ARDUINO_EVENT_MAX;

  if (event_id == WIFI_EVENT_STA_START) {
    log_v("STA Started");
    arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_START;
    setStatusBits(ESP_NETIF_STARTED_BIT);
  } else if (event_id == WIFI_EVENT_STA_STOP) {
    log_v("STA Stopped");
    arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_STOP;
    clearStatusBits(ESP_NETIF_STARTED_BIT | ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT | ESP_NETIF_HAS_STATIC_IP_BIT);
  } else if (event_id == WIFI_EVENT_STA_AUTHMODE_CHANGE) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    wifi_event_sta_authmode_change_t* event = (wifi_event_sta_authmode_change_t*)event_data;
    log_v("STA Auth Mode Changed: From: %s, To: %s", auth_mode_str(event->old_mode), auth_mode_str(event->new_mode));
#endif
    arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE;
    memcpy(&arduino_event.event_info.wifi_sta_authmode_change, event_data, sizeof(wifi_event_sta_authmode_change_t));
  } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
    log_v("STA Connected: SSID: %s, BSSID: " MACSTR ", Channel: %u, Auth: %s", event->ssid, MAC2STR(event->bssid), event->channel, auth_mode_str(event->authmode));
#endif
    arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_CONNECTED;
    memcpy(&arduino_event.event_info.wifi_sta_connected, event_data, sizeof(wifi_event_sta_connected_t));
    setStatusBits(ESP_NETIF_CONNECTED_BIT);
  } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
    wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
    log_v("STA Disconnected: SSID: %s, BSSID: " MACSTR ", Reason: %u", event->ssid, MAC2STR(event->bssid), event->reason);
#endif
    arduino_event.event_id = ARDUINO_EVENT_WIFI_STA_DISCONNECTED;
    memcpy(&arduino_event.event_info.wifi_sta_disconnected, event_data, sizeof(wifi_event_sta_disconnected_t));
    clearStatusBits(ESP_NETIF_CONNECTED_BIT | ESP_NETIF_HAS_IP_BIT | ESP_NETIF_HAS_LOCAL_IP6_BIT | ESP_NETIF_HAS_GLOBAL_IP6_BIT);
  } else {
    return;
  }

  if (arduino_event.event_id < ARDUINO_EVENT_MAX) {
    Network.postEvent(&arduino_event);
  }
}

STAClass::STAClass()
  : _minSecurity(WIFI_AUTH_WPA2_PSK), _scanMethod(WIFI_FAST_SCAN), _sortMethod(WIFI_CONNECT_AP_BY_SIGNAL), _autoReconnect(true), _status(WL_STOPPED) {
  _sta_network_if = this;
}

STAClass::~STAClass() {
  end();
  _sta_network_if = NULL;
}

wl_status_t STAClass::status() {
  return _status;
}

void STAClass::_setStatus(wl_status_t status) {
  _status = status;
}

/**
 * Sets the working bandwidth of the STA mode
 * @param m wifi_bandwidth_t
 */
bool STAClass::bandwidth(wifi_bandwidth_t bandwidth) {
  if (!begin()) {
    return false;
  }

  esp_err_t err;
  err = esp_wifi_set_bandwidth(WIFI_IF_STA, bandwidth);
  if (err) {
    log_e("Could not set STA bandwidth! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }

  return true;
}

bool STAClass::onEnable() {
  if (_sta_ev_instance == NULL && esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &_sta_event_cb, this, &_sta_ev_instance)) {
    log_e("event_handler_instance_register for WIFI_EVENT Failed!");
    return false;
  }
  if (_esp_netif == NULL) {
    _esp_netif = get_esp_interface_netif(ESP_IF_WIFI_STA);
    if (_esp_netif == NULL) {
      log_e("STA was enabled, but netif is NULL???");
      return false;
    }
    /* attach to receive events */
    Network.onSysEvent(_onStaArduinoEvent);
    initNetif(ESP_NETIF_ID_STA);
  }
  return true;
}

bool STAClass::onDisable() {
  Network.removeEvent(_onStaArduinoEvent);
  // we just set _esp_netif to NULL here, so destroyNetif() does not try to destroy it.
  // That would be done by WiFi.enableSTA(false) if AP is not enabled, or when it gets disabled
  _esp_netif = NULL;
  destroyNetif();
  if (_sta_ev_instance != NULL) {
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &_sta_event_cb);
    _sta_ev_instance = NULL;
  }
  return true;
}

bool STAClass::begin(bool tryConnect) {
  if (!WiFi.enableSTA(true)) {
    log_e("STA enable failed!");
    return false;
  }
  if (tryConnect) {
    return connect();
  }
  return true;
}

bool STAClass::end() {
  if (!WiFi.enableSTA(false)) {
    log_e("STA disable failed!");
    return false;
  }
  return true;
}

bool STAClass::connect() {
  if (_esp_netif == NULL) {
    log_e("STA not started! You must call begin() first.");
    return false;
  }

  if (connected()) {
    log_w("STA already connected.");
    return true;
  }

  wifi_config_t current_conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &current_conf) != ESP_OK || esp_wifi_set_config(WIFI_IF_STA, &current_conf) != ESP_OK) {
    log_e("STA config failed");
    return false;
  }

  if ((getStatusBits() & ESP_NETIF_HAS_STATIC_IP_BIT) == 0 && !config()) {
    log_e("STA failed to configure dynamic IP!");
    return false;
  }

  esp_err_t err = esp_wifi_connect();
  if (err) {
    log_e("STA connect failed! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }
  return true;
}

/**
 * Start Wifi connection
 * if passphrase is set the most secure supported mode will be automatically selected
 * @param ssid const char*          Pointer to the SSID string.
 * @param passphrase const char *   Optional. Passphrase. Valid characters in a passphrase must be between ASCII 32-126 (decimal).
 * @param bssid uint8_t[6]          Optional. BSSID / MAC of AP
 * @param channel                   Optional. Channel of AP
 * @param connect                   Optional. call connect
 * @return
 */
bool STAClass::connect(const char* ssid, const char* passphrase, int32_t channel, const uint8_t* bssid, bool tryConnect) {
  if (_esp_netif == NULL) {
    log_e("STA not started! You must call begin() first.");
    return false;
  }

  if (connected()) {
    log_w("STA currently connected. Disconnecting...");
    if (!disconnect(true, 1000)) {
      return false;
    }
  }

  if (!ssid || *ssid == 0x00 || strlen(ssid) > 32) {
    log_e("SSID too long or missing!");
    return false;
  }

  if (passphrase && strlen(passphrase) > 64) {
    log_e("passphrase too long!");
    return false;
  }

  wifi_config_t conf;
  memset(&conf, 0, sizeof(wifi_config_t));
  conf.sta.channel = channel;
  conf.sta.scan_method = _scanMethod;
  conf.sta.sort_method = _sortMethod;
  conf.sta.threshold.rssi = -127;
  conf.sta.pmf_cfg.capable = true;
  if (ssid != NULL && ssid[0] != 0) {
    _wifi_strncpy((char*)conf.sta.ssid, ssid, 32);
    if (passphrase != NULL && passphrase[0] != 0) {
      conf.sta.threshold.authmode = _minSecurity;
      _wifi_strncpy((char*)conf.sta.password, passphrase, 64);
    }
    if (bssid != NULL) {
      conf.sta.bssid_set = 1;
      memcpy(conf.sta.bssid, bssid, 6);
    }
  }

  esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &conf);
  if (err != ESP_OK) {
    log_e("STA clear config failed! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }

  if ((getStatusBits() & ESP_NETIF_HAS_STATIC_IP_BIT) == 0 && !config()) {
    log_e("STA failed to configure dynamic IP!");
    return false;
  }

  if (tryConnect) {
    esp_err_t err = esp_wifi_connect();
    if (err) {
      log_e("STA connect failed! 0x%x: %s", err, esp_err_to_name(err));
      return false;
    }
  }
  return true;
}

/**
 * Start Wifi connection with a WPA2 Enterprise AP
 * if passphrase is set the most secure supported mode will be automatically selected
 * @param ssid const char*          Pointer to the SSID string.
 * @param method wpa2_method_t      The authentication method of WPA2 (WPA2_AUTH_TLS, WPA2_AUTH_PEAP, WPA2_AUTH_TTLS)
 * @param wpa2_identity  const char*          Pointer to the entity
 * @param wpa2_username  const char*          Pointer to the username
 * @param password const char *     Pointer to the password.
 * @param ca_pem const char*        Pointer to a string with the contents of a  .pem  file with CA cert
 * @param client_crt const char*        Pointer to a string with the contents of a .crt file with client cert
 * @param client_key const char*        Pointer to a string with the contents of a .key file with client key
 * @param bssid uint8_t[6]          Optional. BSSID / MAC of AP
 * @param channel                   Optional. Channel of AP
 * @param connect                   Optional. call connect
 * @return
 */
bool STAClass::connect(const char* wpa2_ssid, wpa2_auth_method_t method, const char* wpa2_identity, const char* wpa2_username, const char* wpa2_password, const char* ca_pem, const char* client_crt, const char* client_key, int32_t channel, const uint8_t* bssid, bool tryConnect) {
  if (_esp_netif == NULL) {
    log_e("STA not started! You must call begin() first.");
    return false;
  }

  if (connected()) {
    log_w("STA currently connected. Disconnecting...");
    if (!disconnect(true, 1000)) {
      return false;
    }
  }

  if (!wpa2_ssid || *wpa2_ssid == 0x00 || strlen(wpa2_ssid) > 32) {
    log_e("SSID too long or missing!");
    return false;
  }

  if (wpa2_identity && strlen(wpa2_identity) > 64) {
    log_e("identity too long!");
    return false;
  }

  if (wpa2_username && strlen(wpa2_username) > 64) {
    log_e("username too long!");
    return false;
  }

  if (wpa2_password && strlen(wpa2_password) > 64) {
    log_e("password too long!");
    return false;
  }

  if (ca_pem) {
#if __has_include("esp_eap_client.h")
    esp_eap_client_set_ca_cert((uint8_t*)ca_pem, strlen(ca_pem));
#else
    esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t*)ca_pem, strlen(ca_pem));
#endif
  }

  if (client_crt) {
#if __has_include("esp_eap_client.h")
    esp_eap_client_set_certificate_and_key((uint8_t*)client_crt, strlen(client_crt), (uint8_t*)client_key, strlen(client_key), NULL, 0);
#else
    esp_wifi_sta_wpa2_ent_set_cert_key((uint8_t*)client_crt, strlen(client_crt), (uint8_t*)client_key, strlen(client_key), NULL, 0);
#endif
  }

#if __has_include("esp_eap_client.h")
  esp_eap_client_set_identity((uint8_t*)wpa2_identity, strlen(wpa2_identity));
#else
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)wpa2_identity, strlen(wpa2_identity));
#endif
  if (method == WPA2_AUTH_PEAP || method == WPA2_AUTH_TTLS) {
#if __has_include("esp_eap_client.h")
    esp_eap_client_set_username((uint8_t*)wpa2_username, strlen(wpa2_username));
    esp_eap_client_set_password((uint8_t*)wpa2_password, strlen(wpa2_password));
#else
    esp_wifi_sta_wpa2_ent_set_username((uint8_t*)wpa2_username, strlen(wpa2_username));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t*)wpa2_password, strlen(wpa2_password));
#endif
  }
#if __has_include("esp_eap_client.h")
  esp_wifi_sta_enterprise_enable();  //set config settings to enable function
#else
  esp_wifi_sta_wpa2_ent_enable();  //set config settings to enable function
#endif

  return connect(wpa2_ssid, NULL, 0, NULL, tryConnect);  //connect to wifi
}

bool STAClass::disconnect(bool eraseap, unsigned long timeout) {
  if (eraseap) {
    if (!started()) {
      log_e("STA not started! You must call begin first.");
      return false;
    }
    wifi_config_t conf;
    memset(&conf, 0, sizeof(wifi_config_t));
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &conf);
    if (err != ESP_OK) {
      log_e("STA clear config failed! 0x%x: %s", err, esp_err_to_name(err));
      return false;
    }
  }

  if (!connected()) {
    log_w("STA already disconnected.");
    return true;
  }

  esp_err_t err = esp_wifi_disconnect();
  if (err != ESP_OK) {
    log_e("STA disconnect failed! 0x%x: %s", err, esp_err_to_name(err));
    return false;
  }

  if (timeout) {
    const unsigned long start = millis();
    while (connected() && ((millis() - start) < timeout)) {
      delay(5);
    }
    if (connected()) {
      return false;
    }
  }

  return true;
}

bool STAClass::reconnect() {
  if (connected()) {
    if (esp_wifi_disconnect() != ESP_OK) {
      return false;
    }
  }
  return esp_wifi_connect() == ESP_OK;
}

bool STAClass::erase() {
  if (!started()) {
    log_e("STA not started! You must call begin first.");
    return false;
  }
  return esp_wifi_restore() == ESP_OK;
}

uint8_t STAClass::waitForConnectResult(unsigned long timeoutLength) {
  //1 and 3 have STA enabled
  if ((WiFiGenericClass::getMode() & WIFI_MODE_STA) == 0) {
    return WL_DISCONNECTED;
  }
  unsigned long start = millis();
  while ((!status() || status() >= WL_DISCONNECTED) && (millis() - start) < timeoutLength) {
    delay(100);
  }
  return status();
}

bool STAClass::setAutoReconnect(bool autoReconnect) {
  _autoReconnect = autoReconnect;
  return true;
}

bool STAClass::getAutoReconnect() {
  return _autoReconnect;
}

void STAClass::setMinSecurity(wifi_auth_mode_t minSecurity) {
  _minSecurity = minSecurity;
}

void STAClass::setScanMethod(wifi_scan_method_t scanMethod) {
  _scanMethod = scanMethod;
}

void STAClass::setSortMethod(wifi_sort_method_t sortMethod) {
  _sortMethod = sortMethod;
}

String STAClass::SSID() const {
  if (!started()) {
    return String();
  }
  wifi_ap_record_t info;
  if (!esp_wifi_sta_get_ap_info(&info)) {
    return String(reinterpret_cast<char*>(info.ssid));
  }
  return String();
}

String STAClass::psk() const {
  if (!started()) {
    return String();
  }
  wifi_config_t conf;
  esp_wifi_get_config((wifi_interface_t)ESP_IF_WIFI_STA, &conf);
  return String(reinterpret_cast<char*>(conf.sta.password));
}

uint8_t* STAClass::BSSID(uint8_t* buff) {
  static uint8_t bssid[6];
  wifi_ap_record_t info;
  if (!started()) {
    return NULL;
  }
  esp_err_t err = esp_wifi_sta_get_ap_info(&info);
  if (buff != NULL) {
    if (err) {
      memset(buff, 0, 6);
    } else {
      memcpy(buff, info.bssid, 6);
    }
    return buff;
  }
  if (!err) {
    memcpy(bssid, info.bssid, 6);
    return reinterpret_cast<uint8_t*>(bssid);
  }
  return NULL;
}

String STAClass::BSSIDstr() {
  uint8_t* bssid = BSSID();
  if (!bssid) {
    return String();
  }
  char mac[18] = { 0 };
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
  return String(mac);
}

int8_t STAClass::RSSI() {
  if (!started()) {
    return 0;
  }
  wifi_ap_record_t info;
  if (!esp_wifi_sta_get_ap_info(&info)) {
    return info.rssi;
  }
  return 0;
}

size_t STAClass::printDriverInfo(Print& out) const {
  size_t bytes = 0;
  wifi_ap_record_t info;
  if (!started()) {
    return bytes;
  }
  if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) {
    return bytes;
  }
  bytes += out.print(",");
  bytes += out.print((const char*)info.ssid);
  bytes += out.print(",CH:");
  bytes += out.print(info.primary);
  bytes += out.print(",RSSI:");
  bytes += out.print(info.rssi);
  bytes += out.print(",");
  if (info.phy_11ax) {
    bytes += out.print("AX");
  } else if (info.phy_11n) {
    bytes += out.print("N");
  } else if (info.phy_11g) {
    bytes += out.print("G");
  } else if (info.phy_11b) {
    bytes += out.print("B");
  }
  if (info.phy_lr) {
    bytes += out.print(",");
    bytes += out.print("LR");
  }

  if (info.authmode == WIFI_AUTH_OPEN) {
    bytes += out.print(",OPEN");
  } else if (info.authmode == WIFI_AUTH_WEP) {
    bytes += out.print(",WEP");
  } else if (info.authmode == WIFI_AUTH_WPA_PSK) {
    bytes += out.print(",WPA_PSK");
  } else if (info.authmode == WIFI_AUTH_WPA2_PSK) {
    bytes += out.print(",WPA2_PSK");
  } else if (info.authmode == WIFI_AUTH_WPA_WPA2_PSK) {
    bytes += out.print(",WPA_WPA2_PSK");
  } else if (info.authmode == WIFI_AUTH_ENTERPRISE) {
    bytes += out.print(",EAP");
  } else if (info.authmode == WIFI_AUTH_WPA3_PSK) {
    bytes += out.print(",WPA3_PSK");
  } else if (info.authmode == WIFI_AUTH_WPA2_WPA3_PSK) {
    bytes += out.print(",WPA2_WPA3_PSK");
  } else if (info.authmode == WIFI_AUTH_WAPI_PSK) {
    bytes += out.print(",WAPI_PSK");
  } else if (info.authmode == WIFI_AUTH_OWE) {
    bytes += out.print(",OWE");
  } else if (info.authmode == WIFI_AUTH_WPA3_ENT_192) {
    bytes += out.print(",WPA3_ENT_SUITE_B_192_BIT");
  }

  return bytes;
}

/**
 * @brief Convert wifi_err_reason_t to a string.
 * @param [in] reason The reason to be converted.
 * @return A string representation of the error code.
 * @note: wifi_err_reason_t values as of Mar 2023 (arduino-esp32 r2.0.7) are: (1-39, 46-51, 67-68, 200-208) and are defined in /tools/sdk/esp32/include/esp_wifi/include/esp_wifi_types.h.
 */
const char* STAClass::disconnectReasonName(wifi_err_reason_t reason) {
  switch (reason) {
    //ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2,0,7)
    case WIFI_REASON_UNSPECIFIED: return "UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD: return "DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD: return "DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC: return "BSS_TRANSITION_DISASSOC";
    case WIFI_REASON_IE_INVALID: return "IE_INVALID";
    case WIFI_REASON_MIC_FAILURE: return "MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: return "GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS: return "IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID: return "GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID: return "PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID: return "AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION: return "UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP: return "INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED: return "802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED: return "CIPHER_SUITE_REJECTED";
    case WIFI_REASON_TDLS_PEER_UNREACHABLE: return "TDLS_PEER_UNREACHABLE";
    case WIFI_REASON_TDLS_UNSPECIFIED: return "TDLS_UNSPECIFIED";
    case WIFI_REASON_SSP_REQUESTED_DISASSOC: return "SSP_REQUESTED_DISASSOC";
    case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT: return "NO_SSP_ROAMING_AGREEMENT";
    case WIFI_REASON_BAD_CIPHER_OR_AKM: return "BAD_CIPHER_OR_AKM";
    case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION: return "NOT_AUTHORIZED_THIS_LOCATION";
    case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS: return "SERVICE_CHANGE_PERCLUDES_TS";
    case WIFI_REASON_UNSPECIFIED_QOS: return "UNSPECIFIED_QOS";
    case WIFI_REASON_NOT_ENOUGH_BANDWIDTH: return "NOT_ENOUGH_BANDWIDTH";
    case WIFI_REASON_MISSING_ACKS: return "MISSING_ACKS";
    case WIFI_REASON_EXCEEDED_TXOP: return "EXCEEDED_TXOP";
    case WIFI_REASON_STA_LEAVING: return "STA_LEAVING";
    case WIFI_REASON_END_BA: return "END_BA";
    case WIFI_REASON_UNKNOWN_BA: return "UNKNOWN_BA";
    case WIFI_REASON_TIMEOUT: return "TIMEOUT";
    case WIFI_REASON_PEER_INITIATED: return "PEER_INITIATED";
    case WIFI_REASON_AP_INITIATED: return "AP_INITIATED";
    case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT: return "INVALID_FT_ACTION_FRAME_COUNT";
    case WIFI_REASON_INVALID_PMKID: return "INVALID_PMKID";
    case WIFI_REASON_INVALID_MDE: return "INVALID_MDE";
    case WIFI_REASON_INVALID_FTE: return "INVALID_FTE";
    case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED: return "TRANSMISSION_LINK_ESTABLISH_FAILED";
    case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED: return "ALTERATIVE_CHANNEL_OCCUPIED";
    case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
    case WIFI_REASON_AP_TSF_RESET: return "AP_TSF_RESET";
    case WIFI_REASON_ROAMING: return "ROAMING";
    case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG: return "ASSOC_COMEBACK_TIME_TOO_LONG";
    default: return "";
  }
}

#endif /* SOC_WIFI_SUPPORTED */
