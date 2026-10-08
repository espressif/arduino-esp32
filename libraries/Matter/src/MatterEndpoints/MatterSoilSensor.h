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
// Soil Measurement is a code-driven CHIP cluster (like Boolean State or Valve Configuration and Control).
// MatterSoilSensor uses esp_matter::endpoint::soil_sensor::create() and ESP-Matter's soil_measurement
// integration (SetSoilMoistureLimits / SetSoilMoistureMeasuredValue). SoilMoistureMeasuredValue is not
// served from the Ember attribute store, so use setSoilMoisture() rather than getAttributeVal() /
// updateAttributeVal().
//
// The Soil Measurement cluster only defines a whole-percent (0-100) measured value - there is no
// sub-percent precision to request, unlike e.g. MatterHumiditySensor's 1/100th of a percent.
class MatterSoilSensor : public MatterEndPoint {
public:
  MatterSoilSensor();
  ~MatterSoilSensor();
  // begin Matter Soil Sensor endpoint. The live SoilMeasurementCluster is registered when the Matter
  // stack starts; call setSoilMoisture() before or after Matter.begin(). Values set before the stack
  // starts are cached and applied in onStackStarted() after the first successful setSoilMoisture() call.
  bool begin();
  // Same as begin(), then seeds the moisture cache (and pending report) like setSoilMoisture(initialPercent).
  bool begin(uint8_t initialSoilMoisturePercent);
  // this will just stop processing Soil Sensor Matter events
  void end();

  // set the soil moisture percent [0..100]
  bool setSoilMoisture(uint8_t soilMoisturePercent);
  // returns the last cached soil moisture percent [0..100] (last successful setSoilMoisture() or pending cache)
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
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) override;

protected:
  void onStackStarted() override;

  bool createSoilSensorEndpoint();
  void destroySoilSensorEndpoint(endpoint_t *endpoint);

  bool started = false;
  bool hasPendingMoistureReport = false;
  uint8_t soilMoisture = 0;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
