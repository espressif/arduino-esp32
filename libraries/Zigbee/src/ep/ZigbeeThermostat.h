// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* Class of Zigbee Thermostat endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

// NOTE(zb-v2): v1 defined a local ZB_DEFAULT_THERMOSTAT_CONFIG() macro to dodge a narrowing-conversion
// warning in the SDK. v2.x ships EZB_ZHA_THERMOSTAT_CONFIG() (see ezbee/zha.h) with the same purpose, so
// the local macro is dropped and the SDK macro is used directly in the constructor.

class ZigbeeThermostat : public ZigbeeEP {
public:
  ZigbeeThermostat(uint8_t endpoint);
  ~ZigbeeThermostat() {}

  // Temperature measuring methods
  void onTempReceive(void (*callback)(float)) {
    _on_temp_receive = callback;
  }
  void onTempReceiveWithSource(void (*callback)(float, uint8_t, ezb_address_t)) {
    _on_temp_receive_with_source = callback;
  }
  // For backward compatibility: keep onConfigReceive as an alias to onTempConfigReceive (deprecated).
  [[deprecated("Use onTempConfigReceive instead.")]]
  void onConfigReceive(void (*callback)(float, float, float)) {
    onTempConfigReceive(callback);
  }
  void onTempConfigReceive(void (*callback)(float, float, float)) {
    _on_temp_config_receive = callback;
  }

  void getTemperature();
  void getTemperature(uint16_t group_addr);
  void getTemperature(uint8_t endpoint, uint16_t short_addr);
  void getTemperature(uint8_t endpoint, const uint8_t *ieee_addr);

  // For backward compatibility: keep getSensorSettings as an alias to getTemperatureSettings (deprecated).
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings() {
    getTemperatureSettings();
  }
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings(uint16_t group_addr) {
    getTemperatureSettings(group_addr);
  }
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings(uint8_t endpoint, uint16_t short_addr) {
    getTemperatureSettings(endpoint, short_addr);
  }
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings(uint8_t endpoint, const uint8_t *ieee_addr) {
    getTemperatureSettings(endpoint, ieee_addr);
  }

  void getTemperatureSettings();
  void getTemperatureSettings(uint16_t group_addr);
  void getTemperatureSettings(uint8_t endpoint, uint16_t short_addr);
  void getTemperatureSettings(uint8_t endpoint, const uint8_t *ieee_addr);

  void setTemperatureReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  void setTemperatureReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  void setTemperatureReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  void setTemperatureReporting(uint8_t endpoint, const uint8_t *ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta);

  // Humidity measuring methods
  void onHumidityReceive(void (*callback)(float)) {
    _on_humidity_receive = callback;
  }
  void onHumidityReceiveWithSource(void (*callback)(float, uint8_t, ezb_address_t)) {
    _on_humidity_receive_with_source = callback;
  }
  void onHumidityConfigReceive(void (*callback)(float, float, float)) {
    _on_humidity_config_receive = callback;
  }

  void getHumidity();
  void getHumidity(uint16_t group_addr);
  void getHumidity(uint8_t endpoint, uint16_t short_addr);
  void getHumidity(uint8_t endpoint, const uint8_t *ieee_addr);

  void getHumiditySettings();
  void getHumiditySettings(uint16_t group_addr);
  void getHumiditySettings(uint8_t endpoint, uint16_t short_addr);
  void getHumiditySettings(uint8_t endpoint, const uint8_t *ieee_addr);

  void setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  void setHumidityReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  void setHumidityReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  void setHumidityReporting(uint8_t endpoint, const uint8_t *ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta);

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeThermostat *_instance;
  zb_device_params_t *_device;

  void (*_on_temp_receive)(float);
  void (*_on_temp_receive_with_source)(float, uint8_t, ezb_address_t);
  void (*_on_temp_config_receive)(float, float, float);
  float _min_temp;
  float _max_temp;
  float _tolerance_temp;

  void (*_on_humidity_receive)(float);
  void (*_on_humidity_receive_with_source)(float, uint8_t, ezb_address_t);
  void (*_on_humidity_config_receive)(float, float, float);
  float _min_humidity;
  float _max_humidity;
  float _tolerance_humidity;

  // v2.x read/configure-report senders shared by all addressing overloads.
  bool sendReadAttributes(uint16_t cluster_id, uint16_t *attributes, uint8_t attr_number, ezb_address_t dst_addr, uint8_t dst_ep);
  void sendConfigReport(uint16_t cluster_id, ezb_zcl_config_report_record_t *records, uint16_t record_number, ezb_address_t dst_addr, uint8_t dst_ep);

  void findEndpoint(ezb_zdo_match_desc_req_t *cmd_req) override;
  void bindCb(const ezb_zdp_bind_req_result_t *result, void *user_ctx);
  void findCb(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);
  static void bindCbWrapper(const ezb_zdp_bind_req_result_t *result, void *user_ctx);
  static void findCbWrapper(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);

  void zbAttributeRead(uint16_t cluster_id, const ezb_zcl_attribute_t *attribute, uint8_t src_endpoint, ezb_address_t src_address) override;
};

#endif  // CONFIG_ZB_ENABLED
