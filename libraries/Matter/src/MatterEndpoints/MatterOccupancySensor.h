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

// Forward declaration for friend class
class OccupancySensingAttrAccessWrapper;

class MatterOccupancySensor : public MatterEndPoint {
  friend class OccupancySensingAttrAccessWrapper;
public:
  // Different Occupancy Sensor Types
  enum OccupancySensorType_t {
    OCCUPANCY_SENSOR_TYPE_PIR = (uint8_t)OccupancySensorTypeEnum::kPir,
    OCCUPANCY_SENSOR_TYPE_ULTRASONIC = (uint8_t)OccupancySensorTypeEnum::kUltrasonic,
    OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC = (uint8_t)OccupancySensorTypeEnum::kPIRAndUltrasonic,
    OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT = (uint8_t)OccupancySensorTypeEnum::kPhysicalContact
  };

  // Constructor
  MatterOccupancySensor();
  ~MatterOccupancySensor();
  // begin Matter Occupancy Sensor endpoint with initial occupancy state and default PIR sensor type
  // Note: Call setHoldTimeLimits() after Matter.begin() to configure HoldTimeLimits (optional)
  bool begin(bool _occupancyState = false, OccupancySensorType_t _occupancySensorType = OCCUPANCY_SENSOR_TYPE_PIR);
  // this will just stop processing Occupancy Sensor Matter events
  void end();

  // set the occupancy state
  bool setOccupancy(bool _occupancyState);
  // returns the occupancy state
  bool getOccupancy() {
    return occupancyState;
  }

  // set the hold time (in seconds)
  // Must be called after Matter.begin() has been called (requires Matter event loop to be running)
  bool setHoldTime(uint16_t _holdTime_seconds);
  // returns the hold time (in seconds)
  uint16_t getHoldTime() {
    return holdTime_seconds;
  }

  // set the hold time limits (min, max, default in seconds)
  // Must be called after Matter.begin() has been called (requires Matter event loop to be running)
  // Note: holdTimeDefault_seconds is informational metadata for Matter controllers (recommended default value).
  //       It does NOT automatically set the HoldTime attribute - use setHoldTime() to set the actual value.
  bool setHoldTimeLimits(uint16_t _holdTimeMin_seconds, uint16_t _holdTimeMax_seconds, uint16_t _holdTimeDefault_seconds);

  // bool conversion operator
  void operator=(bool _occupancyState) {
    setOccupancy(_occupancyState);
  }
  // bool conversion operator
  operator bool() {
    return getOccupancy();
  }

  // User callback for HoldTime attribute changes
  using HoldTimeChangeCB = std::function<bool(uint16_t holdTime_seconds)>;
  
  // Set callback for HoldTime changes (called when Matter Controller changes HoldTime)
  void onHoldTimeChange(HoldTimeChangeCB onHoldTimeChangeCB);

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
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
  uint16_t holdTime_seconds = 0;
  
  // HoldTimeLimits settings (set via setHoldTimeLimits() after Matter.begin())
  uint16_t holdTimeMin_seconds = 0;
  uint16_t holdTimeMax_seconds = 0;       // 0 means no maximum, no limits enforced
  uint16_t holdTimeDefault_seconds = 0;
  
  // User callback
  HoldTimeChangeCB _onHoldTimeChangeCB = nullptr;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
