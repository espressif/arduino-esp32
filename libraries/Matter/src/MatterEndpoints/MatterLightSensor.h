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

class MatterLightSensor : public MatterEndPoint {
public:
  MatterLightSensor();
  ~MatterLightSensor();
  // begin Matter Light Sensor endpoint with initial float humidity percent
  bool begin(double illuminance = 1.00) {
    if (illuminance < 1.0 || illuminance > 3576000.0) {
      log_e("Light illuminance value out of range [1..3576000].");
      return false;
    }
    return begin(static_cast<uint16_t>(10000.0 * log10(illuminance) + 1));
  }
  // this will just stop processing Humidity Sensor Matter events
  void end();

  // set the illuminance
  bool setIlluminance(double illuminance) {
    if (illuminance < 1.0 || illuminance > 3576000.0) {
      log_e("Light illuminance value out of range [1..3576000].");
      return false;
    }
    return setRawIlluminance(static_cast<uint16_t>(10000.0 * log10(illuminance) + 1));
  }
  // returns the reported float illuminance value
  double getIlluminance() {
    return (double)pow(10, (rawIlluminance-1) / 10000.0);
  }
  // double conversion operator
  void operator=(double illuminance) {
    setIlluminance(illuminance);
  }
  // double conversion operator
  operator double() {
    return (double)getIlluminance();
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  // implementation keeps illuminance in 1/100th Lux x 100 (int16_t) normalized value
  uint16_t rawIlluminance = 0;
  // internal function to set the raw illuminance value (Matter Cluster)
  bool begin(uint16_t _rawIlluminance);
  bool setRawIlluminance(uint16_t _rawIlluminance);
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
