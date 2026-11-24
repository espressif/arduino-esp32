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
 * @brief Matter rain sensor endpoint (detected / not detected).
 *
 * Read-only Boolean State. Fabric StateValue starts false (not detected). Call
 * @ref setRain after endpoint @c begin(); a value set before @c Matter.begin()
 * is cached and applied when the cluster is created.
 */
class MatterRainSensor : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter rain sensor endpoint.
   */
  MatterRainSensor();
  /**
   * @brief Destroy the rain sensor endpoint object.
   */
  ~MatterRainSensor();
  /**
   * @brief Initialize the Matter rain sensor endpoint.
   *
   * Fabric StateValue starts false (not detected). Call @ref setRain after this.
   *
   * @return true if successful, false otherwise.
   */
  bool begin();
  /**
   * @brief Stop processing Matter rain sensor events.
   */
  void end();

  /**
   * @brief Set the rain detection state. Call after the endpoint @c begin().
   *
   * Before @c Matter.begin() the value is cached and applied when the cluster is created.
   *
   * @param _rainState true = detected, false = not detected.
   * @return true if successful, false otherwise.
   */
  bool setRain(bool _rainState);
  /**
   * @brief Get the current rain detection state.
   * @return true if rain is detected, false if not detected.
   */
  bool getRain() {
    return rainState;
  }

  /**
   * @brief Assignment — set rain state (same as @ref setRain).
   * @param _rainState true = detected, false = not detected.
   */
  void operator=(bool _rainState) {
    setRain(_rainState);
  }
  /**
   * @brief Boolean conversion — current rain detection state.
   * @return true if detected.
   */
  operator bool() {
    return getRain();
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
  bool rainState = false;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
