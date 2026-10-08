// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/*
 * Advanced example: user-defined custom Matter endpoints (not wrapped in-tree).
 *
 * Creates a Matter node with a PM2.5 air-quality sensor built from a MatterEndPoint
 * subclass (Air Quality Sensor device type plus PM2.5 concentration measurement cluster).
 * Uses ensureMatterNode(), registerCreatedEndpoint(), updateAttributeVal(), and onStackStarted().
 *
 * PM2.5 readings are simulated in loop() after Matter.begin(). Long-press the boot button
 * for 5 seconds to decommission. See libraries/Matter/README.md — "Custom endpoints".
 */

// Matter Manager
#include <Arduino.h>
#include <Matter.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace esp_matter::cluster;
using namespace esp_matter::cluster::concentration_measurement::feature;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::detail;

class MatterAirQualityPm25Sensor : public MatterEndPoint {
public:
  MatterAirQualityPm25Sensor() = default;

  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val) override {
    if (!started) {
      log_e("Air Quality PM2.5 sensor has not begun.");
      return false;
    }
    log_d("Attr update: endpoint %u, cluster 0x%08" PRIX32 ", attribute 0x%08" PRIX32, endpoint_id, cluster_id, attribute_id);
    return true;
  }

  bool begin(float initialPm25Ugm3) {
    if (!ensureMatterNode()) {
      return false;
    }

    if (getEndPointId() != 0) {
      log_e("Air Quality PM2.5 sensor endpoint already created (id %u).", getEndPointId());
      return false;
    }

    air_quality_sensor::config_t air_quality_config;

    pm25_concentration_measurement::config_t pm25_config;
    pm25_config.measurement_medium = chip::to_underlying(MeasurementMediumEnum::kAir);
    pm25_config.feature_flags = numeric_measurement::get_id();
    pm25_config.features.numeric_measurement.measured_value = nullable<float>(initialPm25Ugm3);
    pm25_config.features.numeric_measurement.min_measured_value = nullable<float>();
    pm25_config.features.numeric_measurement.max_measured_value = nullable<float>();
    pm25_config.features.numeric_measurement.measurement_unit = chip::to_underlying(MeasurementUnitEnum::kUgm3);

    endpoint_t *endpoint = air_quality_sensor::create(node::get(), &air_quality_config, ENDPOINT_FLAG_NONE, (void *)this);
    if (endpoint == nullptr) {
      log_e("Failed to create Air Quality Sensor endpoint");
      return false;
    }

    if (pm25_concentration_measurement::create(endpoint, &pm25_config, CLUSTER_FLAG_SERVER) == nullptr) {
      log_e("Failed to add PM2.5 concentration measurement cluster");
      return false;
    }

    if (!registerCreatedEndpoint(endpoint)) {
      return false;
    }

    pm25Ugm3 = initialPm25Ugm3;
    started = true;
    log_i("Air Quality PM2.5 sensor created on endpoint %u", getEndPointId());
    return true;
  }

  float getPm25() const {
    return pm25Ugm3;
  }

  bool setPm25(float ugm3) {
    if (!started) {
      log_e("Air Quality PM2.5 sensor has not begun.");
      return false;
    }

    if (pm25Ugm3 == ugm3) {
      return true;
    }

    esp_matter_attr_val_t val = esp_matter_nullable_float(nullable<float>(ugm3));
    lock::ScopedChipStackLock stackLock(portMAX_DELAY);
    if (!updateAttributeVal(Pm25ConcentrationMeasurement::Id, Pm25ConcentrationMeasurement::Attributes::MeasuredValue::Id, &val)) {
      log_e("Failed to update PM2.5 measured value.");
      return false;
    }
    pm25Ugm3 = ugm3;
    log_v("PM2.5 set to %.1f ug/m3", ugm3);
    return true;
  }

protected:
  void onStackStarted() override {
    esp_matter_attr_val_t val = esp_matter_nullable_float(nullable<float>(pm25Ugm3));
    lock::ScopedChipStackLock stackLock(portMAX_DELAY);
    if (!updateAttributeVal(Pm25ConcentrationMeasurement::Id, Pm25ConcentrationMeasurement::Attributes::MeasuredValue::Id, &val)) {
      log_e("Failed to apply cached PM2.5 value after Matter.begin().");
    }
  }

private:
  bool started = false;
  float pm25Ugm3 = 0.0f;
};

// Custom Matter endpoint for this node
MatterAirQualityPm25Sensor pm25Sensor;

// Wi-Fi credentials for this sketch. Fill these in when the board cannot
// commission over BLE (Arduino prebuild on ESP32 / ESP32-S2): the sketch
// joins the AP itself. When Matter commissions over BLE (CHIPoBLE), the
// hub sends SSID and password — leave the placeholders; they are unused.
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"

// set your board USER BUTTON pin here - decommissioning button
const uint8_t buttonPin = BOOT_PIN;  // Set your pin here. Using BOOT Button.
MatterButton button;

static float getSimulatedPm25() {
  static float value = 12.0f;
  value += 5.0f;
  if (value > 80.0f) {
    value = 12.0f;
  }
  return value;
}

void setup() {
  // Initialize the USER BUTTON (Boot button) that will be used to decommission the Matter Node
  button.begin(buttonPin);

  Serial.begin(115200);

// CONFIG_ENABLE_CHIPOBLE=n: sketch starts Wi-Fi here; with CHIPoBLE the hub delivers credentials.
#if !CONFIG_ENABLE_CHIPOBLE
  matterConnectWiFi(WIFI_SSID, WIFI_PASSWORD);
#endif

  if (!pm25Sensor.begin(25.0f)) {
    log_e("Failed to create custom PM2.5 endpoint.");
    return;
  }

  // Matter beginning - Last step, after all EndPoints are initialized
  Matter.begin();
  matterWaitUntilReady();
}

void loop() {
  matterRestartIfNoFabric();

  static uint32_t timeCounter = 0;

  // Print and report PM2.5 every 5s
  if (!(timeCounter++ % 10)) {  // delaying for 500ms x 10 = 5s
    const float pm25 = getSimulatedPm25();
    if (pm25Sensor.setPm25(pm25)) {
      Serial.printf("PM2.5 concentration: %.1f ug/m3 (endpoint %u)\r\n", pm25Sensor.getPm25(), pm25Sensor.getEndPointId());
    }
  }

  matterButtonEvent_t ev;
  while ((ev = button.poll()) != MATTER_BUTTON_NONE) {
    if (ev == MATTER_BUTTON_LONG_HOLD) {
      Serial.println("Decommissioning Matter Node. It shall be commissioned again.");
      Matter.decommission();
    }
  }

  delay(500);
}
