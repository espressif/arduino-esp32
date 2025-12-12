/*
 ESP8266WiFiSTA.h - esp8266 Wifi support.
 Based on WiFi.h from Ardiono WiFi shield library.
 Copyright (c) 2011-2014 Arduino.  All right reserved.
 Modified by Ivan Grokhotkov, December 2014
 Reworked by Markus Sattler, December 2015

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

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED

#include "WiFiType.h"
#include "WiFiGeneric.h"
#ifdef ESP_IDF_VERSION_MAJOR
#include "esp_event.h"
#endif

typedef enum {
  WPA2_AUTH_TLS = 0,
  WPA2_AUTH_PEAP = 1,
  WPA2_AUTH_TTLS = 2
} wpa2_auth_method_t;

// ----------------------------------------------------------------------------------------------
// ------------------------------------ NEW STA Implementation ----------------------------------
// ----------------------------------------------------------------------------------------------

class STAClass : public NetworkInterface {
public:
  STAClass();
  ~STAClass();

  bool begin(bool tryConnect = false);
  bool end();

  bool bandwidth(wifi_bandwidth_t bandwidth);

  bool connect();
  bool connect(const char *ssid, const char *passphrase = NULL, int32_t channel = 0, const uint8_t *bssid = NULL, bool connect = true);
#if CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
  bool connect(
    const char *wpa2_ssid, wpa2_auth_method_t method, const char *wpa2_identity = NULL, const char *wpa2_username = NULL, const char *wpa2_password = NULL,
    const char *ca_pem = NULL, const char *client_crt = NULL, const char *client_key = NULL, int ttls_phase2_type = -1, int32_t channel = 0,
    const uint8_t *bssid = 0, bool connect = true
  );
#endif /* CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT */
  bool disconnect(bool eraseap = false, unsigned long timeout = 0);
  bool reconnect();
  bool erase();

  uint8_t waitForConnectResult(unsigned long timeoutLength = 60000);

  bool setAutoReconnect(bool autoReconnect);
  bool getAutoReconnect();

  // Next group functions must be called before WiFi.begin()
  void setMinSecurity(wifi_auth_mode_t minSecurity);  // Default is WIFI_AUTH_WPA2_PSK
  void setScanMethod(wifi_scan_method_t scanMethod);  // Default is WIFI_FAST_SCAN
  void setSortMethod(wifi_sort_method_t sortMethod);  // Default is WIFI_CONNECT_AP_BY_SIGNAL

  wl_status_t status();

  String SSID() const;
  String psk() const;
  uint8_t *BSSID(uint8_t *bssid = NULL);
  String BSSIDstr();
  int8_t RSSI();

  const char *disconnectReasonName(wifi_err_reason_t reason);

  // Private Use
  void _setStatus(wl_status_t status);
  void _onStaEvent(int32_t event_id, void *event_data);

protected:
  wifi_auth_mode_t _minSecurity;
  wifi_scan_method_t _scanMethod;
  wifi_sort_method_t _sortMethod;
  bool _autoReconnect;
  wl_status_t _status;
  network_event_handle_t _wifi_sta_event_handle;

  size_t printDriverInfo(Print &out) const;

  friend class WiFiGenericClass;
  bool onEnable();
  bool onDisable();
};

// ----------------------------------------------------------------------------------------------
// ------------------------------- OLD STA API (compatibility) ----------------------------------
// ----------------------------------------------------------------------------------------------

class WiFiSTAClass {
public:
  STAClass STA;

#if CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT
  wl_status_t begin(
    const char *wpa2_ssid, wpa2_auth_method_t method, const char *wpa2_identity = NULL, const char *wpa2_username = NULL, const char *wpa2_password = NULL,
    const char *ca_pem = NULL, const char *client_crt = NULL, const char *client_key = NULL, int ttls_phase2_type = -1, int32_t channel = 0,
    const uint8_t *bssid = 0, bool connect = true
  );
  wl_status_t begin(
    const String &wpa2_ssid, wpa2_auth_method_t method, const String &wpa2_identity = (const char *)NULL, const String &wpa2_username = (const char *)NULL,
    const String &wpa2_password = (const char *)NULL, const String &ca_pem = (const char *)NULL, const String &client_crt = (const char *)NULL,
    const String &client_key = (const char *)NULL, int ttls_phase2_type = -1, int32_t channel = 0, const uint8_t *bssid = 0, bool connect = true
  ) {
    return begin(
      wpa2_ssid.c_str(), method, wpa2_identity.c_str(), wpa2_username.c_str(), wpa2_password.c_str(), ca_pem.c_str(), client_crt.c_str(), client_key.c_str(),
      ttls_phase2_type, channel, bssid, connect
    );
  }
#endif /* CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT */

  wl_status_t begin(const char *ssid, const char *passphrase = NULL, int32_t channel = 0, const uint8_t *bssid = NULL, bool connect = true);
  wl_status_t begin(const String &ssid, const String &passphrase = (const char *)NULL, int32_t channel = 0, const uint8_t *bssid = NULL, bool connect = true) {
    return begin(ssid.c_str(), passphrase.c_str(), channel, bssid, connect);
  }
  wl_status_t begin();

  // also accepts Arduino ordering of parameters: ip, dns, gw, mask
  bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);

  // two and one parameter version. 2nd parameter is DNS like in Arduino
  bool config(IPAddress local_ip, IPAddress dns = (uint32_t)0x00000000);

  bool setDNS(IPAddress dns1, IPAddress dns2 = (uint32_t)0x00000000);  // sets DNS IP for all network interfaces

  bool bandwidth(wifi_bandwidth_t bandwidth);

  bool reconnect();
  bool disconnectAsync(bool wifioff = false, bool eraseap = false);
  bool disconnect(bool wifioff = false, bool eraseap = false, unsigned long timeoutLength = 100);
  bool eraseAP(void);

  bool isConnected();

  bool setAutoReconnect(bool autoReconnect);
  bool getAutoReconnect();

  uint8_t waitForConnectResult(unsigned long timeoutLength = 60000);

  // Next group functions must be called before WiFi.begin()
  void setMinSecurity(wifi_auth_mode_t minSecurity);  // Default is WIFI_AUTH_WPA2_PSK
  void setScanMethod(wifi_scan_method_t scanMethod);  // Default is WIFI_FAST_SCAN
  void setSortMethod(wifi_sort_method_t sortMethod);  // Default is WIFI_CONNECT_AP_BY_SIGNAL

  // STA WiFi info
  wl_status_t status();
  String SSID() const;
  String psk() const;

  uint8_t *BSSID(uint8_t *bssid = NULL);
  String BSSIDstr();

  int8_t RSSI();

  IPAddress localIP();

  uint8_t *macAddress(uint8_t *mac);
  String macAddress();

  IPAddress subnetMask();
  IPAddress gatewayIP();
  IPAddress dnsIP(uint8_t dns_no = 0);

  IPAddress broadcastIP();
  IPAddress networkID();
  uint8_t subnetCIDR();

#if CONFIG_LWIP_IPV6
  bool enableIPv6(bool en = true);
  IPAddress linkLocalIPv6();
  IPAddress globalIPv6();
#endif

  // ----------------------------------------------------------------------------------------------
  // ---------------------------------------- Smart Config ----------------------------------------
  // ----------------------------------------------------------------------------------------------
protected:
  static bool _smartConfigStarted;

public:
  bool beginSmartConfig(smartconfig_type_t type = SC_TYPE_ESPTOUCH, char *crypt_key = NULL);
  bool stopSmartConfig();
  bool smartConfigDone();

  static bool _smartConfigDone;
};

#endif /* SOC_WIFI_SUPPORTED */
