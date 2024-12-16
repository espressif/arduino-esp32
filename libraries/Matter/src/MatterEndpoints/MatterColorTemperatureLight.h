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

class MatterColorTemperatureLight : public MatterEndPoint {
public:
  static const uint8_t MAX_BRIGHTNESS = 255;
  static const uint16_t MAX_COLOR_TEMPERATURE = 500;
  static const uint16_t MIN_COLOR_TEMPERATURE = 100;

  MatterColorTemperatureLight();
  ~MatterColorTemperatureLight();
  // default initial state is off, brightness is 64 (25%) and temperature is 370 (Soft White)
  virtual bool begin(bool initialState = false, uint8_t brightness = 64, uint16_t colorTemperature = 370);
  // this will just stop processing Light Matter events
  void end();

  bool setOnOff(bool newState);  // returns true if successful
  bool getOnOff();               // returns current light state
  bool toggle();                 // returns true if successful

  bool setBrightness(uint8_t newBrightness);  // returns true if successful
  uint8_t getBrightness();                    // returns current brightness

  bool setColorTemperature(uint16_t newTemperature);  // returns true if successful
  uint16_t getColorTemperature();                     // returns current temperature

  // User Callback for whenever the Light On/Off state is changed by the Matter Controller
  using EndPointOnOffCB = std::function<bool(bool)>;
  void onChangeOnOff(EndPointOnOffCB onChangeCB) {
    _onChangeOnOffCB = onChangeCB;
  }

  // User Callback for whenever the Light brightness value [0..255] is changed by the Matter Controller
  using EndPointBrightnessCB = std::function<bool(uint8_t)>;
  void onChangeBrightness(EndPointBrightnessCB onChangeCB) {
    _onChangeBrightnessCB = onChangeCB;
  }

  // User Callbqck for whenever the Light temperature value is changed by the Matter Controller
  using EndPointTemperatureCB = std::function<bool(uint16_t)>;
  void onChangeColorTemperature(EndPointTemperatureCB onChangeCB) {
    _onChangeTemperatureCB = onChangeCB;
  }

  // User Callback for whenever any parameter is changed by the Matter Controller
  using EndPointCB = std::function<bool(bool, uint8_t, uint16_t)>;
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  // used to update the state of the light using the current Matter Light internal state
  // It is necessary to set a user callback function using onChange() to handle the physical light state
  void updateAccessory();

  operator bool();             // returns current on/off light state
  void operator=(bool state);  // turns light on or off

  // this function is called by Matter internal event processor. It could be overwritten by the application, if necessary.
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  bool onOffState = false;             // default initial state is off, but it can be changed by begin(bool)
  uint8_t brightnessLevel = 0;         // default initial brightness is 0, but it can be changed by begin(bool, uint8_t)
  uint16_t colorTemperatureLevel = 0;  // default initial color temperature is 0, but it can be changed by begin(bool, uint8_t, uint16_t)
  EndPointOnOffCB _onChangeOnOffCB = NULL;
  EndPointBrightnessCB _onChangeBrightnessCB = NULL;
  EndPointTemperatureCB _onChangeTemperatureCB = NULL;
  EndPointCB _onChangeCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
