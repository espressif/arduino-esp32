/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "sdkconfig.h"
#include "soc/soc_caps.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif_types.h"
#if CONFIG_ETH_ENABLED
#include "esp_eth_driver.h"
#endif
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
#include <mutex>
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED
#include "esp_wifi_types.h"
#include "esp_smartconfig.h"
#if CONFIG_NETWORK_PROV_NETWORK_TYPE_WIFI
#include "network_provisioning/network_config.h"
#endif
#endif

#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED
constexpr int WIFI_SCANNING_BIT = BIT0;
constexpr int WIFI_SCAN_DONE_BIT = BIT1;
#endif

#define NET_HAS_IP6_GLOBAL_BIT 0

ESP_EVENT_DECLARE_BASE(ARDUINO_EVENTS);

typedef enum {
  ARDUINO_EVENT_NONE = 0,
  ARDUINO_EVENT_ETH_START,
  ARDUINO_EVENT_ETH_STOP,
  ARDUINO_EVENT_ETH_CONNECTED,
  ARDUINO_EVENT_ETH_DISCONNECTED,
  ARDUINO_EVENT_ETH_GOT_IP,
  ARDUINO_EVENT_ETH_LOST_IP,
  ARDUINO_EVENT_ETH_GOT_IP6,
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED
  ARDUINO_EVENT_WIFI_OFF = 100,
  ARDUINO_EVENT_WIFI_READY,
  ARDUINO_EVENT_WIFI_SCAN_DONE,
  ARDUINO_EVENT_WIFI_FTM_REPORT,
  ARDUINO_EVENT_WIFI_STA_START = 110,
  ARDUINO_EVENT_WIFI_STA_STOP,
  ARDUINO_EVENT_WIFI_STA_CONNECTED,
  ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
  ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE,
  ARDUINO_EVENT_WIFI_STA_GOT_IP,
  ARDUINO_EVENT_WIFI_STA_GOT_IP6,
  ARDUINO_EVENT_WIFI_STA_LOST_IP,
  ARDUINO_EVENT_WIFI_AP_START = 130,
  ARDUINO_EVENT_WIFI_AP_STOP,
  ARDUINO_EVENT_WIFI_AP_STACONNECTED,
  ARDUINO_EVENT_WIFI_AP_STADISCONNECTED,
  ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED,
  ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED,
  ARDUINO_EVENT_WIFI_AP_GOT_IP6,
  ARDUINO_EVENT_WPS_ER_SUCCESS = 140,
  ARDUINO_EVENT_WPS_ER_FAILED,
  ARDUINO_EVENT_WPS_ER_TIMEOUT,
  ARDUINO_EVENT_WPS_ER_PIN,
  ARDUINO_EVENT_WPS_ER_PBC_OVERLAP,
  ARDUINO_EVENT_SC_SCAN_DONE = 150,
  ARDUINO_EVENT_SC_FOUND_CHANNEL,
  ARDUINO_EVENT_SC_GOT_SSID_PSWD,
  ARDUINO_EVENT_SC_SEND_ACK_DONE,
  ARDUINO_EVENT_PROV_INIT = 160,
  ARDUINO_EVENT_PROV_DEINIT,
  ARDUINO_EVENT_PROV_START,
  ARDUINO_EVENT_PROV_END,
  ARDUINO_EVENT_PROV_CRED_RECV,
  ARDUINO_EVENT_PROV_CRED_FAIL,
  ARDUINO_EVENT_PROV_CRED_SUCCESS,
#endif
  ARDUINO_EVENT_PPP_START = 200,
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
#if CONFIG_ETH_ENABLED
  esp_eth_handle_t eth_connected;
#endif
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED
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
#endif
#if SOC_WIFI_SUPPORTED
  wifi_sta_config_t prov_cred_recv;
#if CONFIG_NETWORK_PROV_NETWORK_TYPE_WIFI
  network_prov_wifi_sta_fail_reason_t prov_fail_reason;
#endif
  smartconfig_event_got_ssid_pswd_t sc_got_ssid_pswd;
#endif
} arduino_event_info_t;

/**
 * @brief struct combines arduino event id and event's data object
 *
 */
struct arduino_event_t {
  arduino_event_id_t event_id;
  arduino_event_info_t event_info;
};

// type aliases
using NetworkEventCb = void (*)(arduino_event_id_t event);
using NetworkEventFuncCb = std::function<void(arduino_event_id_t event, arduino_event_info_t info)>;
using NetworkEventSysCb = void (*)(arduino_event_t *event);
using network_event_handle_t = size_t;

/**
 * @brief Class that provides network events callback handling
 * it registers user callback functions for event handling,
 * maintains the queue of events and propagates the event among subscribed callbacks
 * callback are called in the context of a dedicated task consuming the queue
 *
 */
class NetworkEvents {
public:
  NetworkEvents();
  ~NetworkEvents();

  /**
   * @brief register callback function to be executed on arduino event(s)
   * @note if same handler is registered twice or more than same handler would be called twice or more times
   *
   * @param cbEvent static callback function
   * @param event event to process, any event by default
   * @return network_event_handle_t
   */
  network_event_handle_t onEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

  /**
   * @brief register functional callback to be executed on arduino event(s)
   * also used for lambda callbacks
   * @note if same handler is registered twice or more than same handler would be called twice or more times
   *
   * @param cbEvent static callback function
   * @param event event to process, any event by default
   * @return network_event_handle_t
   */
  network_event_handle_t onEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

  /**
   * @brief register static system callback to be executed on arduino event(s)
   * callback function would be supplied with a pointer to arduino_event_t structure as an argument
   *
   * @note if same handler is registered twice or more than same handler would be called twice or more times
   *
   * @param cbEvent static callback function
   * @param event event to process, any event by default
   * @return network_event_handle_t
   */
  network_event_handle_t onEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

  /**
   * @brief unregister static function callback
   * @note a better way to unregister callbacks is to save/unregister via network_event_handle_t
   *
   * @param cbEvent static callback function
   * @param event event to process, any event by default
   */
  void removeEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

  /**
   * @brief unregister functional callback
   * @note a better way to unregister callbacks is to save/unregister via network_event_handle_t
   * @note this does not work for lambda's! Do unregister via network_event_handle_t
   *
   * @param cbEvent functional callback
   * @param event event to process, any event by default
   */
  void removeEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX)
    __attribute__((deprecated("removing functional callbacks via pointer is deprecated, use removeEvent(network_event_handle_t) instead")));

  /**
   * @brief unregister static system function callback
   * @note a better way to unregister callbacks is to save/unregister via network_event_handle_t
   *
   * @param cbEvent static callback function
   * @param event event to process, any event by default
   */
  void removeEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

  /**
   * @brief unregister event callback via handler
   *
   * @param cbEvent static callback function
   * @param event event to process, any event by default
   */
  void removeEvent(network_event_handle_t event_handle);

  /**
   * @brief get a human-readable name of an event by it's id
   *
   * @param id event id code
   * @return const char* event name string
   */
  static const char *eventName(arduino_event_id_t id);

  /**
   * @brief post an event to the queue
   * and propagade and event to subscribed handlers
   * @note posting an event will trigger context switch from a lower priority task
   *
   * @param event a pointer to arduino_event_t struct
   * @return true if event was queued susccessfuly
   * @return false on memrory allocation error or queue is full
   */
  bool postEvent(const arduino_event_t *event);

  int getStatusBits() const;
  int waitStatusBits(int bits, uint32_t timeout_ms);
  int setStatusBits(int bits);
  int clearStatusBits(int bits);

  friend class ESP_NetworkInterface;
  friend class ETHClass;
  friend class PPPClass;
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED
  friend class STAClass;
  friend class APClass;
  friend class WiFiGenericClass;
#endif

protected:
  bool initNetworkEvents();
  // same as onEvent() but places newly added handler at the beginning of registered events list
  network_event_handle_t onSysEvent(NetworkEventCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  // same as onEvent() but places newly added handler at the beginning of registered events list
  network_event_handle_t onSysEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);
  // same as onEvent() but places newly added handler at the beginning of registered events list
  network_event_handle_t onSysEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event = ARDUINO_EVENT_MAX);

private:
  /**
   * @brief an object holds callback's definitions:
   * - callback id
   * - callback function pointers
   * - binded event id
   *
   */
  struct NetworkEventCbList_t {
    network_event_handle_t id;
    NetworkEventCb cb;
    NetworkEventFuncCb fcb;
    NetworkEventSysCb scb;
    arduino_event_id_t event;

    explicit NetworkEventCbList_t(
      network_event_handle_t id, NetworkEventCb cb = nullptr, NetworkEventFuncCb fcb = nullptr, NetworkEventSysCb scb = nullptr,
      arduino_event_id_t event = ARDUINO_EVENT_MAX
    )
      : id(id), cb(cb), fcb(fcb), scb(scb), event(event) {}
  };

  // define initial id's value
  network_event_handle_t _current_id{0};

  EventGroupHandle_t _arduino_event_group;
  QueueHandle_t _arduino_event_queue;
  TaskHandle_t _arduino_event_task_handle;

  // registered events callbacks container
  std::vector<NetworkEventCbList_t> _cbEventList;

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  // container access mutex
  std::mutex _mtx;
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  /**
   * @brief task function that picks events from an event queue and calls registered callbacks
   *
   */
  void _checkForEvent();
};
