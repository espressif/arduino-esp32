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
 * @brief Matter water freeze detector endpoint (detected / not detected).
 *
 * Read-only Boolean State. Fabric StateValue starts false (not detected). Call
 * @ref setFreeze after endpoint @c begin(); a value set before @c Matter.begin()
 * is cached and applied when the cluster is created.
 */
class MatterWaterFreezeDetector : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter water freeze detector endpoint.
   */
  MatterWaterFreezeDetector();
  /**
   * @brief Destroy the water freeze detector endpoint object.
   */
  ~MatterWaterFreezeDetector();
  /**
   * @brief Initialize the Matter water freeze detector endpoint.
   *
   * Fabric StateValue starts false (not detected). Call @ref setFreeze after this.
   *
   * @return true if successful, false otherwise.
   */
  bool begin();
  /**
   * @brief Stop processing Matter water freeze detector events.
   */
  void end();

  /**
   * @brief Set the water freeze detection state. Call after the endpoint @c begin().
   *
   * Before @c Matter.begin() the value is cached and applied when the cluster is created.
   *
   * @param _freezeState true = detected, false = not detected.
   * @return true if successful, false otherwise.
   */
  bool setFreeze(bool _freezeState);
  /**
   * @brief Get the current water freeze detection state.
   * @return true if water freeze is detected, false if not detected.
   */
  bool getFreeze() {
    return freezeState;
  }

  /**
   * @brief Assignment — set freeze state (same as @ref setFreeze).
   * @param _freezeState true = detected, false = not detected.
   */
  void operator=(bool _freezeState) {
    setFreeze(_freezeState);
  }
  /**
   * @brief Boolean conversion — current freeze detection state.
   * @return true if detected.
   */
  operator bool() {
    return getFreeze();
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
  bool freezeState = false;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
