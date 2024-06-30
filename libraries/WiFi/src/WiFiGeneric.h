/*
 ESP8266WiFiGeneric.h - esp8266 Wifi support.
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
#if SOC_WIFI_SUPPORTED

#include "esp_err.h"
#include "esp_event.h"
#include <functional>
#include "WiFiType.h"
#include "IPAddress.h"
#include "esp_smartconfig.h"
#include "esp_netif_types.h"
#include "esp_eth_driver.h"
#include "wifi_provisioning/manager.h"
#include "lwip/ip_addr.h"

#include "Network.h"

#define WiFiEventCb     NetworkEventCb
#define WiFiEventFuncCb NetworkEventFuncCb
#define WiFiEventSysCb  NetworkEventSysCb
#define wifi_event_id_t network_event_handle_t

typedef enum {
  WIFI_POWER_21dBm = 84,      // 21dBm
  WIFI_POWER_20_5dBm = 82,    // 20.5dBm
  WIFI_POWER_20dBm = 80,      // 20dBm
  WIFI_POWER_19_5dBm = 78,    // 19.5dBm
  WIFI_POWER_19dBm = 76,      // 19dBm
  WIFI_POWER_18_5dBm = 74,    // 18.5dBm
  WIFI_POWER_17dBm = 68,      // 17dBm
  WIFI_POWER_15dBm = 60,      // 15dBm
  WIFI_POWER_13dBm = 52,      // 13dBm
  WIFI_POWER_11dBm = 44,      // 11dBm
  WIFI_POWER_8_5dBm = 34,     // 8.5dBm
  WIFI_POWER_7dBm = 28,       // 7dBm
  WIFI_POWER_5dBm = 20,       // 5dBm
  WIFI_POWER_2dBm = 8,        // 2dBm
  WIFI_POWER_MINUS_1dBm = -4  // -1dBm
} wifi_power_t;

typedef enum {
  WIFI_RX_ANT0 = 0,
  WIFI_RX_ANT1,
  WIFI_RX_ANT_AUTO
} wifi_rx_ant_t;

typedef enum {
  WIFI_TX_ANT0 = 0,
  WIFI_TX_ANT1,
  WIFI_TX_ANT_AUTO
} wifi_tx_ant_t;

class WiFiGenericClass {
public:
  WiFiGenericClass();

  wifi_event_id_t onEvent(WiFiEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  wifi_event_id_t onEvent(WiFiEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  wifi_event_id_t onEvent(WiFiEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  void removeEvent(WiFiEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  void removeEvent(WiFiEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  void removeEvent(wifi_event_id_t id);

  static int getStatusBits();
  static int waitStatusBits(int bits, uint32_t timeout_ms);

  int32_t channel(void);
  int setChannel(uint8_t primary, wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE);

  void persistent(bool persistent);
  void enableLongRange(bool enable);

  static bool mode(wifi_mode_t);
  static wifi_mode_t getMode();

  bool enableSTA(bool enable);
  bool enableAP(bool enable);

  bool setSleep(bool enabled);
  bool setSleep(wifi_ps_type_t sleepType);
  wifi_ps_type_t getSleep();

  bool setTxPower(wifi_power_t power);
  wifi_power_t getTxPower();

  bool initiateFTM(uint8_t frm_count = 16, uint16_t burst_period = 2, uint8_t channel = 1, const uint8_t *mac = NULL);

  static bool setDualAntennaConfig(uint8_t gpio_ant1, uint8_t gpio_ant2, wifi_rx_ant_t rx_mode, wifi_tx_ant_t tx_mode);

  static const char *getHostname();
  static bool setHostname(const char *hostname);
  static bool hostname(const String &aHostname) {
    return setHostname(aHostname.c_str());
  }

  static void useStaticBuffers(bool bufferMode);
  static bool useStaticBuffers();

  static int hostByName(const char *aHostname, IPAddress &aResult);

  static IPAddress calculateNetworkID(IPAddress ip, IPAddress subnet);
  static IPAddress calculateBroadcast(IPAddress ip, IPAddress subnet);
  static uint8_t calculateSubnetCIDR(IPAddress subnetMask);

  const char *disconnectReasonName(wifi_err_reason_t reason);
  const char *eventName(arduino_event_id_t id);

  static void _eventCallback(arduino_event_t *event);

protected:
  static bool _persistent;
  static bool _long_range;
  static wifi_mode_t _forceSleepLastMode;
  static wifi_ps_type_t _sleepEnabled;
  static bool _wifiUseStaticBuffers;

  static int setStatusBits(int bits);
  static int clearStatusBits(int bits);

  friend class WiFiSTAClass;
  friend class WiFiScanClass;
  friend class WiFiAPClass;
  friend class ETHClass;
};

#endif /* SOC_WIFI_SUPPORTED */
