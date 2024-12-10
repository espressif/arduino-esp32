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

class MatterHumiditySensor : public MatterEndPoint {
public:
  MatterHumiditySensor();
  ~MatterHumiditySensor();
  // begin Matter Humidity Sensor endpoint with initial float humidity percent
  bool begin(double humidityPercent = 0.00) {
    if (humidityPercent < 0.0 || humidityPercent > 100.0) {
      log_e("Humidity Sensor Percentage value out of range [0..100].");
      return false;
    }
    return begin(static_cast<uint16_t>(humidityPercent * 100.0f));
  }
  // this will just stop processing Humidity Sensor Matter events
  void end();

  // set the humidity percent with 1/100th of a percent precision
  bool setHumidity(double humidityPercent) {
    if (humidityPercent < 0.0 || humidityPercent > 100.0) {
      log_e("Humidity Sensor Percentage value out of range [0..100].");
      return false;
    }
    return setRawHumidity(static_cast<uint16_t>(humidityPercent * 100.0f));
  }
  // returns the reported float humidity percent with 1/100th of precision
  double getHumidity() {
    return (double)rawHumidity / 100.0;
  }
  // double conversion operator
  void operator=(double humidityPercent) {
    setHumidity(humidityPercent);
  }
  // double conversion operator
  operator double() {
    return (double)getHumidity();
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  // implementation keeps humidity relative percentage with 1/100th of a percent precision
  uint16_t rawHumidity = 0;
  // internal function to set the raw humidity value (Matter Cluster)
  bool begin(uint16_t _rawHumidity);
  bool setRawHumidity(uint16_t _rawHumidity);
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
