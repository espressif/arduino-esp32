// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @brief Thermostat endpoint that binds to temperature and humidity sensors (Zigbee HA).
 *
 * Discovers sensors, receives temperature/humidity readings, retrieves sensor
 * settings, and configures reporting. Addressing overloads target all bound
 * devices, a group, a short address, or an IEEE address.
 */
class ZigbeeThermostat : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee thermostat endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeThermostat(uint8_t endpoint);
  ~ZigbeeThermostat() {}

  /**
   * @brief Set a callback for receiving temperature data.
   * @param callback Function called with temperature in degrees Celsius.
   */
  void onTempReceive(void (*callback)(float)) {
    _on_temp_receive = callback;
  }
  /**
   * @brief Set a callback for receiving temperature data with source information.
   * @param callback Function called with temperature (°C), source endpoint, and source address.
   */
  void onTempReceiveWithSource(void (*callback)(float, uint8_t, ezb_address_t)) {
    _on_temp_receive_with_source = callback;
  }
  /**
   * @brief Set a callback for receiving sensor configuration data.
   *
   * Use `onTempConfigReceive()` instead.
   *
   * @param callback Function called with min temperature, max temperature, and tolerance.
   */
  [[deprecated("Use onTempConfigReceive instead.")]]
  void onConfigReceive(void (*callback)(float, float, float)) {
    onTempConfigReceive(callback);
  }
  /**
   * @brief Set a callback for receiving temperature sensor configuration data.
   * @param callback Function called with min temperature, max temperature, and tolerance (°C).
   */
  void onTempConfigReceive(void (*callback)(float, float, float)) {
    _on_temp_config_receive = callback;
  }

  /**
   * @brief Request temperature data from all bound sensors.
   */
  void getTemperature();
  /**
   * @brief Request temperature data from a specific group.
   * @param group_addr Group address to send the request to.
   */
  void getTemperature(uint16_t group_addr);
  /**
   * @brief Request temperature data from a device by short address.
   * @param endpoint Target endpoint number.
   * @param short_addr Short address of the target device.
   */
  void getTemperature(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request temperature data from a device by IEEE address.
   * @param endpoint Target endpoint number.
   * @param ieee_addr IEEE address of the target device.
   */
  void getTemperature(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Request temperature sensor settings from all bound sensors.
   *
   * Use `getTemperatureSettings()` instead.
   */
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings() {
    getTemperatureSettings();
  }
  /**
   * @brief Request temperature sensor settings from a group.
   *
   * Use `getTemperatureSettings()` instead.
   *
   * @param group_addr Group address to send the request to.
   */
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings(uint16_t group_addr) {
    getTemperatureSettings(group_addr);
  }
  /**
   * @brief Request temperature sensor settings by short address.
   *
   * Use `getTemperatureSettings()` instead.
   *
   * @param endpoint Target endpoint number.
   * @param short_addr Short address of the target device.
   */
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings(uint8_t endpoint, uint16_t short_addr) {
    getTemperatureSettings(endpoint, short_addr);
  }
  /**
   * @brief Request temperature sensor settings by IEEE address.
   *
   * Use `getTemperatureSettings()` instead.
   *
   * @param endpoint Target endpoint number.
   * @param ieee_addr IEEE address of the target device.
   */
  [[deprecated("Use getTemperatureSettings instead.")]]
  void getSensorSettings(uint8_t endpoint, const uint8_t *ieee_addr) {
    getTemperatureSettings(endpoint, ieee_addr);
  }

  /**
   * @brief Request temperature sensor settings from all bound sensors.
   */
  void getTemperatureSettings();
  /**
   * @brief Request temperature sensor settings from a specific group.
   * @param group_addr Group address to send the request to.
   */
  void getTemperatureSettings(uint16_t group_addr);
  /**
   * @brief Request temperature sensor settings by short address.
   * @param endpoint Target endpoint number.
   * @param short_addr Short address of the target device.
   */
  void getTemperatureSettings(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request temperature sensor settings by IEEE address.
   * @param endpoint Target endpoint number.
   * @param ieee_addr IEEE address of the target device.
   */
  void getTemperatureSettings(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Configure temperature reporting for all bound sensors.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in temperature to trigger a report.
   */
  void setTemperatureReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  /**
   * @brief Configure temperature reporting for a specific group.
   * @param group_addr Group address to configure.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in temperature to trigger a report.
   */
  void setTemperatureReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  /**
   * @brief Configure temperature reporting by short address.
   * @param endpoint Target endpoint number.
   * @param short_addr Short address of the target device.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in temperature to trigger a report.
   */
  void setTemperatureReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  /**
   * @brief Configure temperature reporting by IEEE address.
   * @param endpoint Target endpoint number.
   * @param ieee_addr IEEE address of the target device.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in temperature to trigger a report.
   */
  void setTemperatureReporting(uint8_t endpoint, const uint8_t *ieee_addr, uint16_t min_interval, uint16_t max_interval, float delta);

  /**
   * @brief Set a callback for receiving humidity data.
   * @param callback Function called with humidity in percent.
   */
  void onHumidityReceive(void (*callback)(float)) {
    _on_humidity_receive = callback;
  }
  /**
   * @brief Set a callback for receiving humidity data with source information.
   * @param callback Function called with humidity (%), source endpoint, and source address.
   */
  void onHumidityReceiveWithSource(void (*callback)(float, uint8_t, ezb_address_t)) {
    _on_humidity_receive_with_source = callback;
  }
  /**
   * @brief Set a callback for receiving humidity sensor configuration data.
   * @param callback Function called with min humidity, max humidity, and tolerance (%).
   */
  void onHumidityConfigReceive(void (*callback)(float, float, float)) {
    _on_humidity_config_receive = callback;
  }

  /**
   * @brief Request humidity data from all bound sensors.
   */
  void getHumidity();
  /**
   * @brief Request humidity data from a specific group.
   * @param group_addr Group address to send the request to.
   */
  void getHumidity(uint16_t group_addr);
  /**
   * @brief Request humidity data from a device by short address.
   * @param endpoint Target endpoint number.
   * @param short_addr Short address of the target device.
   */
  void getHumidity(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request humidity data from a device by IEEE address.
   * @param endpoint Target endpoint number.
   * @param ieee_addr IEEE address of the target device.
   */
  void getHumidity(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Request humidity sensor settings from all bound sensors.
   */
  void getHumiditySettings();
  /**
   * @brief Request humidity sensor settings from a specific group.
   * @param group_addr Group address to send the request to.
   */
  void getHumiditySettings(uint16_t group_addr);
  /**
   * @brief Request humidity sensor settings by short address.
   * @param endpoint Target endpoint number.
   * @param short_addr Short address of the target device.
   */
  void getHumiditySettings(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request humidity sensor settings by IEEE address.
   * @param endpoint Target endpoint number.
   * @param ieee_addr IEEE address of the target device.
   */
  void getHumiditySettings(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Configure humidity reporting for all bound sensors.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in humidity to trigger a report.
   */
  void setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  /**
   * @brief Configure humidity reporting for a specific group.
   * @param group_addr Group address to configure.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in humidity to trigger a report.
   */
  void setHumidityReporting(uint16_t group_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  /**
   * @brief Configure humidity reporting by short address.
   * @param endpoint Target endpoint number.
   * @param short_addr Short address of the target device.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in humidity to trigger a report.
   */
  void setHumidityReporting(uint8_t endpoint, uint16_t short_addr, uint16_t min_interval, uint16_t max_interval, float delta);
  /**
   * @brief Configure humidity reporting by IEEE address.
   * @param endpoint Target endpoint number.
   * @param ieee_addr IEEE address of the target device.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change in humidity to trigger a report.
   */
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

  // Read / configure-report senders shared by all addressing overloads.
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
