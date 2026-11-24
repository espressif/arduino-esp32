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
 * @brief Matter pressure sensor endpoint (read-only reporting in hectopascals).
 *
 * Typical atmospheric pressure at sea level is around 1013.25 hPa.
 * Compatible with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterPressureSensor : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter pressure sensor endpoint.
   */
  MatterPressureSensor();
  /**
   * @brief Destroy the pressure sensor endpoint object.
   */
  ~MatterPressureSensor();
  /**
   * @brief Initialize the Matter pressure sensor endpoint.
   * @param pressure Initial pressure in hectopascals (hPa, default 0.00).
   * @return true if successful, false otherwise.
   * @note Typical atmospheric pressure at sea level is around 1013.25 hPa (often 950–1050 hPa).
   */
  bool begin(double pressure = 0.00) {
    return begin(static_cast<int16_t>(pressure));
  }
  /**
   * @brief Stop processing Matter pressure sensor events.
   */
  void end();

  /**
   * @brief Set the reported pressure value.
   * @param pressure Pressure in hectopascals (hPa).
   * @return true if successful, false otherwise.
   */
  bool setPressure(double pressure) {
    int16_t rawValue = static_cast<int16_t>(pressure);
    return setRawPressure(rawValue);
  }
  /**
   * @brief Get the current reported pressure value.
   * @return Pressure in hectopascals (hPa).
   */
  double getPressure() {
    return (double)rawPressure;
  }

  /**
   * @brief Assignment — set the reported pressure.
   * @param pressure Pressure in hectopascals (hPa).
   */
  void operator=(double pressure) {
    setPressure(pressure);
  }
  /**
   * @brief Conversion — current reported pressure.
   * @return Pressure in hectopascals (hPa).
   */
  operator double() {
    return (double)getPressure();
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
  // implementation keeps pressure in hPa
  int16_t rawPressure = 0;
  // internal function to set the raw pressure value (Matter Cluster)
  bool setRawPressure(int16_t _rawPressure);
  bool begin(int16_t _rawPressure);
  void onStackStarted() override;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
