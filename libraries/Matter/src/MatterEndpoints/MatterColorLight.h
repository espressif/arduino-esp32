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

class MatterColorLight : public MatterEndPoint {
public:
  MatterColorLight();
  ~MatterColorLight();
  // default initial state is off, color is red 12% intensity HSV(0, 254, 31)
  virtual bool begin(bool initialState = false, espHsvColor_t colorHSV = {0, 254, 31});
  // this will just stop processing Light Matter events
  void end();

  bool setOnOff(bool newState);  // returns true if successful
  bool getOnOff();               // returns current light state
  bool toggle();                 // returns true if successful

  bool setColorRGB(espRgbColor_t rgbColor);  // returns true if successful
  espRgbColor_t getColorRGB();               // returns current RGB Color
  bool setColorHSV(espHsvColor_t hsvColor);  // returns true if successful
  espHsvColor_t getColorHSV();               // returns current HSV Color

  // User Callback for whenever the Light On/Off state is changed by the Matter Controller
  using EndPointOnOffCB = std::function<bool(bool)>;
  void onChangeOnOff(EndPointOnOffCB onChangeCB) {
    _onChangeOnOffCB = onChangeCB;
  }
  // User Callback for whenever the HSV Color value is changed by the Matter Controller
  using EndPointRGBColorCB = std::function<bool(espHsvColor_t)>;
  void onChangeColorHSV(EndPointRGBColorCB onChangeCB) {
    _onChangeColorCB = onChangeCB;
  }

  // User Callback for whenever any parameter is changed by the Matter Controller
  using EndPointCB = std::function<bool(bool, espHsvColor_t)>;
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
  espHsvColor_t colorHSV = {0, 0, 0};  // default initial color HSV is black, but it can be changed by begin(bool, espHsvColor_t)
  EndPointOnOffCB _onChangeOnOffCB = NULL;
  EndPointRGBColorCB _onChangeColorCB = NULL;
  EndPointCB _onChangeCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
