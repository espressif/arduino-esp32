// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
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

using namespace chip::app::Clusters::OccupancySensing;

class MatterOccupancySensor : public MatterEndPoint {
public:
  // Different Occupancy Sensor Types
  enum OccupancySensorType_t {
    OCCUPANCY_SENSOR_TYPE_PIR = (uint8_t)OccupancySensorTypeEnum::kPir,
    OCCUPANCY_SENSOR_TYPE_ULTRASONIC = (uint8_t)OccupancySensorTypeEnum::kUltrasonic,
    OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC = (uint8_t)OccupancySensorTypeEnum::kPIRAndUltrasonic,
    OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT = (uint8_t)OccupancySensorTypeEnum::kPhysicalContact
  };

  MatterOccupancySensor();
  ~MatterOccupancySensor();
  // begin Matter Occupancy Sensor endpoint with initial occupancy state and default PIR sensor type
  bool begin(bool _occupancyState = false, OccupancySensorType_t _occupancySensorType = OCCUPANCY_SENSOR_TYPE_PIR);
  // this will just stop processing Occupancy Sensor Matter events
  void end();

  // set the occupancy state
  bool setOccupancy(bool _occupancyState);
  // returns the occupancy state
  bool getOccupancy() {
    return occupancyState;
  }

  // bool conversion operator
  void operator=(bool _occupancyState) {
    setOccupancy(_occupancyState);
  }
  // bool conversion operator
  operator bool() {
    return getOccupancy();
  }

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
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
