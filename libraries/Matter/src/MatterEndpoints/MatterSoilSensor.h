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

// Matter Soil Sensor endpoint (device type 0x0045) - Soil Measurement cluster (0x0430).
//
// Soil Measurement has no ember-native implementation: it only exists as a code-driven cluster (the
// same family as BooleanState/ValveConfigurationAndControl), and esp_matter does not (yet) ship a
// esp_matter::endpoint::soil_sensor / esp_matter::cluster::soil_measurement convenience wrapper for it.
// MatterSoilSensor therefore builds the endpoint from the generic low-level endpoint/cluster API and
// registers the live chip::app::Clusters::SoilMeasurementCluster itself (see MatterSoilSensor.cpp),
// the same technique esp_matter's own boolean_state_integration.cpp uses for BooleanState.
//
// SoilMoistureMeasuredValue is managed by that cluster implementation, not by the ember attribute
// store, so it cannot be read/written through getAttributeVal()/setAttributeVal()/updateAttributeVal().
// setSoilMoisture() drives it directly through SoilMeasurementCluster::SetSoilMoistureMeasuredValue().
//
// The Soil Measurement cluster only defines a whole-percent (0-100) measured value - there is no
// sub-percent precision to request, unlike e.g. MatterHumiditySensor's 1/100th of a percent.
class MatterSoilSensor : public MatterEndPoint {
public:
  MatterSoilSensor();
  ~MatterSoilSensor();
  // begin Matter Soil Sensor endpoint. Like the Boolean State sensors, the live Soil Measurement
  // cluster instance is not available until the Matter stack starts, so begin() takes no initial
  // value - call setSoilMoisture() with the real sensor reading after Matter.begin().
  bool begin();
  // this will just stop processing Soil Sensor Matter events
  void end();

  // set the soil moisture percent [0..100]
  bool setSoilMoisture(uint8_t soilMoisturePercent);
  // returns the last reported soil moisture percent [0..100]
  uint8_t getSoilMoisture() {
    return soilMoisture;
  }
  // uint8_t conversion operator
  void operator=(uint8_t soilMoisturePercent) {
    setSoilMoisture(soilMoisturePercent);
  }
  // uint8_t conversion operator
  operator uint8_t() {
    return getSoilMoisture();
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  uint8_t soilMoisture = 0;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
