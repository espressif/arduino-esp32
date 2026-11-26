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
 * @brief Matter humidity sensor endpoint (read-only reporting, 0–100%).
 *
 * Stores humidity with 1/100th percent precision. Compatible with Home
 * Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterHumiditySensor : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter humidity sensor endpoint.
   */
  MatterHumiditySensor();
  /**
   * @brief Destroy the humidity sensor endpoint object.
   */
  ~MatterHumiditySensor();
  /**
   * @brief Initialize the Matter humidity sensor endpoint.
   * @param humidityPercent Initial humidity percentage (0.0–100.0, default 0.00).
   * @return true if successful, false otherwise.
   * @note The implementation stores humidity with 1/100th percent precision internally.
   */
  bool begin(double humidityPercent = 0.00) {
    if (humidityPercent < 0.0 || humidityPercent > 100.0) {
      log_e("Humidity Sensor Percentage value out of range [0..100].");
      return false;
    }
    return begin(static_cast<uint16_t>(humidityPercent * 100.0f));
  }
  /**
   * @brief Stop processing Matter humidity sensor events.
   */
  void end();

  /**
   * @brief Set the reported humidity percentage.
   * @param humidityPercent Humidity percentage (0.0–100.0).
   * @return true if successful, false otherwise (values outside 0.0–100.0 are rejected).
   * @note Humidity is stored with 1/100th percent precision.
   */
  bool setHumidity(double humidityPercent) {
    if (humidityPercent < 0.0 || humidityPercent > 100.0) {
      log_e("Humidity Sensor Percentage value out of range [0..100].");
      return false;
    }
    return setRawHumidity(static_cast<uint16_t>(humidityPercent * 100.0f));
  }
  /**
   * @brief Get the current reported humidity percentage.
   * @return Humidity percentage with 1/100th percent precision.
   */
  double getHumidity() {
    return (double)rawHumidity / 100.0;
  }
  /**
   * @brief Assignment — set the reported humidity percentage.
   * @param humidityPercent Humidity percentage (0.0–100.0).
   */
  void operator=(double humidityPercent) {
    setHumidity(humidityPercent);
  }
  /**
   * @brief Conversion — current reported humidity percentage.
   * @return Humidity percentage.
   */
  operator double() {
    return (double)getHumidity();
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
  // implementation keeps humidity relative percentage with 1/100th of a percent precision
  uint16_t rawHumidity = 0;
  // internal function to set the raw humidity value (Matter Cluster)
  bool begin(uint16_t _rawHumidity);
  bool setRawHumidity(uint16_t _rawHumidity);
  void onStackStarted() override;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
