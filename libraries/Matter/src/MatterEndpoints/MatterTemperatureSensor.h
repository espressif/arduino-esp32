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

class MatterTemperatureSensor : public MatterEndPoint {
public:
  MatterTemperatureSensor();
  ~MatterTemperatureSensor();
  // begin Matter Temperature Sensor endpoint
  virtual bool begin(int16_t _rawTemperature = 0);
  bool begin(double temperatureCelcius) {
    int16_t rawValue = static_cast<int16_t>(temperatureCelcius * 100.0f);
    return begin(rawValue);
  }
  // this will stop processing Temperature Sensor Matter events
  void end();

  // set the reported raw temperature
  bool setRawTemperature(int16_t _rawTemperature);
  bool setTemperatureCelsius(double temperatureCelcius) {
    int16_t rawValue = static_cast<int16_t>(temperatureCelcius * 100.0f);
    return setRawTemperature(rawValue);
  }
  // returns the reported raw temperature (in 1/100th of a degree)
  int16_t getRawTemperature() {
    return rawTemperature;
  }
  // returns the reported temperature in Celcius
  double getTemperatureCelsius() {
    return (double)rawTemperature / 100.0;
  }
  // sets the reported temperature in Celcius
  void operator=(double temperatureCelcius) {
    int16_t rawValue = static_cast<int16_t>(temperatureCelcius * 100.0f);
    setRawTemperature(rawValue);
  }
  operator double() {
    return (double) getTemperatureCelsius();
  }

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  int16_t rawTemperature = 0;  // default initial reported temperature
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
