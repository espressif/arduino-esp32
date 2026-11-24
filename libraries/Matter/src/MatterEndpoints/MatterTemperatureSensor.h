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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>

/**
 * @brief Matter temperature sensor endpoint (read-only reporting in Celsius).
 *
 * Stores temperature with 1/100th degree Celsius precision. Compatible with
 * Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterTemperatureSensor : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter temperature sensor endpoint.
   */
  MatterTemperatureSensor();
  /**
   * @brief Destroy the temperature sensor endpoint object.
   */
  ~MatterTemperatureSensor();
  /**
   * @brief Initialize the Matter temperature sensor endpoint.
   * @param temperature Initial temperature in Celsius (default 0.00).
   *                    Matter MeasuredValue is int16 hundredths (−327.68…327.67 °C).
   * @return true if successful, false if the value is out of range or creation fails.
   * @note The implementation stores temperature with 1/100th degree Celsius precision internally.
   */
  bool begin(double temperature = 0.00);
  /**
   * @brief Stop processing Matter temperature sensor events.
   */
  void end();

  /**
   * @brief Set the reported temperature value.
   * @param temperature Temperature in Celsius.
   * @return true if successful, false otherwise (including int16 overflow outside −327.68…327.67 °C).
   * @note Temperature is stored with 1/100th degree Celsius precision.
   */
  bool setTemperature(double temperature);
  /**
   * @brief Get the current reported temperature value.
   * @return Temperature in Celsius with 1/100th degree precision.
   */
  double getTemperature() {
    return (double)rawTemperature / 100.0;
  }

  /**
   * @brief Assignment — set the reported temperature.
   * @param temperature Temperature in Celsius.
   */
  void operator=(double temperature) {
    setTemperature(temperature);
  }
  /**
   * @brief Conversion — current reported temperature.
   * @return Temperature in Celsius.
   */
  operator double() {
    return (double)getTemperature();
  }

  /**
   * @brief Called by the Matter internal event processor when an attribute changes.
   *
   * Can be overridden by the application if necessary.
   *
   * @param endpoint_id Endpoint ID where the attribute changed.
   * @param cluster_id Cluster ID of the changed attribute.
   * @param attribute_id Attribute ID that changed.
   * @param val Pointer to the new attribute value.
   * @return true if the change was handled successfully.
   */
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  // implementation keeps temperature in 1/100th Celsius x 100 (int16_t) normalized value
  int16_t rawTemperature = 0;
  // internal function to set the raw temperature value (Matter Cluster)
  bool setRawTemperature(int16_t _rawTemperature);
  bool begin(int16_t _rawTemperature);
  void onStackStarted() override;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
