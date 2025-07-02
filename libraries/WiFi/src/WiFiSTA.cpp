/*
 WiFiSTA.cpp - WiFi library for esp32

 Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
 This file is part of the esp8266 core for Arduino environment.

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

 Reworked on 28 Dec 2015 by Markus Sattler

 */

#include "WiFi.h"
#include "WiFiGeneric.h"
#include "WiFiSTA.h"
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED

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

// -----------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------- STA function -----------------------------------------------------
// -----------------------------------------------------------------------------------------------------------------------

/**
 * Return Connection status.
 * @return one of the value defined in wl_status_t
 *
 */
wl_status_t WiFiSTAClass::status() {
  return STA.status();
}

#if CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
wl_status_t WiFiSTAClass::begin(
  const char *wpa2_ssid, wpa2_auth_method_t method, const char *wpa2_identity, const char *wpa2_username, const char *wpa2_password, const char *ca_pem,
  const char *client_crt, const char *client_key, int ttls_phase2_type, int32_t channel, const uint8_t *bssid, bool connect
) {
  if (!STA.begin()) {
    return WL_CONNECT_FAILED;
  }

  if (!STA.connect(wpa2_ssid, method, wpa2_identity, wpa2_username, wpa2_password, ca_pem, client_crt, client_key, ttls_phase2_type, channel, bssid, connect)) {
    return WL_CONNECT_FAILED;
  }

  return STA.status();
}
#endif /* CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT */

wl_status_t WiFiSTAClass::begin(const char *ssid, const char *passphrase, int32_t channel, const uint8_t *bssid, bool connect) {
  if (!STA.begin()) {
    return WL_CONNECT_FAILED;
  }

  if (!STA.connect(ssid, passphrase, channel, bssid, connect)) {
    return WL_CONNECT_FAILED;
  }

  return STA.status();
}

/**
 * Use to connect to SDK config.
 * @return wl_status_t
 */
wl_status_t WiFiSTAClass::begin() {
  if (!STA.begin(true)) {
    return WL_CONNECT_FAILED;
  }

  return STA.status();
}

/**
 * will force a disconnect and then start reconnecting to AP
 * @return true when successful
 */
bool WiFiSTAClass::reconnect() {
  return STA.reconnect();
}

/**
 * Disconnect from the network.
 * @param wifioff `true` to turn the Wi-Fi radio off.
 * @param eraseap `true` to erase the AP configuration from the NVS memory.
 * @return `true` when successful.
 */
bool WiFiSTAClass::disconnectAsync(bool wifioff, bool eraseap) {
  return disconnect(wifioff, eraseap, 0);
}

/**
 * Disconnect from the network.
 * @param wifioff `true` to turn the Wi-Fi radio off.
 * @param eraseap `true` to erase the AP configuration from the NVS memory.
 * @param timeoutLength timeout to wait for status change
 * @return `true` when successful.
 */
bool WiFiSTAClass::disconnect(bool wifioff, bool eraseap, unsigned long timeoutLength) {
  if (!STA.disconnect(eraseap, timeoutLength)) {
    return false;
  }
  if (wifioff) {
    return STA.end();
  }
  return true;
}

/**
 * @brief  Reset WiFi settings in NVS to default values.
 *
 * This function will reset settings made using the following APIs:
 * - esp_wifi_set_bandwidth,
 * - esp_wifi_set_protocol,
 * - esp_wifi_set_config related
 * - esp_wifi_set_mode
 *
 * @return true if erase succeeded
 * @note: Resets SSID, password, protocol, mode, etc.
 * These settings are maintained by WiFi driver in IDF.
 * WiFi driver must be initialized.
 */
bool WiFiSTAClass::eraseAP(void) {
  return STA.erase();
}

/**
 * Change IP configuration settings disabling the dhcp client
 * @param local_ip   Static ip configuration
 * @param gateway    Static gateway configuration
 * @param subnet     Static Subnet mask
 * @param dns1       Static DNS server 1
 * @param dns2       Static DNS server 2
 */
bool WiFiSTAClass::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1, IPAddress dns2) {
  // handle Arduino ordering of parameters: ip, dns, gw, subnet
  if (local_ip.type() == IPv4 && local_ip != INADDR_NONE && subnet[0] != 255) {
    IPAddress tmp = dns1;
    dns1 = gateway;
    gateway = subnet;
    subnet = (tmp != INADDR_NONE) ? tmp : IPAddress(255, 255, 255, 0);
  }

  return STA.begin() && STA.config(local_ip, gateway, subnet, dns1, dns2);
}

bool WiFiSTAClass::config(IPAddress local_ip, IPAddress dns) {

  if (local_ip == INADDR_NONE) {
    return config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }

  if (local_ip.type() != IPv4) {
    return false;
  }

  IPAddress gw(local_ip);
  gw[3] = 1;
  if (dns == INADDR_NONE) {
    dns = gw;
  }
  return config(local_ip, gw, IPAddress(255, 255, 255, 0), dns);
}

/**
 * Change DNS server for static IP configuration
 * @param dns1       Static DNS server 1
 * @param dns2       Static DNS server 2 (optional)
 */
bool WiFiSTAClass::setDNS(IPAddress dns1, IPAddress dns2) {
  return STA.begin() && STA.dnsIP(0, dns1) && STA.dnsIP(1, dns2);
}

/**
 * Sets the working bandwidth of the STA mode
 * @param m wifi_bandwidth_t
 */
bool WiFiSTAClass::bandwidth(wifi_bandwidth_t bandwidth) {
  return STA.bandwidth(bandwidth);
}

/**
 * is STA interface connected?
 * @return true if STA is connected to an AP
 */
bool WiFiSTAClass::isConnected() {
  return STA.connected() && STA.hasIP();
}

/**
 * Set the minimum security for AP to be considered connectable.
 * Must be called before WiFi.begin().
 * @param minSecurity wifi_auth_mode_t
 */
void WiFiSTAClass::setMinSecurity(wifi_auth_mode_t minSecurity) {
  return STA.setMinSecurity(minSecurity);
}

/**
 * Set the way that AP is chosen.
 * First SSID match[WIFI_FAST_SCAN] or Sorted[WIFI_ALL_CHANNEL_SCAN] (RSSI or Security)
 * Must be called before WiFi.begin()
 * @param scanMethod wifi_scan_method_t
 */
void WiFiSTAClass::setScanMethod(wifi_scan_method_t scanMethod) {
  return STA.setScanMethod(scanMethod);
}

/**
 * Set the way that AP is sorted. (requires scanMethod WIFI_ALL_CHANNEL_SCAN)
 * By SSID[WIFI_CONNECT_AP_BY_SIGNAL] or Security[WIFI_CONNECT_AP_BY_SECURITY]
 * Must be called before WiFi.begin()
 * @param sortMethod wifi_sort_method_t
 */
void WiFiSTAClass::setSortMethod(wifi_sort_method_t sortMethod) {
  return STA.setSortMethod(sortMethod);
}

/**
 * Function used to set the automatic reconnection if the connection is lost.
 * @param autoReconnect `true` to enable this option.
 * @return true
 */
bool WiFiSTAClass::setAutoReconnect(bool autoReconnect) {
  return STA.setAutoReconnect(autoReconnect);
}
/**
 * Function used to get the automatic reconnection if the connection is lost.
 * @return The function will return `true` if this setting is enabled.
 */
bool WiFiSTAClass::getAutoReconnect() {
  return STA.getAutoReconnect();
}

/**
 * Wait for WiFi connection to reach a result
 * returns the status reached or disconnect if STA is off
 * @return wl_status_t
 */
uint8_t WiFiSTAClass::waitForConnectResult(unsigned long timeoutLength) {
  return STA.waitForConnectResult(timeoutLength);
}

/**
 * Get the station interface IP address.
 * @return IPAddress station IP
 */
IPAddress WiFiSTAClass::localIP() {
  return STA.localIP();
}

/**
 * Get the station interface MAC address.
 * @param mac   pointer to uint8_t array with length WL_MAC_ADDR_LENGTH
 * @return      pointer to uint8_t *
 */
uint8_t *WiFiSTAClass::macAddress(uint8_t *mac) {
  return STA.macAddress(mac);
}

/**
 * Get the station interface MAC address.
 * @return String mac
 */
String WiFiSTAClass::macAddress(void) {
  return STA.macAddress();
}

/**
 * Get the interface subnet mask address.
 * @return IPAddress subnetMask
 */
IPAddress WiFiSTAClass::subnetMask() {
  return STA.subnetMask();
}

/**
 * Get the gateway ip address.
 * @return IPAddress gatewayIP
 */
IPAddress WiFiSTAClass::gatewayIP() {
  return STA.gatewayIP();
}

/**
 * Get the DNS ip address.
 * @param dns_no
 * @return IPAddress DNS Server IP
 */
IPAddress WiFiSTAClass::dnsIP(uint8_t dns_no) {
  return STA.dnsIP(dns_no);
}

/**
 * Get the broadcast ip address.
 * @return IPAddress broadcastIP
 */
IPAddress WiFiSTAClass::broadcastIP() {
  return STA.broadcastIP();
}

/**
 * Get the network id.
 * @return IPAddress networkID
 */
IPAddress WiFiSTAClass::networkID() {
  return STA.networkID();
}

/**
 * Get the subnet CIDR.
 * @return uint8_t subnetCIDR
 */
uint8_t WiFiSTAClass::subnetCIDR() {
  return STA.subnetCIDR();
}

/**
 * Return the current SSID associated with the network
 * @return SSID
 */
String WiFiSTAClass::SSID() const {
  return STA.SSID();
}

/**
 * Return the current pre shared key associated with the network
 * @return  psk string
 */
String WiFiSTAClass::psk() const {
  return STA.psk();
}

/**
 * Return the current bssid / mac associated with the network if configured
 * @return bssid uint8_t *
 */
uint8_t *WiFiSTAClass::BSSID(uint8_t *buff) {
  return STA.BSSID(buff);
}

/**
 * Return the current bssid / mac associated with the network if configured
 * @return String bssid mac
 */
String WiFiSTAClass::BSSIDstr(void) {
  return STA.BSSIDstr();
}

/**
 * Return the current network RSSI.
 * @return  RSSI value
 */
int8_t WiFiSTAClass::RSSI(void) {
  return STA.RSSI();
}

#if CONFIG_LWIP_IPV6
/**
 * Enable IPv6 on the station interface.
 * Should be called before WiFi.begin()
 *
 * @return true on success
 */
bool WiFiSTAClass::enableIPv6(bool en) {
  return STA.enableIPv6(en);
}

/**
 * Get the station interface link-local IPv6 address.
 * @return IPAddress
 */
IPAddress WiFiSTAClass::linkLocalIPv6() {
  return STA.linkLocalIPv6();
}

/**
 * Get the station interface global IPv6 address.
 * @return IPAddress
 */
IPAddress WiFiSTAClass::globalIPv6() {
  return STA.globalIPv6();
}
#endif

bool WiFiSTAClass::_smartConfigStarted = false;
bool WiFiSTAClass::_smartConfigDone = false;

/**
 * @brief
 *
 * @param type Select type of SmartConfig. Default type is SC_TYPE_ESPTOUCH
 * @param crypt_key When using type SC_TYPE_ESPTOUTCH_V2 crypt key needed, else ignored. Length should be 16 chars.
 * @return true if configuration is successful.
 * @return false if configuration fails.
 */
bool WiFiSTAClass::beginSmartConfig(smartconfig_type_t type, char *crypt_key) {
  esp_err_t err;
  if (_smartConfigStarted) {
    return false;
  }

  if (!WiFi.mode(WIFI_STA)) {
    return false;
  }
  esp_wifi_disconnect();

  smartconfig_start_config_t conf = SMARTCONFIG_START_CONFIG_DEFAULT();

  if (type == SC_TYPE_ESPTOUCH_V2) {
    conf.esp_touch_v2_enable_crypt = true;
    conf.esp_touch_v2_key = crypt_key;
  }

  err = esp_smartconfig_set_type(type);
  if (err != ESP_OK) {
    log_e("SmartConfig Set Type Failed!");
    return false;
  }
  err = esp_smartconfig_start(&conf);
  if (err != ESP_OK) {
    log_e("SmartConfig Start Failed!");
    return false;
  }
  _smartConfigStarted = true;
  _smartConfigDone = false;
  return true;
}

bool WiFiSTAClass::stopSmartConfig() {
  if (!_smartConfigStarted) {
    return true;
  }

  if (esp_smartconfig_stop() == ESP_OK) {
    _smartConfigStarted = false;
    return true;
  }

  return false;
}

bool WiFiSTAClass::smartConfigDone() {
  if (!_smartConfigStarted) {
    return false;
  }

  return _smartConfigDone;
}

#endif /* SOC_WIFI_SUPPORTED */
