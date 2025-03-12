/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "NetworkEvents.h"
#include "esp32-hal.h"

ESP_EVENT_DEFINE_BASE(ARDUINO_EVENTS);

NetworkEvents::~NetworkEvents() {
  if (_arduino_event_group != NULL) {
    vEventGroupDelete(_arduino_event_group);
    _arduino_event_group = NULL;
  }
  // unregister event bus handler
  if (_evt_handler){
    esp_event_handler_instance_unregister(ARDUINO_EVENTS, ESP_EVENT_ANY_ID, _evt_handler);
    _evt_handler = nullptr;
  }
}

bool NetworkEvents::initNetworkEvents() {
  if (!_arduino_event_group) {
    _arduino_event_group = xEventGroupCreate();
    if (!_arduino_event_group) {
      log_e("Network Event Group Create Failed!");
      return false;
    }
    xEventGroupSetBits(_arduino_event_group, _initial_bits);
  }

  // create default ESP event loop
  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    log_e("esp_event_loop_create_default failed!");
    return false;
  }

  // subscribe to default ESP event bus
  if (!_evt_handler){
    ESP_ERROR_CHECK(
      esp_event_handler_instance_register(
        ARDUINO_EVENTS, ESP_EVENT_ANY_ID,
        [](void* self, esp_event_base_t base, int32_t id, void* data) { static_cast<NetworkEvents*>(self)->_evt_picker(id, reinterpret_cast<arduino_event_info_t*>(data)); },
        this,
        &_evt_handler
      )
    );
  }
  return true;
}

bool NetworkEvents::postEvent(const arduino_event_t *data, TickType_t timeout) {
  if (!data) return false;
  esp_err_t err = esp_event_post(ARDUINO_EVENTS, static_cast<int32_t>(data->event_id), &data->event_info, sizeof(data->event_info), timeout);
  if (err == ESP_OK)
    return true;

  log_e("Arduino Event Send Failed!");
  return false;
}

bool NetworkEvents::postEvent(arduino_event_id_t event, const arduino_event_info_t *info, TickType_t timeout){
  if (info)
    return esp_event_post(ARDUINO_EVENTS, static_cast<int32_t>(event), info, sizeof(arduino_event_info_t), timeout) == pdTRUE;
  else
    return esp_event_post(ARDUINO_EVENTS, static_cast<int32_t>(event), NULL, 0, timeout) == pdTRUE;
}


void NetworkEvents::_evt_picker(int32_t id, arduino_event_info_t *info){
#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  // iterate over registered callbacks
  for (auto &i : _cbEventList) {
    if (i.event == ARDUINO_EVENT_ANY || i.event == static_cast<arduino_event_id_t>(id)){

      std::visit([id, info](auto&& arg){
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, NetworkEventReceiver>)
          arg(static_cast<arduino_event_id_t>(id), info);
        else if constexpr (std::is_same_v<T, NetworkEventCb>)
          arg(static_cast<arduino_event_id_t>(id));
        else if constexpr (std::is_same_v<T, NetworkEventFuncCb>)
          if (info)
            arg(static_cast<arduino_event_id_t>(id), *info);
          else
            arg(static_cast<arduino_event_id_t>(id), {});
        else if constexpr (std::is_same_v<T, NetworkEventSysCb>){
          // system event callback needs a ptr to struct
          arduino_event_t event{static_cast<arduino_event_id_t>(id), {}};
          if (info)
            memcpy(&event.event_info, info, sizeof(arduino_event_info_t));
          arg(&event);
        }
      }, i.cb_v);
    }
  }
}

template<typename T, typename... U> static size_t getStdFunctionAddress(std::function<T(U...)> f) {
  typedef T(fnType)(U...);
  fnType **fnPointer = f.template target<fnType *>();
  if (fnPointer != nullptr) {
    return (size_t)*fnPointer;
  }
  return (size_t)fnPointer;
}

network_event_handle_t NetworkEvents::onEvent(NetworkEventCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return 0;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.emplace_back(++_current_id, cbEvent, event);
  return _cbEventList.back().id;
}

network_event_handle_t NetworkEvents::onEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return 0;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.emplace_back(++_current_id, cbEvent, event);
  return _cbEventList.back().id;
}

network_event_handle_t NetworkEvents::onEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return 0;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.emplace_back(++_current_id, cbEvent, event);
  return _cbEventList.back().id;
}

network_event_handle_t NetworkEvents::onSysEvent(NetworkEventCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return 0;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.emplace(_cbEventList.begin(), ++_current_id, cbEvent, event);
  return _cbEventList.front().id;
}

network_event_handle_t NetworkEvents::onSysEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return 0;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.emplace(_cbEventList.begin(), ++_current_id, cbEvent, event);
  return _cbEventList.front().id;
}

network_event_handle_t NetworkEvents::onSysEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return 0;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.emplace(_cbEventList.begin(), ++_current_id, cbEvent, event);
  return _cbEventList.front().id;
}

network_event_handle_t NetworkEvents::onSysEvent(NetworkEventReceiver cbEvent, arduino_event_id_t event){
  if (!cbEvent) {
    return 0;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.emplace(_cbEventList.begin(), ++_current_id, cbEvent, event);
  return _cbEventList.front().id;
}

void NetworkEvents::removeEvent(NetworkEventCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.erase(
    std::remove_if(
      _cbEventList.begin(), _cbEventList.end(),
      [cbEvent, event](const NetworkEventCbList_t &e) {
        return e.event == event && std::visit([cbEvent](auto&& arg) -> bool {
          if constexpr (std::is_same_v<NetworkEventCb, std::decay_t<decltype(arg)>>)
            return cbEvent == arg;
          else return false;
        }, e.cb_v);
      }
    ),
    _cbEventList.end()
  );
}

void NetworkEvents::removeEvent(NetworkEventFuncCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1
  _cbEventList.erase(
    std::remove_if(
      _cbEventList.begin(), _cbEventList.end(),
      [cbEvent, event](const NetworkEventCbList_t &e) {
        return e.event == event && std::visit([cbEvent](auto&& arg) -> bool {
          if constexpr (std::is_same_v<NetworkEventFuncCb, std::decay_t<decltype(arg)>>)
            return getStdFunctionAddress(cbEvent) == getStdFunctionAddress(arg);
          else return false;
        }, e.cb_v);
      }
    ),
    _cbEventList.end()
  );
}

void NetworkEvents::removeEvent(NetworkEventSysCb cbEvent, arduino_event_id_t event) {
  if (!cbEvent) {
    return;
  }

#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.erase(
    std::remove_if(
      _cbEventList.begin(), _cbEventList.end(),
      [cbEvent, event](const NetworkEventCbList_t &e) {
        return e.event == event && std::visit([cbEvent](auto&& arg) -> bool {
          if constexpr (std::is_same_v<NetworkEventSysCb, std::decay_t<decltype(arg)>>)
            return cbEvent == arg;
          else return false;
        }, e.cb_v);
      }
    ),
    _cbEventList.end()
  );
}

void NetworkEvents::removeEvent(network_event_handle_t id) {
#if defined NETWORK_EVENTS_MUTEX && SOC_CPU_CORES_NUM > 1
  std::lock_guard<std::mutex> lock(_mtx);
#endif  // defined NETWORK_EVENTS_MUTEX &&  SOC_CPU_CORES_NUM > 1

  _cbEventList.erase(
    std::remove_if(
      _cbEventList.begin(), _cbEventList.end(),
      [id](const NetworkEventCbList_t &e) {
        return e.id == id;
      }
    ),
    _cbEventList.end()
  );
}

int NetworkEvents::setStatusBits(int bits) {
  if (!_arduino_event_group) {
    _initial_bits |= bits;
    return _initial_bits;
  }
  return xEventGroupSetBits(_arduino_event_group, bits);
}

int NetworkEvents::clearStatusBits(int bits) {
  if (!_arduino_event_group) {
    _initial_bits &= ~bits;
    return _initial_bits;
  }
  return xEventGroupClearBits(_arduino_event_group, bits);
}

int NetworkEvents::getStatusBits() const {
  if (!_arduino_event_group) {
    return _initial_bits;
  }
  return xEventGroupGetBits(_arduino_event_group);
}

int NetworkEvents::waitStatusBits(int bits, uint32_t timeout_ms) {
  if (!_arduino_event_group) {
    return 0;
  }
  return xEventGroupWaitBits(
           _arduino_event_group,  // The event group being tested.
           bits,                  // The bits within the event group to wait for.
           pdFALSE,               // bits should be cleared before returning.
           pdTRUE,                // Don't wait for all bits, any bit will do.
           timeout_ms / portTICK_PERIOD_MS
         )
         & bits;  // Wait a maximum of timeout_ms for any bit to be set.
}

/**
 * @brief Convert arduino_event_id_t to a C string.
 * @param [in] id The event id to be converted.
 * @return A string representation of the event id.
 */
const char *NetworkEvents::eventName(arduino_event_id_t id) {
  switch (id) {
    case ARDUINO_EVENT_ETH_START:        return "ETH_START";
    case ARDUINO_EVENT_ETH_STOP:         return "ETH_STOP";
    case ARDUINO_EVENT_ETH_CONNECTED:    return "ETH_CONNECTED";
    case ARDUINO_EVENT_ETH_DISCONNECTED: return "ETH_DISCONNECTED";
    case ARDUINO_EVENT_ETH_GOT_IP:       return "ETH_GOT_IP";
    case ARDUINO_EVENT_ETH_LOST_IP:      return "ETH_LOST_IP";
    case ARDUINO_EVENT_ETH_GOT_IP6:      return "ETH_GOT_IP6";
    case ARDUINO_EVENT_PPP_START:        return "PPP_START";
    case ARDUINO_EVENT_PPP_STOP:         return "PPP_STOP";
    case ARDUINO_EVENT_PPP_CONNECTED:    return "PPP_CONNECTED";
    case ARDUINO_EVENT_PPP_DISCONNECTED: return "PPP_DISCONNECTED";
    case ARDUINO_EVENT_PPP_GOT_IP:       return "PPP_GOT_IP";
    case ARDUINO_EVENT_PPP_LOST_IP:      return "PPP_LOST_IP";
    case ARDUINO_EVENT_PPP_GOT_IP6:      return "PPP_GOT_IP6";
#if SOC_WIFI_SUPPORTED || CONFIG_ESP_WIFI_REMOTE_ENABLED
    case ARDUINO_EVENT_WIFI_OFF:                 return "WIFI_OFF";
    case ARDUINO_EVENT_WIFI_READY:               return "WIFI_READY";
    case ARDUINO_EVENT_WIFI_SCAN_DONE:           return "SCAN_DONE";
    case ARDUINO_EVENT_WIFI_STA_START:           return "STA_START";
    case ARDUINO_EVENT_WIFI_STA_STOP:            return "STA_STOP";
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:       return "STA_CONNECTED";
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:    return "STA_DISCONNECTED";
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE: return "STA_AUTHMODE_CHANGE";
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:          return "STA_GOT_IP";
    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:         return "STA_GOT_IP6";
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:         return "STA_LOST_IP";
    case ARDUINO_EVENT_WIFI_AP_START:            return "AP_START";
    case ARDUINO_EVENT_WIFI_AP_STOP:             return "AP_STOP";
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:     return "AP_STACONNECTED";
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:  return "AP_STADISCONNECTED";
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:    return "AP_STAIPASSIGNED";
    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:   return "AP_PROBEREQRECVED";
    case ARDUINO_EVENT_WIFI_AP_GOT_IP6:          return "AP_GOT_IP6";
    case ARDUINO_EVENT_WIFI_FTM_REPORT:          return "FTM_REPORT";
    case ARDUINO_EVENT_WPS_ER_SUCCESS:           return "WPS_ER_SUCCESS";
    case ARDUINO_EVENT_WPS_ER_FAILED:            return "WPS_ER_FAILED";
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:           return "WPS_ER_TIMEOUT";
    case ARDUINO_EVENT_WPS_ER_PIN:               return "WPS_ER_PIN";
    case ARDUINO_EVENT_WPS_ER_PBC_OVERLAP:       return "WPS_ER_PBC_OVERLAP";
    case ARDUINO_EVENT_SC_SCAN_DONE:             return "SC_SCAN_DONE";
    case ARDUINO_EVENT_SC_FOUND_CHANNEL:         return "SC_FOUND_CHANNEL";
    case ARDUINO_EVENT_SC_GOT_SSID_PSWD:         return "SC_GOT_SSID_PSWD";
    case ARDUINO_EVENT_SC_SEND_ACK_DONE:         return "SC_SEND_ACK_DONE";
    case ARDUINO_EVENT_PROV_INIT:                return "PROV_INIT";
    case ARDUINO_EVENT_PROV_DEINIT:              return "PROV_DEINIT";
    case ARDUINO_EVENT_PROV_START:               return "PROV_START";
    case ARDUINO_EVENT_PROV_END:                 return "PROV_END";
    case ARDUINO_EVENT_PROV_CRED_RECV:           return "PROV_CRED_RECV";
    case ARDUINO_EVENT_PROV_CRED_FAIL:           return "PROV_CRED_FAIL";
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:        return "PROV_CRED_SUCCESS";
#endif
    default: return "";
  }
}
