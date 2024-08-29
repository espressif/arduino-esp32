/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "soc/soc_caps.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif_types.h"
#include "esp_eth_driver.h"
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#if SOC_WIFI_SUPPORTED
#include "esp_wifi_types.h"
#include "esp_smartconfig.h"
#include "network_provisioning/network_config.h"
#endif

#if SOC_WIFI_SUPPORTED
static const int WIFI_SCANNING_BIT = BIT0;
static const int WIFI_SCAN_DONE_BIT = BIT1;
#endif

#define NET_HAS_IP6_GLOBAL_BIT 0

ESP_EVENT_DECLARE_BASE(ARDUINO_EVENTS);

typedef enum {
  ARDUINO_EVENT_NONE,
  ARDUINO_EVENT_ETH_START,
  ARDUINO_EVENT_ETH_STOP,
  ARDUINO_EVENT_ETH_CONNECTED,
  ARDUINO_EVENT_ETH_DISCONNECTED,
  ARDUINO_EVENT_ETH_GOT_IP,
  ARDUINO_EVENT_ETH_LOST_IP,
  ARDUINO_EVENT_ETH_GOT_IP6,
#if SOC_WIFI_SUPPORTED
  ARDUINO_EVENT_WIFI_OFF,
  ARDUINO_EVENT_WIFI_READY,
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
  ARDUINO_EVENT_WIFI_FTM_REPORT,
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
#endif
  ARDUINO_EVENT_PPP_START,
  ARDUINO_EVENT_PPP_STOP,
  ARDUINO_EVENT_PPP_CONNECTED,
  ARDUINO_EVENT_PPP_DISCONNECTED,
  ARDUINO_EVENT_PPP_GOT_IP,
  ARDUINO_EVENT_PPP_LOST_IP,
  ARDUINO_EVENT_PPP_GOT_IP6,
  ARDUINO_EVENT_MAX
} arduino_event_id_t;

typedef union {
  ip_event_ap_staipassigned_t wifi_ap_staipassigned;
  ip_event_got_ip_t got_ip;
  ip_event_got_ip6_t got_ip6;
  esp_eth_handle_t eth_connected;
#if SOC_WIFI_SUPPORTED
  wifi_event_sta_scan_done_t wifi_scan_done;
  wifi_event_sta_authmode_change_t wifi_sta_authmode_change;
  wifi_event_sta_connected_t wifi_sta_connected;
  wifi_event_sta_disconnected_t wifi_sta_disconnected;
  wifi_event_sta_wps_er_pin_t wps_er_pin;
  wifi_event_sta_wps_fail_reason_t wps_fail_reason;
  wifi_event_ap_probe_req_rx_t wifi_ap_probereqrecved;
  wifi_event_ap_staconnected_t wifi_ap_staconnected;
  wifi_event_ap_stadisconnected_t wifi_ap_stadisconnected;
  wifi_event_ftm_report_t wifi_ftm_report;
  wifi_sta_config_t prov_cred_recv;
  network_prov_wifi_sta_fail_reason_t prov_fail_reason;
  smartconfig_event_got_ssid_pswd_t sc_got_ssid_pswd;
#endif
} arduino_event_info_t;

typedef struct {
  arduino_event_id_t event_id;
  arduino_event_info_t event_info;
} arduino_event_t;

typedef void (*NetworkEventCb)(arduino_event_id_t event);
typedef std::function<void(arduino_event_id_t event, arduino_event_info_t info)> NetworkEventFuncCb;
typedef void (*NetworkEventSysCb)(arduino_event_t *event);

typedef size_t network_event_handle_t;

class NetworkEvents {
public:
  NetworkEvents();
  ~NetworkEvents();

  network_event_handle_t onEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  network_event_handle_t onEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  network_event_handle_t onEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  void removeEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  void removeEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  void removeEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  void removeEvent(network_event_handle_t event_handle);

  const char *eventName(arduino_event_id_t id);

  void checkForEvent();
  bool postEvent(arduino_event_t *event);

  int getStatusBits();
  int waitStatusBits(int bits, uint32_t timeout_ms);
  int setStatusBits(int bits);
  int clearStatusBits(int bits);

  friend class ESP_NetworkInterface;
  friend class ETHClass;
  friend class PPPClass;
#if SOC_WIFI_SUPPORTED
  friend class STAClass;
  friend class APClass;
  friend class WiFiGenericClass;
#endif

protected:
  bool initNetworkEvents();
  network_event_handle_t onSysEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  network_event_handle_t onSysEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  network_event_handle_t onSysEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

private:
  EventGroupHandle_t _arduino_event_group;
  QueueHandle_t _arduino_event_queue;
  TaskHandle_t _arduino_event_task_handle;
};
