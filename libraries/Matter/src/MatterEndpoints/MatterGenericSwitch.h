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
 * @brief Matter generic switch endpoint — momentary smart button with optional gesture features.
 *
 * Implements the Matter Switch cluster and reports press gestures to the controller.
 * Sibling switches of the same device type should use @ref MatterEndPoint::setTagList
 * so a controller can tell them apart. Compatible with Home Assistant, Apple Home,
 * Amazon Alexa, and Google Home.
 *
 * @note FEATURE_LONG_PRESS and FEATURE_MULTI_PRESS each require FEATURE_RELEASE.
 */
class MatterGenericSwitch : public MatterEndPoint {
public:
  static constexpr uint32_t FEATURE_MOMENTARY = 0x02;    ///< FeatureMap bit 0x02: InitialPress.
  static constexpr uint32_t FEATURE_RELEASE = 0x04;      ///< FeatureMap bit 0x04: ShortRelease.
  static constexpr uint32_t FEATURE_LONG_PRESS = 0x08;   ///< FeatureMap bit 0x08: LongPress, LongRelease. Requires FEATURE_RELEASE.
  static constexpr uint32_t FEATURE_MULTI_PRESS = 0x10;  ///< FeatureMap bit 0x10: MultiPressOngoing, MultiPressComplete. Requires FEATURE_RELEASE.

  /**
   * @brief Short click: InitialPress + ShortRelease (default).
   */
  static constexpr uint32_t FEATURE_SIMPLE = FEATURE_MOMENTARY | FEATURE_RELEASE;
  /**
   * @brief All momentary gesture features supported by this class.
   */
  static constexpr uint32_t FEATURE_ALL = FEATURE_SIMPLE | FEATURE_LONG_PRESS | FEATURE_MULTI_PRESS;

  /**
   * @brief Create a new Matter generic switch endpoint.
   */
  MatterGenericSwitch();
  /**
   * @brief Destroy the generic switch endpoint object.
   */
  ~MatterGenericSwitch();

  /**
   * @brief Initialize the Matter generic switch endpoint.
   * @param featureFlags Switch cluster features at endpoint creation (default FEATURE_SIMPLE).
   * @param multiPressMax Maximum press count for multi-press (2–255). Used only when FEATURE_MULTI_PRESS is set.
   * @return true on success, false otherwise.
   */
  virtual bool begin(uint32_t featureFlags = FEATURE_SIMPLE, uint8_t multiPressMax = 5);
  /**
   * @brief Stop processing Matter switch events.
   */
  void end();

  /**
   * @brief Whether the given feature bit is enabled.
   * @param feature Feature bit (e.g. FEATURE_LONG_PRESS).
   * @return true if enabled.
   */
  bool hasFeature(uint32_t feature) const;

  /**
   * @brief Send InitialPress (button pressed down). Call from the button driver.
   */
  void press();
  /**
   * @brief Send ShortRelease (button released after a short press). Call from the button driver.
   */
  void release();
  /**
   * @brief Send LongPress (button held past the long-press threshold). Call from the button driver.
   */
  void longPress();
  /**
   * @brief Send LongRelease (button released after a long press). Call from the button driver.
   */
  void longRelease();
  /**
   * @brief Send MultiPressOngoing when an additional press starts within the multi-press window.
   * @param count Current press count (e.g. 2 for the second press).
   */
  void multiPressOngoing(uint8_t count);
  /**
   * @brief Send MultiPressComplete when the multi-press window expires after the last release.
   * @param count Total press count.
   */
  void multiPressComplete(uint8_t count);

  /**
   * @brief Convenience helper: InitialPress then ShortRelease when FEATURE_RELEASE is enabled.
   *
   * Both events run in one Matter-task lambda so ShortRelease cannot be processed before InitialPress.
   */
  void click();

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
  void onStackStarted() override;

  bool started = false;
  uint32_t featureFlags = FEATURE_SIMPLE;
  uint8_t multiPressMax = 5;
  static constexpr uint8_t pressPosition = 1;
  static constexpr uint8_t idlePosition = 0;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
