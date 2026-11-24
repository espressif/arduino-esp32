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
#include <app-common/zap-generated/cluster-objects.h>
#include <functional>

using namespace chip::app::Clusters::OccupancySensing;

/**
 * @brief Matter occupancy sensor endpoint (occupied/unoccupied, e.g. PIR / ultrasonic).
 *
 * Optional HoldTime (seconds) is off unless @ref setHoldTime / @ref setHoldTimeLimits
 * is called after the sensor @c begin() and before @c Matter.begin(). Compatible with
 * Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterOccupancySensor : public MatterEndPoint {
public:
  /**
   * @brief Occupancy sensor type advertised in the Occupancy Sensing cluster.
   */
  enum OccupancySensorType_t {
    OCCUPANCY_SENSOR_TYPE_PIR = (uint8_t)OccupancySensorTypeEnum::kPir,                         ///< Passive Infrared (PIR).
    OCCUPANCY_SENSOR_TYPE_ULTRASONIC = (uint8_t)OccupancySensorTypeEnum::kUltrasonic,           ///< Ultrasonic.
    OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC = (uint8_t)OccupancySensorTypeEnum::kPIRAndUltrasonic,  ///< Combined PIR and Ultrasonic.
    OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT = (uint8_t)OccupancySensorTypeEnum::kPhysicalContact ///< Physical contact.
  };

  /**
   * @brief Create a new Matter occupancy sensor endpoint.
   */
  MatterOccupancySensor();
  /**
   * @brief Destroy the occupancy sensor endpoint object.
   */
  ~MatterOccupancySensor();
  /**
   * @brief Initialize the Matter occupancy sensor endpoint.
   *
   * Sensor type is taken from Occupancy Sensing features at create time.
   * HoldTime is off unless @ref setHoldTime / @ref setHoldTimeLimits is called before @c Matter.begin().
   *
   * @param _occupancyState Initial occupancy state (true = occupied, false = unoccupied, default false).
   * @param _occupancySensorType Sensor type (default PIR).
   * @return true if successful, false otherwise.
   */
  bool begin(bool _occupancyState = false, OccupancySensorType_t _occupancySensorType = OCCUPANCY_SENSOR_TYPE_PIR);
  /**
   * @brief Stop processing Matter occupancy sensor events.
   */
  void end();

  /**
   * @brief Set the occupancy state (last raw reading from the sketch).
   *
   * Duplicate calls are ignored (CHIP SetOccupancy(false) restarts HoldTime).
   * With HoldTime enabled, @c setOccupancy(false) starts the cluster hold timer
   * instead of going vacant immediately. @ref getOccupancy is the last request;
   * @ref isOccupied is the Occupancy attribute the hub reads.
   *
   * @param _occupancyState Occupancy state (true = occupied, false = unoccupied).
   * @return true if successful, false otherwise.
   */
  bool setOccupancy(bool _occupancyState);
  /**
   * @brief Last occupancy value requested by the sketch, not the held cluster state.
   * @return true if the sketch last requested occupied, false if vacant.
   */
  bool getOccupancy() {
    return occupancyState;
  }
  /**
   * @brief Occupancy attribute the hub reads, including HoldTime.
   * @return true if occupied (including HoldTime), false if vacant.
   */
  bool isOccupied();
  /**
   * @brief Sensor type passed to @ref begin.
   * @return Occupancy sensor type.
   */
  OccupancySensorType_t getOccupancySensorType() {
    return occupancySensorType;
  }

  /**
   * @brief Set HoldTime in seconds.
   *
   * Call after the sensor @c begin() and before @c Matter.begin() to create the
   * HoldTime attributes. After @c Matter.begin() it only updates a cluster that
   * already has HoldTime enabled. 0 is not allowed.
   *
   * @param _holdTime_seconds HoldTime in seconds (at least 1). When limits are
   *        configured (holdTimeMax > 0), the value must fall in that range.
   * @return true if successful, false otherwise.
   */
  bool setHoldTime(uint16_t _holdTime_seconds);
  /**
   * @brief Get the current HoldTime value in seconds.
   * @return HoldTime in seconds.
   */
  uint16_t getHoldTime() {
    return holdTime_seconds;
  }

  /**
   * @brief Set HoldTime limits (minimum, maximum, and default) in seconds.
   *
   * Call after the sensor @c begin() and before @c Matter.begin() to enable
   * HoldTime. After start it only updates a cluster that already has HoldTime.
   * CHIP coerces min below 1 up to 1 and max below 10 up to at least 10.
   * @a _holdTimeDefault_seconds is informational metadata for controllers; it
   * does not set HoldTime — use @ref setHoldTime for the live value.
   * If the current HoldTime is outside the new limits, it is clamped.
   *
   * @param _holdTimeMin_seconds Minimum HoldTime in seconds.
   * @param _holdTimeMax_seconds Maximum HoldTime in seconds.
   * @param _holdTimeDefault_seconds Recommended default (informational).
   * @return true if successful, false otherwise.
   */
  bool setHoldTimeLimits(uint16_t _holdTimeMin_seconds, uint16_t _holdTimeMax_seconds, uint16_t _holdTimeDefault_seconds);

  /**
   * @brief Assignment — set occupancy state (same as @ref setOccupancy).
   * @param _occupancyState true = occupied, false = unoccupied.
   */
  void operator=(bool _occupancyState) {
    setOccupancy(_occupancyState);
  }
  /**
   * @brief Boolean conversion — last occupancy value requested by the sketch.
   * @return true if occupied was last requested.
   */
  operator bool() {
    return getOccupancy();
  }

  /**
   * @brief Callback invoked when HoldTime is changed by a Matter controller.
   * @param holdTime_seconds New HoldTime in seconds.
   * @return true to accept the change, false to reject it.
   */
  using HoldTimeChangeCB = std::function<bool(uint16_t holdTime_seconds)>;

  /**
   * @brief Register a callback for HoldTime changes from a Matter controller.
   * @param onHoldTimeChangeCB Callback of type @ref HoldTimeChangeCB.
   */
  void onHoldTimeChange(HoldTimeChangeCB onHoldTimeChangeCB);

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
  // bitmap for Occupancy Sensor Types
  static const uint8_t occupancySensorTypePir = 0x01;
  static const uint8_t occupancySensorTypeUltrasonic = 0x02;
  static const uint8_t occupancySensorTypePhysicalContact = 0x04;

  // bitmap for Occupancy Sensor Type Bitmap mapped array
  static const uint8_t occupancySensorTypeBitmap[4];

  bool started = false;
  bool occupancyState = false;
  OccupancySensorType_t occupancySensorType = OCCUPANCY_SENSOR_TYPE_PIR;
  uint16_t holdTime_seconds = 0;

  // HoldTimeLimits settings (set via setHoldTimeLimits() before Matter.begin())
  uint16_t holdTimeMin_seconds = 0;
  uint16_t holdTimeMax_seconds = 0;  // 0 means no maximum, no limits enforced
  uint16_t holdTimeDefault_seconds = 0;

  // User callback
  HoldTimeChangeCB _onHoldTimeChangeCB = nullptr;

  bool ensureHoldTimeAttributes();
  void onStackStarted() override;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
