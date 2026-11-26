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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>

/**
 * @brief Matter soil sensor endpoint (device type 0x0045, Soil Measurement cluster 0x0430).
 *
 * Soil Measurement is a code-driven CHIP cluster. Use @ref setSoilMoisture rather
 * than getAttributeVal() / updateAttributeVal(). Reports whole-percent moisture
 * (0–100); unlike @ref MatterHumiditySensor there is no sub-percent precision.
 */
class MatterSoilSensor : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter soil sensor endpoint.
   */
  MatterSoilSensor();
  /**
   * @brief Destroy the soil sensor endpoint object.
   */
  ~MatterSoilSensor();
  /**
   * @brief Create the soil sensor endpoint. Call before @c Matter.begin().
   *
   * The live SoilMeasurementCluster is registered when the Matter stack starts.
   * The measured attribute stays null until the first successful @ref setSoilMoisture.
   *
   * @return true if successful, false otherwise.
   */
  bool begin();
  /**
   * @brief Create the endpoint and seed an initial whole-percent moisture reading.
   *
   * @param initialSoilMoisturePercent Whole number in [0..100]. Cached until the
   *        live cluster is available if called before @c Matter.begin().
   * @return true if successful, false otherwise.
   */
  bool begin(uint8_t initialSoilMoisturePercent);
  /**
   * @brief Stop processing Matter soil sensor events.
   */
  void end();

  /**
   * @brief Set the reported soil moisture (0–100%) after endpoint @c begin().
   *
   * Before @c Matter.begin() the value is cached and applied when the live cluster
   * is registered; afterward it updates the live cluster directly.
   *
   * @param soilMoisturePercent Whole number in [0..100].
   * @return true if successful, false otherwise.
   */
  bool setSoilMoisture(uint8_t soilMoisturePercent);
  /**
   * @brief Get the cached soil moisture (0–100%).
   *
   * Returns 0 until the first @ref setSoilMoisture. The cluster attribute stays
   * null until a publish succeeds.
   *
   * @return Cached soil moisture percent.
   */
  uint8_t getSoilMoisture() {
    return soilMoisture;
  }
  /**
   * @brief Assignment — set the reported soil moisture.
   * @param soilMoisturePercent Whole number in [0..100].
   */
  void operator=(uint8_t soilMoisturePercent) {
    setSoilMoisture(soilMoisturePercent);
  }
  /**
   * @brief Conversion — cached soil moisture percent.
   * @return Moisture in [0..100].
   */
  operator uint8_t() {
    return getSoilMoisture();
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
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) override;

protected:
  void onStackStarted() override;

  bool createSoilSensorEndpoint();
  void destroySoilSensorEndpoint(endpoint_t *endpoint);

  bool started = false;
  bool hasPendingMoistureReport = false;
  // true only after SetSoilMoistureMeasuredValue() succeeds for the cached percent
  bool moisturePublished = false;
  uint8_t soilMoisture = 0;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
