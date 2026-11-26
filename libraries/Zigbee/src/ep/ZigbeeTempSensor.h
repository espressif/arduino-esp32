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

/* Class of Zigbee Temperature + Humidity sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/temperature_measurement_desc.h"

/**
 * @brief Temperature and humidity sensor endpoint (Zigbee HA environmental monitoring).
 *
 * Supports temperature measurement with optional humidity, configurable reporting
 * intervals, and min/max/tolerance settings.
 */
class ZigbeeTempSensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee temperature sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeTempSensor(uint8_t endpoint);
  ~ZigbeeTempSensor() {}

  /**
   * @brief Set the temperature value in 0.01 °C resolution.
   * @param value Temperature in degrees Celsius.
   * @return true if successful.
   */
  bool setTemperature(float value);

  /**
   * @brief Set the default (initial) temperature in 0.01 °C resolution.
   *
   * Used as the initial measured value in factory-reset mode before the sensor
   * provides actual readings. Must be called before adding the endpoint to Zigbee.
   * Only effective before commissioning/joining a network.
   *
   * @param defaultValue Default temperature in degrees Celsius.
   * @return true if successful.
   */
  bool setDefaultValue(float defaultValue);

  /**
   * @brief Set the minimum and maximum temperature values.
   * @param min Minimum temperature in degrees Celsius.
   * @param max Maximum temperature in degrees Celsius.
   * @return true if successful.
   */
  bool setMinMaxValue(float min, float max);

  /**
   * @brief Set the tolerance value for temperature reporting.
   * @param tolerance Tolerance in degrees Celsius.
   * @return true if successful.
   */
  bool setTolerance(float tolerance);

  /**
   * @brief Set the reporting interval for temperature measurements.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum temperature change to trigger a report, in degrees Celsius.
   * @return true if successful.
   */
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  /**
   * @brief Manually report the current temperature value.
   * @return true if successful.
   */
  bool reportTemperature();

  /**
   * @brief Add humidity measurement capability to the temperature sensor.
   * @param min Minimum humidity in percentage.
   * @param max Maximum humidity in percentage.
   * @param tolerance Tolerance in percentage.
   * @param defaultValue Default humidity in percentage.
   */
  void addHumiditySensor(float min = 0.0, float max = 100.0, float tolerance = 0.1, float defaultValue = 0.0);

  /**
   * @brief Set the humidity value in 0.01% resolution.
   * @param value Humidity in percentage (0-100).
   * @return true if successful.
   */
  bool setHumidity(float value);

  /**
   * @brief Set the reporting interval for humidity measurements.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum humidity change to trigger a report, in percentage.
   * @return true if successful.
   */
  bool setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  /**
   * @brief Manually report the current humidity value.
   * @return true if successful.
   */
  bool reportHumidity();

  /**
   * @brief Report temperature and humidity (humidity only if that sensor was added).
   * @return true if successful.
   */
  bool report();

private:
  ezb_zcl_temperature_measurement_cluster_config_t _temp_meas_cfg;
  uint16_t _tolerance;
  bool _humidity_sensor;
};

#endif  // CONFIG_ZB_ENABLED
