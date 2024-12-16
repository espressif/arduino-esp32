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

class MatterPressureSensor : public MatterEndPoint {
public:
  MatterPressureSensor();
  ~MatterPressureSensor();
  // begin Matter Pressure Sensor endpoint with initial float pressure
  bool begin(double pressure = 0.00) {
    return begin(static_cast<int16_t>(pressure));
  }
  // this will stop processing Pressure Sensor Matter events
  void end();

  // set the reported raw pressure in hPa
  bool setPressure(double pressure) {
    int16_t rawValue = static_cast<int16_t>(pressure);
    return setRawPressure(rawValue);
  }
  // returns the reported float pressure in hPa
  double getPressure() {
    return (double)rawPressure;
  }

  // double conversion operator
  void operator=(double pressure) {
    setPressure(pressure);
  }
  // double conversion operator
  operator double() {
    return (double)getPressure();
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  // implementation keeps pressure in hPa
  int16_t rawPressure = 0;
  // internal function to set the raw pressure value (Matter Cluster)
  bool setRawPressure(int16_t _rawPressure);
  bool begin(int16_t _rawPressure);
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
