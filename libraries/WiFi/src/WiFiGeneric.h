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

#ifndef ESP32WIFIGENERIC_H_
#define ESP32WIFIGENERIC_H_

#include "esp_err.h"
#include "esp_event.h"
#include <functional>
#include "WiFiType.h"
#include "IPAddress.h"
#include "esp_smartconfig.h"
#include "wifi_provisioning/manager.h"

ESP_EVENT_DECLARE_BASE(ARDUINO_EVENTS);

typedef enum {
	ARDUINO_EVENT_WIFI_READY = 0,
	ARDUINO_EVENT_WIFI_SCAN_DONE,
	ARDUINO_EVENT_WIFI_STA_START,
	ARDUINO_EVENT_WIFI_STA_STOP,
	ARDUINO_EVENT_WIFI_STA_CONNECTED,
	ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
	ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE,
	ARDUINO_EVENT_WIFI_STA_GOT_IP,
	ARDUINO_EVENT_WIFI_STA_GOT_IP6,
	ARDUINO_EVENT_WIFI_STA_LOST_IP,
	ARDUINO_EVENT_WIFI_AP_START,
	ARDUINO_EVENT_WIFI_AP_STOP,
	ARDUINO_EVENT_WIFI_AP_STACONNECTED,
	ARDUINO_EVENT_WIFI_AP_STADISCONNECTED,
	ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED,
	ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED,
	ARDUINO_EVENT_WIFI_AP_GOT_IP6,
	ARDUINO_EVENT_ETH_START,
	ARDUINO_EVENT_ETH_STOP,
	ARDUINO_EVENT_ETH_CONNECTED,
	ARDUINO_EVENT_ETH_DISCONNECTED,
	ARDUINO_EVENT_ETH_GOT_IP,
	ARDUINO_EVENT_ETH_GOT_IP6,
	ARDUINO_EVENT_WPS_ER_SUCCESS,
	ARDUINO_EVENT_WPS_ER_FAILED,
	ARDUINO_EVENT_WPS_ER_TIMEOUT,
	ARDUINO_EVENT_WPS_ER_PIN,
	ARDUINO_EVENT_WPS_ER_PBC_OVERLAP,
	ARDUINO_EVENT_SC_SCAN_DONE,
	ARDUINO_EVENT_SC_FOUND_CHANNEL,
	ARDUINO_EVENT_SC_GOT_SSID_PSWD,
	ARDUINO_EVENT_SC_SEND_ACK_DONE,
	ARDUINO_EVENT_PROV_INIT,
	ARDUINO_EVENT_PROV_DEINIT,
	ARDUINO_EVENT_PROV_START,
	ARDUINO_EVENT_PROV_END,
	ARDUINO_EVENT_PROV_CRED_RECV,
	ARDUINO_EVENT_PROV_CRED_FAIL,
	ARDUINO_EVENT_PROV_CRED_SUCCESS,
	ARDUINO_EVENT_MAX
} arduino_event_id_t;

typedef union {
	wifi_event_sta_scan_done_t wifi_scan_done;
	wifi_event_sta_authmode_change_t wifi_sta_authmode_change;
	wifi_event_sta_connected_t wifi_sta_connected;
	wifi_event_sta_disconnected_t wifi_sta_disconnected;
	wifi_event_sta_wps_er_pin_t wps_er_pin;
	wifi_event_sta_wps_fail_reason_t wps_fail_reason;
	wifi_event_ap_probe_req_rx_t wifi_ap_probereqrecved;
	wifi_event_ap_staconnected_t wifi_ap_staconnected;
	wifi_event_ap_stadisconnected_t wifi_ap_stadisconnected;
	ip_event_ap_staipassigned_t wifi_ap_staipassigned;
	ip_event_got_ip_t got_ip;
	ip_event_got_ip6_t got_ip6;
	smartconfig_event_got_ssid_pswd_t sc_got_ssid_pswd;
	esp_eth_handle_t eth_connected;
	wifi_sta_config_t prov_cred_recv;
	wifi_prov_sta_fail_reason_t prov_fail_reason;
} arduino_event_info_t;

typedef struct{
	arduino_event_id_t event_id;
	arduino_event_info_t event_info;
} arduino_event_t;

typedef void (*WiFiEventCb)(arduino_event_id_t event);
typedef std::function<void(arduino_event_id_t event, arduino_event_info_t info)> WiFiEventFuncCb;
typedef void (*WiFiEventSysCb)(arduino_event_t *event);

typedef size_t wifi_event_id_t;

typedef enum {
    WIFI_POWER_19_5dBm = 78,// 19.5dBm
    WIFI_POWER_19dBm = 76,// 19dBm
    WIFI_POWER_18_5dBm = 74,// 18.5dBm
    WIFI_POWER_17dBm = 68,// 17dBm
    WIFI_POWER_15dBm = 60,// 15dBm
    WIFI_POWER_13dBm = 52,// 13dBm
    WIFI_POWER_11dBm = 44,// 11dBm
    WIFI_POWER_8_5dBm = 34,// 8.5dBm
    WIFI_POWER_7dBm = 28,// 7dBm
    WIFI_POWER_5dBm = 20,// 5dBm
    WIFI_POWER_2dBm = 8,// 2dBm
    WIFI_POWER_MINUS_1dBm = -4// -1dBm
} wifi_power_t;

static const int AP_STARTED_BIT    = BIT0;
static const int AP_HAS_IP6_BIT    = BIT1;
static const int AP_HAS_CLIENT_BIT = BIT2;
static const int STA_STARTED_BIT   = BIT3;
static const int STA_CONNECTED_BIT = BIT4;
static const int STA_HAS_IP_BIT    = BIT5;
static const int STA_HAS_IP6_BIT   = BIT6;
static const int ETH_STARTED_BIT   = BIT7;
static const int ETH_CONNECTED_BIT = BIT8;
static const int ETH_HAS_IP_BIT    = BIT9;
static const int ETH_HAS_IP6_BIT   = BIT10;
static const int WIFI_SCANNING_BIT = BIT11;
static const int WIFI_SCAN_DONE_BIT= BIT12;
static const int WIFI_DNS_IDLE_BIT = BIT13;
static const int WIFI_DNS_DONE_BIT = BIT14;

class WiFiGenericClass
{
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

    static const char * getHostname();
    static bool setHostname(const char * hostname);
    static bool hostname(const String& aHostname) { return setHostname(aHostname.c_str()); }

    static esp_err_t _eventCallback(arduino_event_t *event);

  protected:
    static bool _persistent;
    static bool _long_range;
    static wifi_mode_t _forceSleepLastMode;
    static wifi_ps_type_t _sleepEnabled;

    static int setStatusBits(int bits);
    static int clearStatusBits(int bits);
    
  public:
    static int hostByName(const char *aHostname, IPAddress &aResult);

    static IPAddress calculateNetworkID(IPAddress ip, IPAddress subnet);
    static IPAddress calculateBroadcast(IPAddress ip, IPAddress subnet);
    static uint8_t calculateSubnetCIDR(IPAddress subnetMask);

  protected:
    friend class WiFiSTAClass;
    friend class WiFiScanClass;
    friend class WiFiAPClass;
};

#endif /* ESP32WIFIGENERIC_H_ */
