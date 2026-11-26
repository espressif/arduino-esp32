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
 * @brief Matter contact sensor endpoint (open/closed, e.g. doors and windows).
 *
 * Read-only Boolean State. Fabric StateValue starts false (open). Call
 * @ref setContact after endpoint @c begin(); a value set before @c Matter.begin()
 * is cached and applied when the cluster is created.
 */
class MatterContactSensor : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter contact sensor endpoint.
   */
  MatterContactSensor();
  /**
   * @brief Destroy the contact sensor endpoint object.
   */
  ~MatterContactSensor();
  /**
   * @brief Initialize the Matter contact sensor endpoint.
   *
   * Fabric StateValue starts false (open). Call @ref setContact after this.
   *
   * @return true if successful, false otherwise.
   */
  bool begin();
  /**
   * @brief Stop processing Matter contact sensor events.
   */
  void end();

  /**
   * @brief Set the contact state. Call after the endpoint @c begin().
   *
   * Before @c Matter.begin() the value is cached and applied when the cluster is created.
   *
   * @param _contactState Contact state (true = closed, false = open).
   * @return true if successful, false otherwise.
   */
  bool setContact(bool _contactState);
  /**
   * @brief Get the current contact state.
   * @return true if closed, false if open.
   */
  bool getContact() {
    return contactState;
  }

  /**
   * @brief Assignment — set the contact state (same as @ref setContact).
   * @param _contactState true = closed, false = open.
   */
  void operator=(bool _contactState) {
    setContact(_contactState);
  }
  /**
   * @brief Boolean conversion — current contact state.
   * @return true if closed, false if open.
   */
  operator bool() {
    return getContact();
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
  bool contactState = false;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
